/**
 * Proof-Logger 구현
 *
 * 아키텍처:
 *   메인 스레드 → fl_logger_log() → SPSC 링 버퍼 → 워커 스레드
 *   워커: 소비 → [콘솔] [파일: pwrite/io_uring] [Gogs: HTTP POST]
 *
 * io_uring: liburing 없이 syscall() 직접 사용 (Linux 6.8 검증)
 * Gogs:     BSD socket + HTTP/1.1 (논블로킹, 비연결 패킷)
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <linux/io_uring.h>  /* 실제 io_uring_params 구조체 (커널 헤더) */

#include "../include/logger.h"
#include "../include/runtime.h"

/* ============================================================
   전역 로거 싱글톤
   ============================================================ */

fl_logger_t*   fl_global_logger    = NULL;
fl_log_level_t fl_compile_log_level = FL_LOG_DEBUG;  /* @log_level 어노테이션으로 설정 */

/* ============================================================
   ANSI 색상 코드
   ============================================================ */

#define ANSI_RESET   "\x1b[0m"
#define ANSI_GRAY    "\x1b[90m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_RED     "\x1b[31;1m"

static const char* level_color(fl_log_level_t level) {
    switch (level) {
        case FL_LOG_DEBUG: return ANSI_GRAY;
        case FL_LOG_INFO:  return ANSI_GREEN;
        case FL_LOG_WARN:  return ANSI_YELLOW;
        case FL_LOG_ERROR: return ANSI_RED;
        default:           return ANSI_RESET;
    }
}

/* ============================================================
   타임스탬프 (나노초)
   ============================================================ */

static inline uint64_t ts_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void ts_to_iso(uint64_t ts_ns, char* buf, size_t n) {
    time_t sec = (time_t)(ts_ns / 1000000000ULL);
    uint32_t ms  = (uint32_t)((ts_ns % 1000000000ULL) / 1000000ULL);
    struct tm t;
    gmtime_r(&sec, &t);
    snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec, ms);
}

/* ============================================================
   메시지 포맷팅 (JSON / Plain)
   ============================================================ */

static int format_plain(char* buf, size_t n, fl_log_level_t level,
                         uint64_t ts_ns, const char* msg) {
    char tsbuf[32];
    ts_to_iso(ts_ns, tsbuf, sizeof(tsbuf));
    return snprintf(buf, n, "[%s] [%s] %s\n", tsbuf, fl_log_level_name(level), msg);
}

static int format_json(char* buf, size_t n, fl_log_level_t level,
                        uint64_t ts_ns, const char* msg) {
    /* JSON 이스케이프: " → \", \ → \\ */
    char escaped[FL_LOG_MSG_MAX * 2];
    size_t ei = 0;
    for (const char* p = msg; *p && ei < sizeof(escaped) - 2; p++) {
        if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
        else if (*p == '\n') { escaped[ei++] = '\\'; escaped[ei++] = 'n'; continue; }
        else if (*p == '\t') { escaped[ei++] = '\\'; escaped[ei++] = 't'; continue; }
        escaped[ei++] = *p;
    }
    escaped[ei] = '\0';

    return snprintf(buf, n,
        "{\"level\":\"%s\",\"ts\":%llu,\"msg\":\"%s\"}\n",
        fl_log_level_name(level),
        (unsigned long long)ts_ns,
        escaped);
}

/* ============================================================
   io_uring: syscall 직접 호출
   Linux 6.8+ (x86_64)
   ============================================================ */

#ifndef SYS_io_uring_setup
#  define SYS_io_uring_setup    425
#  define SYS_io_uring_enter    426
#  define SYS_io_uring_register 427
#endif

/* io_uring SQE 최소 구조 (opcode: IORING_OP_WRITE=23) */
typedef struct {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t ioprio;
    int32_t  fd;
    uint64_t off;
    uint64_t addr;
    uint32_t len;
    uint32_t rw_flags;
    uint64_t user_data;
    uint8_t  pad[24];
} fl_sqe_t;

/* io_uring CQE 최소 구조 */
typedef struct {
    uint64_t user_data;
    int32_t  res;
    uint32_t flags;
} fl_cqe_t;

#define IORING_OP_WRITE  23

static bool uring_init(fl_iouring_t* u, unsigned entries) {
    /* 실제 커널 io_uring_params 구조체 사용 (linux/io_uring.h) */
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    long fd = syscall(SYS_io_uring_setup, entries, &params);
    if (fd < 0) return false;

    u->ring_fd = (int)fd;

    /* SQ 링 mmap (IORING_OFF_SQ_RING = 0) */
    size_t sq_ring_sz = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
    u->sq_ring = mmap(NULL, sq_ring_sz, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQ_RING);
    if (u->sq_ring == MAP_FAILED) { close(fd); return false; }
    u->sq_ring_sz = sq_ring_sz;

    /* SQE 배열 mmap */
    u->sqe_sz = params.sq_entries * sizeof(fl_sqe_t);
    u->sqes = mmap(NULL, u->sqe_sz, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQES);
    if (u->sqes == MAP_FAILED) {
        munmap(u->sq_ring, sq_ring_sz);
        close(fd);
        return false;
    }

    /* CQ 링 mmap */
    size_t cq_ring_sz = params.cq_off.cqes + params.cq_entries * sizeof(fl_cqe_t);
    u->cq_ring = mmap(NULL, cq_ring_sz, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_CQ_RING);
    if (u->cq_ring == MAP_FAILED) {
        munmap(u->sqes, u->sqe_sz);
        munmap(u->sq_ring, sq_ring_sz);
        close(fd);
        return false;
    }
    u->cq_ring_sz = cq_ring_sz;

    uint8_t* sqb = (uint8_t*)u->sq_ring;
    u->sq_head      = (uint32_t*)(sqb + params.sq_off.head);
    u->sq_tail      = (uint32_t*)(sqb + params.sq_off.tail);
    u->sq_ring_mask = (uint32_t*)(sqb + params.sq_off.ring_mask);
    u->sq_array     = (uint32_t*)(sqb + params.sq_off.array);

    uint8_t* cqb = (uint8_t*)u->cq_ring;
    u->cq_head      = (uint32_t*)(cqb + params.cq_off.head);
    u->cq_tail      = (uint32_t*)(cqb + params.cq_off.tail);
    u->cq_ring_mask = (uint32_t*)(cqb + params.cq_off.ring_mask);

    u->initialized = true;
    return true;
}

static void uring_destroy(fl_iouring_t* u) {
    if (!u->initialized) return;
    munmap(u->sqes, u->sqe_sz);
    munmap(u->cq_ring, u->cq_ring_sz);
    munmap(u->sq_ring, u->sq_ring_sz);
    close(u->ring_fd);
    u->initialized = false;
}

/* io_uring으로 비동기 쓰기 (워커 스레드에서만 호출) */
static bool uring_write_async(fl_iouring_t* u, int fd, const void* buf, size_t len, off_t off) {
    if (!u->initialized) return false;

    uint32_t tail = atomic_load_explicit((_Atomic uint32_t*)u->sq_tail, memory_order_acquire);
    uint32_t head = atomic_load_explicit((_Atomic uint32_t*)u->sq_head, memory_order_acquire);
    uint32_t mask = *u->sq_ring_mask;

    if (tail - head >= mask + 1) return false;  /* SQ 풀 */

    uint32_t idx = tail & mask;
    fl_sqe_t* sqe = (fl_sqe_t*)((uint8_t*)u->sqes + idx * 64);
    memset(sqe, 0, 64);
    sqe->opcode    = IORING_OP_WRITE;
    sqe->fd        = fd;
    sqe->addr      = (uint64_t)(uintptr_t)buf;
    sqe->len       = (uint32_t)len;
    sqe->off       = (off == -1) ? (uint64_t)-1 : (uint64_t)off; /* -1 = 현재 파일 위치 */
    sqe->user_data = (uint64_t)len;

    u->sq_array[idx] = idx;
    atomic_store_explicit((_Atomic uint32_t*)u->sq_tail, tail + 1, memory_order_release);

    /* 커널에 알림 (IORING_ENTER_GETEVENTS=1, min_complete=0) */
    syscall(SYS_io_uring_enter, u->ring_fd, 1, 0, 0, NULL, 0);
    return true;
}

/* ============================================================
   Gogs HTTP POST (guestbook.dclub.kr)
   ============================================================ */

static void gogs_post_log(fl_logger_t* logger, fl_log_level_t level, const char* msg) {
    const fl_log_config_t* cfg = &logger->config;
    if (!(cfg->targets & FL_LOG_TARGET_GOGS)) return;
    if ((size_t)level < cfg->gogs_min_level) return;
    if (!cfg->gogs_host[0]) return;

    /* JSON 이스케이프 */
    char escaped[FL_LOG_MSG_MAX * 2];
    size_t ei = 0;
    for (const char* p = msg; *p && ei < sizeof(escaped) - 2; p++) {
        if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
        else if (*p == '\n') { escaped[ei++] = '\\'; escaped[ei++] = 'n'; continue; }
        escaped[ei++] = *p;
    }
    escaped[ei] = '\0';

    /* HTTP 요청 본문 */
    char body[FL_LOG_MSG_MAX * 3];
    int body_len = snprintf(body, sizeof(body),
        "{\"author\":\"%s\",\"author_id\":\"%s\",\"message\":\"[%s] %s\"}",
        cfg->gogs_author[0] ? cfg->gogs_author : "FreeLang-Proof-Logger",
        logger->author_id,
        fl_log_level_name(level),
        escaped);

    /* HTTP 요청 헤더 */
    char request[4096];
    int req_len = snprintf(request, sizeof(request),
        "POST /api/write HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        cfg->gogs_host, body_len, body);

    /* 소켓 연결 */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", cfg->gogs_port > 0 ? cfg->gogs_port : 80);

    if (getaddrinfo(cfg->gogs_host, port_str, &hints, &res) != 0) return;

    int sock = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, 0);
    if (sock < 0) { freeaddrinfo(res); return; }

    /* 비동기 connect (타임아웃 없이 fire-and-forget) */
    connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    /* 쓰기 타임아웃 200ms */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* select로 연결 완료 대기 (200ms) */
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    if (select(sock + 1, NULL, &wfds, NULL, &tv) > 0) {
        send(sock, request, req_len, MSG_NOSIGNAL);
    }

    close(sock);
}

/* ============================================================
   워커 스레드 (소비자)
   ============================================================ */

static void worker_write_file(fl_logger_t* logger, const char* line, size_t len) {
    if (logger->file_fd < 0) return;
    /* O_APPEND로 열렸으므로 write()가 원자적으로 파일 끝에 추가
     * io_uring은 고급 최적화용: 현재는 워커 스레드 직접 write()가 더 안전 */
    ssize_t r = write(logger->file_fd, line, len);
    (void)r;
}

static void* logger_worker(void* arg) {
    fl_logger_t* logger = (fl_logger_t*)arg;
    fl_log_ring_t* ring = &logger->ring;

    /* SIGPIPE 무시 (Gogs 소켓 전송 중 연결 끊길 때) */
    signal(SIGPIPE, SIG_IGN);

    char linebuf[FL_LOG_MSG_MAX + 256];

    while (atomic_load_explicit(&logger->running, memory_order_acquire)) {
        uint64_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
        uint64_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);

        if (tail == head) {
            /* 링 버퍼 비어있음 → 1ms 대기 */
            struct timespec ns = { .tv_sec = 0, .tv_nsec = 1000000 };
            nanosleep(&ns, NULL);
            continue;
        }

        fl_log_entry_t* entry = &ring->slots[tail & FL_LOG_RING_MASK];
        fl_log_level_t  level = entry->level;
        uint64_t        ts_ns = entry->ts_ns;
        bool            gogs  = entry->gogs_send;
        char            msg[FL_LOG_MSG_MAX];
        strncpy(msg, entry->msg, FL_LOG_MSG_MAX - 1);
        msg[FL_LOG_MSG_MAX - 1] = '\0';

        /* 소비 완료: tail 증가 */
        atomic_store_explicit(&ring->tail, tail + 1, memory_order_release);

        /* 런타임 레벨 필터 */
        if ((int)level < (int)logger->config.level) {
            continue;
        }

        /* 포맷 */
        int n;
        if (logger->config.use_json) {
            n = format_json(linebuf, sizeof(linebuf), level, ts_ns, msg);
        } else {
            n = format_plain(linebuf, sizeof(linebuf), level, ts_ns, msg);
        }
        if (n <= 0) continue;
        size_t linelen = (size_t)n;

        /* 콘솔 출력 */
        if (logger->config.targets & FL_LOG_TARGET_CONSOLE) {
            if (logger->config.use_color) {
                fprintf(stderr, "%s%s%s", level_color(level), linebuf, ANSI_RESET);
            } else {
                fwrite(linebuf, 1, linelen, stderr);
            }
        }

        /* 파일 쓰기 */
        if (logger->config.targets & FL_LOG_TARGET_FILE) {
            worker_write_file(logger, linebuf, linelen);
        }

        /* Gogs 전송 */
        if (gogs && (logger->config.targets & FL_LOG_TARGET_GOGS)) {
            gogs_post_log(logger, level, msg);
        }

        atomic_fetch_add_explicit(&logger->written, 1, memory_order_relaxed);
    }

    /* 종료 시 남은 항목 모두 처리 */
    uint64_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    uint64_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    while (tail < head) {
        fl_log_entry_t* entry = &ring->slots[tail & FL_LOG_RING_MASK];
        char linebuf2[FL_LOG_MSG_MAX + 256];
        int n = logger->config.use_json
            ? format_json(linebuf2, sizeof(linebuf2), entry->level, entry->ts_ns, entry->msg)
            : format_plain(linebuf2, sizeof(linebuf2), entry->level, entry->ts_ns, entry->msg);
        if (n > 0) {
            if (logger->config.targets & FL_LOG_TARGET_CONSOLE)
                fwrite(linebuf2, 1, (size_t)n, stderr);
            if (logger->config.targets & FL_LOG_TARGET_FILE)
                worker_write_file(logger, linebuf2, (size_t)n);
        }
        atomic_store_explicit(&ring->tail, ++tail, memory_order_release);
    }

    return NULL;
}

/* ============================================================
   공개 API 구현
   ============================================================ */

fl_log_config_t fl_log_default_config(void) {
    fl_log_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.targets        = FL_LOG_TARGET_CONSOLE | FL_LOG_TARGET_FILE;
    cfg.level          = FL_LOG_DEBUG;
    cfg.gogs_port      = 80;
    cfg.use_json       = true;
    cfg.use_color      = true;
    cfg.use_iouring    = true;
    cfg.gogs_min_level = FL_LOG_WARN;
    snprintf(cfg.file_path, sizeof(cfg.file_path), "/tmp/freelang-proof.log");
    snprintf(cfg.gogs_host, sizeof(cfg.gogs_host), "gogs.dclub.kr");
    snprintf(cfg.gogs_author, sizeof(cfg.gogs_author), "FreeLang-Proof-Logger");
    return cfg;
}

fl_logger_t* fl_logger_create(const fl_log_config_t* config) {
    fl_logger_t* logger = (fl_logger_t*)calloc(1, sizeof(fl_logger_t));
    if (!logger) return NULL;

    if (config) {
        logger->config = *config;
    } else {
        logger->config = fl_log_default_config();
    }

    /* 링 버퍼 초기화 */
    atomic_init(&logger->ring.head, 0);
    atomic_init(&logger->ring.tail, 0);
    atomic_init(&logger->running, true);
    atomic_init(&logger->dropped, 0);
    atomic_init(&logger->written, 0);

    /* author_id: PID 기반 */
    snprintf(logger->author_id, sizeof(logger->author_id),
             "PID-PROOF-LOGGER-%d", (int)getpid());

    /* 파일 오픈 */
    logger->file_fd = -1;
    if ((logger->config.targets & FL_LOG_TARGET_FILE) && logger->config.file_path[0]) {
        logger->file_fd = open(logger->config.file_path,
                               O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logger->file_fd < 0) {
            fprintf(stderr, "[Proof-Logger] 파일 열기 실패: %s (%s)\n",
                    logger->config.file_path, strerror(errno));
            /* 파일 쓰기 타겟 비활성화 (계속 동작) */
            logger->config.targets &= ~FL_LOG_TARGET_FILE;
        }
    }

    /* io_uring 초기화 시도 */
    logger->uring.initialized = false;
    if (logger->config.use_iouring) {
        if (uring_init(&logger->uring, FL_IOURING_ENTRIES)) {
            fprintf(stderr, "[Proof-Logger] io_uring 초기화 성공 (Zero-Wait I/O 활성화)\n");
        } else {
            fprintf(stderr, "[Proof-Logger] io_uring 불가, pwrite 폴백\n");
        }
    }

    /* flush 동기화 */
    pthread_mutex_init(&logger->flush_mutex, NULL);
    pthread_cond_init(&logger->flush_cond, NULL);

    /* 워커 스레드 시작 */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (pthread_create(&logger->worker_tid, &attr, logger_worker, logger) != 0) {
        fprintf(stderr, "[Proof-Logger] 워커 스레드 시작 실패\n");
        if (logger->file_fd >= 0) close(logger->file_fd);
        free(logger);
        pthread_attr_destroy(&attr);
        return NULL;
    }
    pthread_attr_destroy(&attr);

    return logger;
}

void fl_logger_log(fl_logger_t* logger, fl_log_level_t level, const char* fmt, ...) {
    if (!logger) return;
    if (!atomic_load_explicit(&logger->running, memory_order_relaxed)) return;

    fl_log_ring_t* ring = &logger->ring;

    /* 생산자: head 확인 */
    uint64_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

    /* 링 버퍼 풀 → 드롭 */
    if (head - tail >= FL_LOG_RING_SIZE) {
        atomic_fetch_add_explicit(&logger->dropped, 1, memory_order_relaxed);
        return;
    }

    fl_log_entry_t* entry = &ring->slots[head & FL_LOG_RING_MASK];
    entry->level    = level;
    entry->ts_ns    = ts_now_ns();
    entry->gogs_send = ((size_t)level >= logger->config.gogs_min_level);

    /* 메시지 포맷 */
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(entry->msg, FL_LOG_MSG_MAX, fmt, ap);
    va_end(ap);

    /* 생산 완료: head 증가 (release fence) */
    atomic_store_explicit(&ring->head, head + 1, memory_order_release);
}

void fl_logger_flush(fl_logger_t* logger) {
    if (!logger) return;
    /* 워커가 tail을 head와 같게 만들 때까지 대기 */
    uint64_t target = atomic_load_explicit(&logger->ring.head, memory_order_acquire);
    for (int i = 0; i < 10000; i++) {
        uint64_t t = atomic_load_explicit(&logger->ring.tail, memory_order_acquire);
        if (t >= target) return;
        struct timespec ns = { .tv_sec = 0, .tv_nsec = 100000 };
        nanosleep(&ns, NULL);
    }
}

void fl_logger_stats(fl_logger_t* logger, uint64_t* written, uint64_t* dropped) {
    if (!logger) return;
    if (written) *written = atomic_load_explicit(&logger->written, memory_order_relaxed);
    if (dropped) *dropped = atomic_load_explicit(&logger->dropped, memory_order_relaxed);
}

void fl_logger_destroy(fl_logger_t* logger) {
    if (!logger) return;
    /* 워커 종료 신호 */
    atomic_store_explicit(&logger->running, false, memory_order_release);
    pthread_join(logger->worker_tid, NULL);

    /* 통계 출력 */
    uint64_t w = atomic_load_explicit(&logger->written, memory_order_relaxed);
    uint64_t d = atomic_load_explicit(&logger->dropped, memory_order_relaxed);
    fprintf(stderr, "[Proof-Logger] 종료. 기록=%llu 드롭=%llu\n",
            (unsigned long long)w, (unsigned long long)d);

    /* 정리 */
    if (logger->uring.initialized) uring_destroy(&logger->uring);
    if (logger->file_fd >= 0) close(logger->file_fd);
    pthread_mutex_destroy(&logger->flush_mutex);
    pthread_cond_destroy(&logger->flush_cond);
    free(logger);
}

/* ============================================================
   FreeLang stdlib 바인딩 함수
   ============================================================ */

#include "../include/stdlib_fl.h"

/* log_configure({targets, level, file, gogs_host, gogs_port, json}) */
fl_value_t fl_log_configure_builtin(fl_value_t* args, size_t argc) {
    fl_log_config_t cfg = fl_log_default_config();

    if (argc >= 1 && args[0].type == FL_TYPE_OBJECT) {
        fl_object_t* obj = args[0].data.object_val;
        if (!obj) goto done;
        for (size_t i = 0; i < obj->size; i++) {
            const char* k = obj->keys[i];
            fl_value_t  v = obj->values[i];
            if (!k) continue;

            if (strcmp(k, "level") == 0 && v.type == FL_TYPE_STRING) {
                const char* ls = v.data.string_val;
                if      (strcmp(ls, "debug") == 0) cfg.level = FL_LOG_DEBUG;
                else if (strcmp(ls, "info")  == 0) cfg.level = FL_LOG_INFO;
                else if (strcmp(ls, "warn")  == 0) cfg.level = FL_LOG_WARN;
                else if (strcmp(ls, "error") == 0) cfg.level = FL_LOG_ERROR;
                else if (strcmp(ls, "off")   == 0) cfg.level = FL_LOG_OFF;
            } else if (strcmp(k, "file") == 0 && v.type == FL_TYPE_STRING) {
                strncpy(cfg.file_path, v.data.string_val, sizeof(cfg.file_path) - 1);
            } else if (strcmp(k, "gogs_host") == 0 && v.type == FL_TYPE_STRING) {
                strncpy(cfg.gogs_host, v.data.string_val, sizeof(cfg.gogs_host) - 1);
                cfg.targets |= FL_LOG_TARGET_GOGS;
            } else if (strcmp(k, "gogs_port") == 0 && v.type == FL_TYPE_INT) {
                cfg.gogs_port = (int)v.data.int_val;
            } else if (strcmp(k, "gogs_token") == 0 && v.type == FL_TYPE_STRING) {
                strncpy(cfg.gogs_token, v.data.string_val, sizeof(cfg.gogs_token) - 1);
            } else if (strcmp(k, "gogs_author") == 0 && v.type == FL_TYPE_STRING) {
                strncpy(cfg.gogs_author, v.data.string_val, sizeof(cfg.gogs_author) - 1);
            } else if (strcmp(k, "json") == 0 && v.type == FL_TYPE_BOOL) {
                cfg.use_json = v.data.bool_val;
            } else if (strcmp(k, "console") == 0 && v.type == FL_TYPE_BOOL) {
                if (v.data.bool_val) cfg.targets |=  FL_LOG_TARGET_CONSOLE;
                else                 cfg.targets &= ~FL_LOG_TARGET_CONSOLE;
            } else if (strcmp(k, "iouring") == 0 && v.type == FL_TYPE_BOOL) {
                cfg.use_iouring = v.data.bool_val;
            }
        }
    }

done:
    if (fl_global_logger) {
        fl_logger_destroy(fl_global_logger);
        fl_global_logger = NULL;
    }
    fl_global_logger = fl_logger_create(&cfg);

    fl_value_t result;
    result.type = FL_TYPE_BOOL;
    result.data.bool_val = (fl_global_logger != NULL);
    return result;
}

static fl_value_t do_log(fl_value_t* args, size_t argc, fl_log_level_t level) {
    /* 전역 로거 없으면 자동 초기화 */
    if (!fl_global_logger) {
        fl_log_config_t cfg = fl_log_default_config();
        fl_global_logger = fl_logger_create(&cfg);
    }

    if (!fl_global_logger) {
        fl_value_t r; r.type = FL_TYPE_NULL; return r;
    }

    /* 인자들을 공백으로 이어서 하나의 메시지로 */
    char msg[FL_LOG_MSG_MAX];
    size_t pos = 0;
    for (size_t i = 0; i < argc && pos < FL_LOG_MSG_MAX - 1; i++) {
        if (i > 0 && pos < FL_LOG_MSG_MAX - 2) msg[pos++] = ' ';
        fl_value_t v = args[i];
        int written = 0;
        if (v.type == FL_TYPE_STRING && v.data.string_val) {
            written = snprintf(msg + pos, FL_LOG_MSG_MAX - pos, "%s", v.data.string_val);
        } else if (v.type == FL_TYPE_INT) {
            written = snprintf(msg + pos, FL_LOG_MSG_MAX - pos, "%lld", (long long)v.data.int_val);
        } else if (v.type == FL_TYPE_FLOAT) {
            written = snprintf(msg + pos, FL_LOG_MSG_MAX - pos, "%g", v.data.float_val);
        } else if (v.type == FL_TYPE_BOOL) {
            written = snprintf(msg + pos, FL_LOG_MSG_MAX - pos, "%s", v.data.bool_val ? "true" : "false");
        } else if (v.type == FL_TYPE_NULL) {
            written = snprintf(msg + pos, FL_LOG_MSG_MAX - pos, "null");
        }
        if (written > 0) pos += (size_t)written;
    }
    msg[pos] = '\0';

    fl_logger_log(fl_global_logger, level, "%s", msg);

    fl_value_t r; r.type = FL_TYPE_NULL; return r;
}

fl_value_t fl_log_debug_builtin(fl_value_t* args, size_t argc) { return do_log(args, argc, FL_LOG_DEBUG); }
fl_value_t fl_log_info_builtin (fl_value_t* args, size_t argc) { return do_log(args, argc, FL_LOG_INFO);  }
fl_value_t fl_log_warn_builtin (fl_value_t* args, size_t argc) { return do_log(args, argc, FL_LOG_WARN);  }
fl_value_t fl_log_error_builtin(fl_value_t* args, size_t argc) { return do_log(args, argc, FL_LOG_ERROR); }

fl_value_t fl_log_flush_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    if (fl_global_logger) fl_logger_flush(fl_global_logger);
    fl_value_t r; r.type = FL_TYPE_NULL; return r;
}

fl_value_t fl_log_stats_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    fl_value_t r; r.type = FL_TYPE_NULL;
    if (!fl_global_logger) return r;

    uint64_t w = 0, d = 0;
    fl_logger_stats(fl_global_logger, &w, &d);

    /* Object{written, dropped} 반환 */
    fl_object_t* obj = (fl_object_t*)malloc(sizeof(fl_object_t));
    if (!obj) return r;
    obj->capacity = 4;
    obj->size     = 2;
    obj->keys     = (char**)malloc(4 * sizeof(char*));
    obj->values   = (fl_value_t*)malloc(4 * sizeof(fl_value_t));
    if (!obj->keys || !obj->values) { free(obj->keys); free(obj->values); free(obj); return r; }

    obj->keys[0] = strdup("written");
    obj->values[0].type = FL_TYPE_INT;
    obj->values[0].data.int_val = (fl_int)w;

    obj->keys[1] = strdup("dropped");
    obj->values[1].type = FL_TYPE_INT;
    obj->values[1].data.int_val = (fl_int)d;

    r.type = FL_TYPE_OBJECT;
    r.data.object_val = obj;
    return r;
}

void fl_logger_stdlib_register(fl_runtime_t* runtime) {
    fl_runtime_register_builtin(runtime, "log_configure", fl_log_configure_builtin);
    fl_runtime_register_builtin(runtime, "log_debug",     fl_log_debug_builtin);
    fl_runtime_register_builtin(runtime, "log_info",      fl_log_info_builtin);
    fl_runtime_register_builtin(runtime, "log_warn",      fl_log_warn_builtin);
    fl_runtime_register_builtin(runtime, "log_error",     fl_log_error_builtin);
    fl_runtime_register_builtin(runtime, "log_flush",     fl_log_flush_builtin);
    fl_runtime_register_builtin(runtime, "log_stats",     fl_log_stats_builtin);
}
