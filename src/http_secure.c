/**
 * FreeLang HTTP Secure-Pipeline 구현
 *
 * helmet 외부 의존성 없이 FreeLang 런타임에 내장된
 * 보안 헤더 엔진입니다.
 *
 * 설계 원칙:
 * - header_template 정적 상수로 메모리 할당 최소화
 * - 단일 snprintf 버퍼로 헤더 문자열 생성 (오버헤드 최소화)
 * - CSP 정책은 필드별 조합으로 유연성 확보
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/http_secure.h"
#include "../include/stdlib_fl.h"

/* ============================================================
   내부 헬퍼
   ============================================================ */

static fl_value_t hs_new_null(void) {
    fl_value_t v; v.type = FL_TYPE_NULL; return v;
}

static fl_value_t hs_new_string(const char* s) {
    fl_value_t v;
    v.type = FL_TYPE_STRING;
    if (s) {
        v.data.string_val = malloc(strlen(s) + 1);
        strcpy(v.data.string_val, s);
    } else {
        v.data.string_val = NULL;
    }
    return v;
}

static fl_object_t* hs_object_create(size_t cap) {
    fl_object_t* o = malloc(sizeof(fl_object_t));
    o->capacity = cap > 0 ? cap : 8;
    o->size = 0;
    o->keys   = malloc(o->capacity * sizeof(char*));
    o->values = malloc(o->capacity * sizeof(fl_value_t));
    return o;
}

static void hs_object_set(fl_object_t* obj, const char* key, const char* val) {
    if (obj->size >= obj->capacity) {
        obj->capacity *= 2;
        obj->keys   = realloc(obj->keys,   obj->capacity * sizeof(char*));
        obj->values = realloc(obj->values, obj->capacity * sizeof(fl_value_t));
    }
    obj->keys[obj->size] = malloc(strlen(key) + 1);
    strcpy(obj->keys[obj->size], key);
    obj->values[obj->size] = hs_new_string(val);
    obj->size++;
}

const char* fl_http_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

/* ============================================================
   보안 헤더 문자열을 버퍼에 쓰는 내부 함수
   CSP, HSTS는 파라미터로 커스터마이징 가능
   ============================================================ */
static int hs_write_secure_headers(char* buf, size_t buf_size,
                                    const char* csp_val,
                                    const char* hsts_val) {
    /* 기본값 설정 */
    const char* csp  = (csp_val  && csp_val[0])  ? csp_val  : "default-src 'self'";
    const char* hsts = (hsts_val && hsts_val[0])  ? hsts_val : "max-age=31536000; includeSubDomains";

    return snprintf(buf, buf_size,
        /* Clickjacking 방어 */
        "X-Frame-Options: SAMEORIGIN\r\n"
        /* MIME 스니핑 방어 */
        "X-Content-Type-Options: nosniff\r\n"
        /* 정보 유출 방어 */
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        /* 구형 브라우저 XSS 필터 */
        "X-XSS-Protection: 1; mode=block\r\n"
        /* XSS / 인젝션 방어 */
        "Content-Security-Policy: %s\r\n"
        /* HTTPS 강제 */
        "Strict-Transport-Security: %s\r\n"
        /* 브라우저 API 접근 제한 */
        "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n",
        csp, hsts
    );
}

/* ============================================================
   FreeLang stdlib API 구현
   ============================================================ */

/**
 * http_secure_headers() -> object
 * 7가지 보안 헤더를 object로 반환 (이름: 값 형식)
 */
fl_value_t fl_http_secure_headers(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;

    fl_object_t* obj = hs_object_create(8);

    hs_object_set(obj, "X-Frame-Options",        "SAMEORIGIN");
    hs_object_set(obj, "X-Content-Type-Options",  "nosniff");
    hs_object_set(obj, "Referrer-Policy",         "strict-origin-when-cross-origin");
    hs_object_set(obj, "X-XSS-Protection",        "1; mode=block");
    hs_object_set(obj, "Content-Security-Policy", "default-src 'self'");
    hs_object_set(obj, "Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    hs_object_set(obj, "Permissions-Policy",      "camera=(), microphone=(), geolocation=()");

    fl_value_t result;
    result.type = FL_TYPE_OBJECT;
    result.data.object_val = obj;
    return result;
}

/**
 * http_csp(default_src [, script_src [, style_src]]) -> string
 * Content-Security-Policy 헤더값 생성
 */
fl_value_t fl_http_csp(fl_value_t* args, size_t argc) {
    char buf[FL_CSP_POLICY_BUF] = {0};
    int  pos = 0;

    /* default-src */
    const char* default_src = (argc >= 1 && args[0].type == FL_TYPE_STRING)
                              ? args[0].data.string_val : "'self'";
    pos += snprintf(buf + pos, sizeof(buf) - pos, "default-src %s", default_src);

    /* script-src (선택) */
    if (argc >= 2 && args[1].type == FL_TYPE_STRING && args[1].data.string_val[0]) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "; script-src %s", args[1].data.string_val);
    }

    /* style-src (선택) */
    if (argc >= 3 && args[2].type == FL_TYPE_STRING && args[2].data.string_val[0]) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "; style-src %s", args[2].data.string_val);
    }

    return hs_new_string(buf);
}

/**
 * http_hsts(max_age) -> string
 * Strict-Transport-Security 헤더값 생성
 */
fl_value_t fl_http_hsts(fl_value_t* args, size_t argc) {
    long max_age = 31536000; /* 기본값: 1년 */

    if (argc >= 1 && args[0].type == FL_TYPE_INT) {
        max_age = (long)args[0].data.int_val;
        if (max_age < 0) max_age = 0;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "max-age=%ld; includeSubDomains", max_age);
    return hs_new_string(buf);
}

/**
 * http_response(status_code, body) -> string
 * 보안 헤더가 자동 삽입된 완전한 HTTP/1.1 응답 문자열
 */
fl_value_t fl_http_response(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        return hs_new_string("HTTP/1.1 500 Internal Server Error\r\n\r\n");
    }

    int status = 200;
    if (args[0].type == FL_TYPE_INT) {
        status = (int)args[0].data.int_val;
    }

    const char* body = "";
    if (args[1].type == FL_TYPE_STRING && args[1].data.string_val) {
        body = args[1].data.string_val;
    }

    size_t body_len = strlen(body);
    const char* status_text = fl_http_status_text(status);

    /* 보안 헤더 빌드 (정적 기본값) */
    char sec_headers[FL_SECURE_HEADER_BUF];
    hs_write_secure_headers(sec_headers, sizeof(sec_headers), NULL, NULL);

    /* 전체 응답 조립 */
    char* resp = malloc(FL_RESPONSE_BUF + body_len);
    if (!resp) return hs_new_null();

    snprintf(resp, FL_RESPONSE_BUF + body_len,
        "HTTP/1.1 %d %s\r\n"
        "%s"                          /* 7가지 보안 헤더 */
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status, status_text,
        sec_headers,
        body_len,
        body
    );

    fl_value_t result = hs_new_string(resp);
    free(resp);
    return result;
}

/**
 * http_response_json(status_code, json_body) -> string
 * Content-Type: application/json 보안 응답
 */
fl_value_t fl_http_response_json(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        return hs_new_string("HTTP/1.1 500 Internal Server Error\r\n\r\n");
    }

    int status = 200;
    if (args[0].type == FL_TYPE_INT) {
        status = (int)args[0].data.int_val;
    }

    const char* body = "{}";
    if (args[1].type == FL_TYPE_STRING && args[1].data.string_val) {
        body = args[1].data.string_val;
    }

    size_t body_len = strlen(body);
    const char* status_text = fl_http_status_text(status);

    char sec_headers[FL_SECURE_HEADER_BUF];
    hs_write_secure_headers(sec_headers, sizeof(sec_headers), NULL, NULL);

    char* resp = malloc(FL_RESPONSE_BUF + body_len);
    if (!resp) return hs_new_null();

    snprintf(resp, FL_RESPONSE_BUF + body_len,
        "HTTP/1.1 %d %s\r\n"
        "%s"
        "Content-Type: application/json; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status, status_text,
        sec_headers,
        body_len,
        body
    );

    fl_value_t result = hs_new_string(resp);
    free(resp);
    return result;
}
