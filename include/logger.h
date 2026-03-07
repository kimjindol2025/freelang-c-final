/**
 * Proof-Logger: FreeLang 내장 비동기 로깅 커널
 *
 * 설계 원칙:
 *  - SPSC 링 버퍼: 생산자(메인)가 절대 블로킹되지 않음
 *  - 워커 스레드가 소비 → 파일/콘솔/Gogs HTTP 전송
 *  - io_uring 시스템콜 직접 호출 (Linux 5.1+, liburing 불필요)
 *  - @log_level 어노테이션: 컴파일 시 낮은 레벨 제거
 *  - JSON 포맷: {"level":"INFO","ts":1234567890123,"msg":"..."}
 *  - Gogs HTTP: guestbook API로 중요 이벤트 자동 전송
 */

#ifndef FL_LOGGER_H
#define FL_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>

/* ============================================================
   로그 레벨 (컴파일 시 필터링 기준)
   ============================================================ */

typedef enum {
    FL_LOG_DEBUG = 0,
    FL_LOG_INFO  = 1,
    FL_LOG_WARN  = 2,
    FL_LOG_ERROR = 3,
    FL_LOG_OFF   = 4   /* 로깅 완전 비활성화 */
} fl_log_level_t;

/* 컴파일 시 @log_level 어노테이션으로 설정되는 전역 임계값 */
/* 이 값보다 낮은 레벨의 log_*() 호출은 컴파일러가 NOP으로 대체 */
extern fl_log_level_t fl_compile_log_level;

/* 레벨 이름 문자열 */
static inline const char* fl_log_level_name(fl_log_level_t level) {
    switch (level) {
        case FL_LOG_DEBUG: return "DEBUG";
        case FL_LOG_INFO:  return "INFO";
        case FL_LOG_WARN:  return "WARN";
        case FL_LOG_ERROR: return "ERROR";
        default:           return "OFF";
    }
}

/* ============================================================
   SPSC 링 버퍼 (Single Producer Single Consumer, Lockless)
   ============================================================ */

#define FL_LOG_RING_SHIFT   10                      /* 2^10 = 1024 슬롯 (스택 안전) */
#define FL_LOG_RING_SIZE    (1u << FL_LOG_RING_SHIFT)
#define FL_LOG_RING_MASK    (FL_LOG_RING_SIZE - 1u)
#define FL_LOG_MSG_MAX      512                     /* 메시지 최대 길이 */

typedef struct {
    char            msg[FL_LOG_MSG_MAX];  /* 포맷된 로그 메시지 */
    fl_log_level_t  level;
    uint64_t        ts_ns;                /* 나노초 타임스탬프 */
    bool            gogs_send;            /* Gogs로 전송 여부 */
} fl_log_entry_t;

typedef struct {
    fl_log_entry_t      slots[FL_LOG_RING_SIZE];
    _Atomic uint64_t    head;   /* 생산자 위치 (메인 스레드) */
    _Atomic uint64_t    tail;   /* 소비자 위치 (워커 스레드) */
    char                _pad[48]; /* 캐시라인 패딩 (고정 크기) */
} fl_log_ring_t;

/* ============================================================
   타겟 설정
   ============================================================ */

typedef enum {
    FL_LOG_TARGET_CONSOLE = 0x01,   /* stderr */
    FL_LOG_TARGET_FILE    = 0x02,   /* 파일 (비동기 pwrite) */
    FL_LOG_TARGET_GOGS    = 0x04    /* Gogs guestbook HTTP POST */
} fl_log_target_t;

#define FL_LOG_TARGET_ALL  (FL_LOG_TARGET_CONSOLE | FL_LOG_TARGET_FILE | FL_LOG_TARGET_GOGS)

/* ============================================================
   로거 설정
   ============================================================ */

typedef struct {
    uint32_t        targets;            /* 비트마스크: FL_LOG_TARGET_* */
    fl_log_level_t  level;              /* 런타임 최소 레벨 */
    char            file_path[256];     /* 로그 파일 경로 */
    char            gogs_host[128];     /* Gogs 호스트 (예: gogs.dclub.kr) */
    int             gogs_port;          /* Gogs 포트 (기본: 443) */
    char            gogs_token[128];    /* Gogs API 토큰 */
    char            gogs_author[64];    /* 방명록 작성자 이름 */
    bool            use_json;           /* JSON 포맷 여부 */
    bool            use_color;          /* ANSI 색상 (콘솔) */
    bool            use_iouring;        /* io_uring 사용 (Linux 5.1+) */
    size_t          gogs_min_level;     /* Gogs 전송 최소 레벨 (기본: WARN) */
} fl_log_config_t;

/* ============================================================
   io_uring 내부 구조체 (linux/io_uring.h 없이 최소 정의)
   ============================================================ */

#define FL_IOURING_SQE_SIZE  64
#define FL_IOURING_CQE_SIZE  16
#define FL_IOURING_ENTRIES   256   /* SQ 크기 */

/* io_uring_params (커널 ↔ 유저 공유) */
typedef struct fl_iouring_params {
    uint32_t sq_entries;
    uint32_t cq_entries;
    uint32_t flags;
    uint32_t sq_thread_cpu;
    uint32_t sq_thread_idle;
    uint32_t features;
    uint32_t wq_fd;
    uint32_t resv[3];
    struct { uint32_t head, tail, ring_mask, ring_entries, flags, dropped, array[0]; } sq_off;
    struct { uint32_t head, tail, ring_mask, ring_entries, overflow, cqes[0]; } cq_off;
} fl_iouring_params_t;

/* io_uring 상태 */
typedef struct {
    int         ring_fd;        /* io_uring FD */
    bool        initialized;    /* 초기화 성공 여부 */
    void*       sq_ring;        /* SQ 링 mmap */
    void*       cq_ring;        /* CQ 링 mmap */
    void*       sqes;           /* SQE 배열 mmap */
    size_t      sq_ring_sz;
    size_t      cq_ring_sz;
    size_t      sqe_sz;
    uint32_t*   sq_head;
    uint32_t*   sq_tail;
    uint32_t*   sq_ring_mask;
    uint32_t*   sq_array;
    uint32_t*   cq_head;
    uint32_t*   cq_tail;
    uint32_t*   cq_ring_mask;
} fl_iouring_t;

/* ============================================================
   로거 메인 구조체
   ============================================================ */

typedef struct {
    fl_log_ring_t       ring;           /* SPSC 링 버퍼 */
    fl_log_config_t     config;         /* 설정 */
    pthread_t           worker_tid;     /* 워커 스레드 ID */
    pthread_mutex_t     flush_mutex;
    pthread_cond_t      flush_cond;
    int                 file_fd;        /* 로그 파일 FD */
    fl_iouring_t        uring;          /* io_uring 상태 */
    _Atomic bool        running;        /* 실행 중 여부 */
    _Atomic uint64_t    dropped;        /* 드롭된 메시지 수 (버퍼 풀) */
    _Atomic uint64_t    written;        /* 성공 기록 수 */
    char                author_id[64];  /* 방명록 author_id (PID 기반) */
} fl_logger_t;

/* ============================================================
   전역 로거 (싱글톤)
   ============================================================ */

extern fl_logger_t* fl_global_logger;

/* ============================================================
   공개 API
   ============================================================ */

/* 로거 초기화 (워커 스레드 시작) */
fl_logger_t* fl_logger_create(const fl_log_config_t* config);

/* 로거 종료 (버퍼 플러시 후 스레드 종료) */
void fl_logger_destroy(fl_logger_t* logger);

/* 로그 기록 (논블로킹, 링 버퍼 enqueue) */
void fl_logger_log(fl_logger_t* logger, fl_log_level_t level,
                   const char* fmt, ...) __attribute__((format(printf, 3, 4)));

/* 버퍼 완전 플러시 (동기) */
void fl_logger_flush(fl_logger_t* logger);

/* 통계 조회 */
void fl_logger_stats(fl_logger_t* logger, uint64_t* written, uint64_t* dropped);

/* 기본 설정 반환 */
fl_log_config_t fl_log_default_config(void);

/* ============================================================
   편의 매크로 (전역 로거 사용)
   ============================================================ */

#define FL_LOG_D(fmt, ...) \
    do { if (fl_global_logger && FL_LOG_DEBUG >= fl_compile_log_level) \
         fl_logger_log(fl_global_logger, FL_LOG_DEBUG, fmt, ##__VA_ARGS__); } while(0)

#define FL_LOG_I(fmt, ...) \
    do { if (fl_global_logger && FL_LOG_INFO >= fl_compile_log_level) \
         fl_logger_log(fl_global_logger, FL_LOG_INFO, fmt, ##__VA_ARGS__); } while(0)

#define FL_LOG_W(fmt, ...) \
    do { if (fl_global_logger && FL_LOG_WARN >= fl_compile_log_level) \
         fl_logger_log(fl_global_logger, FL_LOG_WARN, fmt, ##__VA_ARGS__); } while(0)

#define FL_LOG_E(fmt, ...) \
    do { if (fl_global_logger && FL_LOG_ERROR >= fl_compile_log_level) \
         fl_logger_log(fl_global_logger, FL_LOG_ERROR, fmt, ##__VA_ARGS__); } while(0)

/* ============================================================
   FreeLang stdlib 함수 (runtime에서 호출)
   ============================================================ */

#include "freelang.h"

fl_value_t fl_log_configure_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_debug_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_info_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_warn_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_error_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_flush_builtin(fl_value_t* args, size_t argc);
fl_value_t fl_log_stats_builtin(fl_value_t* args, size_t argc);

void fl_logger_stdlib_register(fl_runtime_t* runtime);

#endif /* FL_LOGGER_H */
