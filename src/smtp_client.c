/**
 * FreeLang SMTP Client - MOSS-Mail-Core
 *
 * SMTP 상태 머신 구현 (nodemailer 완전 대체)
 * 외부 패키지: 0개 (OpenSSL은 시스템 라이브러리)
 *
 * 지원:
 *  - Plain SMTP (포트 25, 587)
 *  - STARTTLS (OpenSSL 3.x)
 *  - AUTH LOGIN (Base64)
 *  - MIME text/plain (UTF-8, base64 encoded body)
 *  - FSM: CONNECT→EHLO→[STARTTLS]→AUTH→MAIL FROM→RCPT TO→DATA→QUIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/* POSIX 소켓 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

/* OpenSSL STARTTLS */
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../include/smtp_client.h"
#include "../include/freelang.h"

/* ============================================================
   내부 헬퍼: fl_value_t 생성
   ============================================================ */

static fl_value_t smtp_str(const char *s) {
    fl_value_t v;
    v.type = FL_TYPE_STRING;
    if (s) {
        v.data.string_val = (char *)malloc(strlen(s) + 1);
        strcpy(v.data.string_val, s);
    } else {
        v.data.string_val = NULL;
    }
    return v;
}

static fl_value_t smtp_err(const char *msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "error: %s", msg);
    return smtp_str(buf);
}

/* ============================================================
   Base64 인코더 (순수 C, SIMD 힌트)
   RFC 4648 표준 알파벳 사용
   ============================================================ */

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode: src → dst(null-terminated)
 * dst는 충분한 크기 보장 필요: ceil(src_len/3)*4 + 1
 * 컴파일러 자동 벡터화 힌트: restrict 포인터
 */
static size_t base64_encode(const uint8_t * restrict src, size_t src_len,
                             char * restrict dst) {
    size_t i = 0, j = 0;

    /* 3바이트씩 처리 (컴파일러 SIMD 자동 벡터화 경로) */
    for (; i + 2 < src_len; i += 3) {
        uint32_t tri = ((uint32_t)src[i] << 16)
                     | ((uint32_t)src[i+1] << 8)
                     |  (uint32_t)src[i+2];
        dst[j++] = B64_TABLE[(tri >> 18) & 0x3F];
        dst[j++] = B64_TABLE[(tri >> 12) & 0x3F];
        dst[j++] = B64_TABLE[(tri >>  6) & 0x3F];
        dst[j++] = B64_TABLE[(tri      ) & 0x3F];
    }

    /* 나머지 1~2바이트 패딩 처리 */
    if (i < src_len) {
        uint32_t rem = (uint32_t)src[i] << 16;
        if (i + 1 < src_len) rem |= (uint32_t)src[i+1] << 8;
        dst[j++] = B64_TABLE[(rem >> 18) & 0x3F];
        dst[j++] = B64_TABLE[(rem >> 12) & 0x3F];
        dst[j++] = (i + 1 < src_len) ? B64_TABLE[(rem >> 6) & 0x3F] : '=';
        dst[j++] = '=';
    }

    dst[j] = '\0';
    return j;
}

/* 간편 래퍼: 문자열 → base64 문자열 (heap 할당) */
static char *b64_str(const char *input) {
    size_t len = strlen(input);
    size_t out_sz = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(out_sz);
    if (!out) return NULL;
    base64_encode((const uint8_t *)input, len, out);
    return out;
}

/* ============================================================
   TCP 소켓 연결
   ============================================================ */

static int tcp_connect(const char *host, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;

    int fd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* ============================================================
   소켓 I/O 래퍼 (Plain / TLS 공통 인터페이스)
   ============================================================ */

typedef struct {
    int       fd;     /* raw socket fd */
    SSL      *ssl;    /* NULL = plain, non-NULL = TLS */
} smtp_io_t;

static ssize_t smtp_write(smtp_io_t *io, const char *buf, size_t len) {
    if (io->ssl) return (ssize_t)SSL_write(io->ssl, buf, (int)len);
    return write(io->fd, buf, len);
}

static ssize_t smtp_read_line(smtp_io_t *io, char *buf, size_t buf_sz) {
    size_t i = 0;
    char c;
    while (i < buf_sz - 1) {
        ssize_t n;
        if (io->ssl) n = SSL_read(io->ssl, &c, 1);
        else         n = read(io->fd, &c, 1);
        if (n <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

/* SMTP 응답 코드 파싱 (멀티라인 250-EHLO 지원) */
static int smtp_read_response(smtp_io_t *io, char *last_line, size_t lsz) {
    char line[SMTP_LINE_BUF];
    int code = -1;
    while (1) {
        if (smtp_read_line(io, line, sizeof(line)) <= 0) return -1;
        int c = 0;
        sscanf(line, "%d", &c);
        if (c > 0) code = c;
        /* "250 " (공백) = 마지막 라인, "250-" = 계속 */
        if (strlen(line) >= 4 && line[3] == ' ') {
            if (last_line) {
                snprintf(last_line, lsz, "%s", line);
                /* 개행 제거 */
                size_t ll = strlen(last_line);
                if (ll > 0 && last_line[ll-1] == '\n') last_line[--ll] = '\0';
                if (ll > 0 && last_line[ll-1] == '\r') last_line[--ll] = '\0';
            }
            break;
        }
        /* 코드만 있으면 (5자 미만) 마지막으로 처리 */
        if (strlen(line) < 4) break;
    }
    return code;
}

/* SMTP 커맨드 전송 + 응답 수신 */
static int smtp_cmd(smtp_io_t *io, const char *cmd,
                    char *resp_buf, size_t resp_sz) {
    smtp_write(io, cmd, strlen(cmd));
    return smtp_read_response(io, resp_buf, resp_sz);
}

/* ============================================================
   STARTTLS 업그레이드
   ============================================================ */

static SSL_CTX *g_ssl_ctx = NULL;

static void smtp_ssl_init(void) {
    if (g_ssl_ctx) return;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    g_ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (g_ssl_ctx) {
        SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_NONE, NULL);
    }
}

static SSL *smtp_upgrade_tls(int fd) {
    smtp_ssl_init();
    if (!g_ssl_ctx) return NULL;

    SSL *ssl = SSL_new(g_ssl_ctx);
    if (!ssl) return NULL;

    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        return NULL;
    }
    return ssl;
}

/* ============================================================
   핵심: SMTP 세션 실행기
   use_starttls = 0 → plain / 1 → STARTTLS
   ============================================================ */

typedef struct {
    const char *host;
    int         port;
    const char *user;
    const char *pass;
    const char *from;
    const char *to;
    const char *subject;
    const char *body;
    int         use_starttls;
} smtp_params_t;

/* 결과 문자열 반환 ("ok" 또는 "error: ...") */
static char *smtp_run_session(const smtp_params_t *p) {
    char resp[SMTP_LINE_BUF];
    char errbuf[256];
    smtp_io_t io;
    io.fd  = -1;
    io.ssl = NULL;

    /* 1. TCP 연결 */
    io.fd = tcp_connect(p->host, p->port);
    if (io.fd < 0) {
        snprintf(errbuf, sizeof(errbuf), "connect failed: %s", strerror(errno));
        return strdup(errbuf);
    }

    /* 2. 220 greeting */
    int code = smtp_read_response(&io, resp, sizeof(resp));
    if (code != SMTP_CODE_READY) {
        snprintf(errbuf, sizeof(errbuf), "greeting failed: code=%d", code);
        goto fail_plain;
    }

    /* 3. EHLO */
    code = smtp_cmd(&io, "EHLO freelang-mail-core\r\n", resp, sizeof(resp));
    if (code != SMTP_CODE_OK) {
        snprintf(errbuf, sizeof(errbuf), "EHLO failed: code=%d", code);
        goto fail_plain;
    }

    /* 4. STARTTLS 업그레이드 (요청 시) */
    if (p->use_starttls) {
        code = smtp_cmd(&io, "STARTTLS\r\n", resp, sizeof(resp));
        if (code != SMTP_CODE_READY) {
            snprintf(errbuf, sizeof(errbuf), "STARTTLS rejected: code=%d", code);
            goto fail_plain;
        }
        io.ssl = smtp_upgrade_tls(io.fd);
        if (!io.ssl) {
            snprintf(errbuf, sizeof(errbuf), "TLS handshake failed");
            goto fail_plain;
        }
        /* TLS 터널 위에서 다시 EHLO */
        code = smtp_cmd(&io, "EHLO freelang-mail-core\r\n", resp, sizeof(resp));
        if (code != SMTP_CODE_OK) {
            snprintf(errbuf, sizeof(errbuf), "EHLO(TLS) failed: code=%d", code);
            goto fail_tls;
        }
    }

    /* 5. AUTH LOGIN */
    if (p->user && p->user[0]) {
        code = smtp_cmd(&io, "AUTH LOGIN\r\n", resp, sizeof(resp));
        if (code != SMTP_CODE_AUTH_CONT) {
            snprintf(errbuf, sizeof(errbuf), "AUTH LOGIN init failed: code=%d", code);
            goto fail_tls;
        }

        /* 사용자명 base64 */
        char *b64user = b64_str(p->user);
        if (!b64user) { snprintf(errbuf, sizeof(errbuf), "OOM b64user"); goto fail_tls; }
        char auth_cmd[512];
        snprintf(auth_cmd, sizeof(auth_cmd), "%s\r\n", b64user);
        free(b64user);

        code = smtp_cmd(&io, auth_cmd, resp, sizeof(resp));
        if (code != SMTP_CODE_AUTH_CONT) {
            snprintf(errbuf, sizeof(errbuf), "AUTH user failed: code=%d", code);
            goto fail_tls;
        }

        /* 비밀번호 base64 */
        char *b64pass = b64_str(p->pass);
        if (!b64pass) { snprintf(errbuf, sizeof(errbuf), "OOM b64pass"); goto fail_tls; }
        snprintf(auth_cmd, sizeof(auth_cmd), "%s\r\n", b64pass);
        free(b64pass);

        code = smtp_cmd(&io, auth_cmd, resp, sizeof(resp));
        if (code != SMTP_CODE_AUTH_OK && code != SMTP_CODE_OK) {
            snprintf(errbuf, sizeof(errbuf), "AUTH pass failed: code=%d", code);
            goto fail_tls;
        }
    }

    /* 6. MAIL FROM */
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "MAIL FROM:<%s>\r\n", p->from);
        code = smtp_cmd(&io, cmd, resp, sizeof(resp));
        if (code != SMTP_CODE_OK) {
            snprintf(errbuf, sizeof(errbuf), "MAIL FROM failed: code=%d", code);
            goto fail_tls;
        }
    }

    /* 7. RCPT TO */
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "RCPT TO:<%s>\r\n", p->to);
        code = smtp_cmd(&io, cmd, resp, sizeof(resp));
        if (code != SMTP_CODE_OK && code != 251) {
            snprintf(errbuf, sizeof(errbuf), "RCPT TO failed: code=%d", code);
            goto fail_tls;
        }
    }

    /* 8. DATA */
    code = smtp_cmd(&io, "DATA\r\n", resp, sizeof(resp));
    if (code != SMTP_CODE_DATA_START) {
        snprintf(errbuf, sizeof(errbuf), "DATA failed: code=%d", code);
        goto fail_tls;
    }

    /* 9. MIME 메시지 본문 조립 + 전송 */
    {
        /* body를 base64로 인코딩 */
        size_t body_len = strlen(p->body);
        size_t b64_sz   = ((body_len + 2) / 3) * 4 + 4;
        char  *b64body  = (char *)malloc(b64_sz);
        if (!b64body) {
            snprintf(errbuf, sizeof(errbuf), "OOM b64body");
            goto fail_tls;
        }
        base64_encode((const uint8_t *)p->body, body_len, b64body);

        /* MIME 헤더 */
        char hdrs[1024];
        int hlen = snprintf(hdrs, sizeof(hdrs),
            "From: <%s>\r\n"
            "To: <%s>\r\n"
            "Subject: %s\r\n"
            "MIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "X-Mailer: FreeLang-MOSS-Mail-Core/1.0\r\n"
            "\r\n",
            p->from, p->to, p->subject
        );

        /* 헤더 전송 */
        smtp_write(&io, hdrs, (size_t)hlen);

        /* base64 body: RFC 2045 - 76자마다 개행 */
        size_t b64len = strlen(b64body);
        size_t pos = 0;
        while (pos < b64len) {
            size_t chunk = (b64len - pos > 76) ? 76 : (b64len - pos);
            smtp_write(&io, b64body + pos, chunk);
            smtp_write(&io, "\r\n", 2);
            pos += chunk;
        }
        free(b64body);

        /* 메시지 종료 (단일 점) */
        smtp_write(&io, "\r\n.\r\n", 5);

        code = smtp_read_response(&io, resp, sizeof(resp));
        if (code != SMTP_CODE_OK) {
            snprintf(errbuf, sizeof(errbuf), "message rejected: code=%d %s", code, resp);
            goto fail_tls;
        }
    }

    /* 10. QUIT */
    smtp_cmd(&io, "QUIT\r\n", NULL, 0);

    if (io.ssl) { SSL_shutdown(io.ssl); SSL_free(io.ssl); }
    close(io.fd);
    return strdup("ok");

fail_tls:
    if (io.ssl) { SSL_shutdown(io.ssl); SSL_free(io.ssl); }
fail_plain:
    if (io.fd >= 0) close(io.fd);
    return strdup(errbuf);
}

/* ============================================================
   FreeLang 빌트인 함수 구현
   ============================================================ */

/* 공통 파라미터 추출 헬퍼 */
static int extract_smtp_args(fl_value_t *args, size_t argc, smtp_params_t *p) {
    if (argc < 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (args[i].type != FL_TYPE_STRING) return 0;
    }
    p->host    = args[0].data.string_val;
    p->port    = 587;  /* 기본값 */
    if (args[1].type == FL_TYPE_INT) {
        p->port = (int)args[1].data.int_val;
    } else if (args[1].type == FL_TYPE_STRING) {
        p->port = atoi(args[1].data.string_val);
    }
    p->user    = args[2].data.string_val;
    p->pass    = args[3].data.string_val;
    p->from    = args[4].data.string_val;
    p->to      = args[5].data.string_val;
    p->subject = args[6].data.string_val;
    p->body    = args[7].data.string_val;
    return 1;
}

/**
 * smtp_mail(host, port, user, pass, from, to, subject, body) -> string
 */
fl_value_t fl_smtp_mail(fl_value_t *args, size_t argc) {
    smtp_params_t p;
    memset(&p, 0, sizeof(p));

    if (argc < 8) return smtp_err("smtp_mail: 인자 8개 필요");

    p.host    = (args[0].type == FL_TYPE_STRING) ? args[0].data.string_val : "";
    p.port    = (args[1].type == FL_TYPE_INT)    ? (int)args[1].data.int_val :
                (args[1].type == FL_TYPE_STRING)  ? atoi(args[1].data.string_val) : 587;
    p.user    = (args[2].type == FL_TYPE_STRING) ? args[2].data.string_val : "";
    p.pass    = (args[3].type == FL_TYPE_STRING) ? args[3].data.string_val : "";
    p.from    = (args[4].type == FL_TYPE_STRING) ? args[4].data.string_val : "";
    p.to      = (args[5].type == FL_TYPE_STRING) ? args[5].data.string_val : "";
    p.subject = (args[6].type == FL_TYPE_STRING) ? args[6].data.string_val : "";
    p.body    = (args[7].type == FL_TYPE_STRING) ? args[7].data.string_val : "";
    p.use_starttls = 0;

    char *result = smtp_run_session(&p);
    fl_value_t ret = smtp_str(result);
    free(result);
    return ret;
}

/**
 * smtp_mail_tls(host, port, user, pass, from, to, subject, body) -> string
 */
fl_value_t fl_smtp_mail_tls(fl_value_t *args, size_t argc) {
    smtp_params_t p;
    memset(&p, 0, sizeof(p));

    if (argc < 8) return smtp_err("smtp_mail_tls: 인자 8개 필요");

    p.host    = (args[0].type == FL_TYPE_STRING) ? args[0].data.string_val : "";
    p.port    = (args[1].type == FL_TYPE_INT)    ? (int)args[1].data.int_val :
                (args[1].type == FL_TYPE_STRING)  ? atoi(args[1].data.string_val) : 587;
    p.user    = (args[2].type == FL_TYPE_STRING) ? args[2].data.string_val : "";
    p.pass    = (args[3].type == FL_TYPE_STRING) ? args[3].data.string_val : "";
    p.from    = (args[4].type == FL_TYPE_STRING) ? args[4].data.string_val : "";
    p.to      = (args[5].type == FL_TYPE_STRING) ? args[5].data.string_val : "";
    p.subject = (args[6].type == FL_TYPE_STRING) ? args[6].data.string_val : "";
    p.body    = (args[7].type == FL_TYPE_STRING) ? args[7].data.string_val : "";
    p.use_starttls = 1;

    char *result = smtp_run_session(&p);
    fl_value_t ret = smtp_str(result);
    free(result);
    return ret;
}

/**
 * smtp_mime_encode(text) -> string
 * base64 인코딩 테스트 유틸리티
 */
fl_value_t fl_smtp_mime_encode(fl_value_t *args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_STRING)
        return smtp_err("smtp_mime_encode: 문자열 인자 필요");

    const char *input = args[0].data.string_val;
    char *encoded = b64_str(input);
    if (!encoded) return smtp_err("OOM");

    fl_value_t ret = smtp_str(encoded);
    free(encoded);
    return ret;
}
