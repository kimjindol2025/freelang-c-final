/**
 * FreeLang Runtime Introspection
 * System.metrics() 구현
 *
 * 기능:
 * - CPU 사용률 (/proc/stat + getrusage)
 * - 메모리 사용량 (/proc/self/status)
 * - 프로세스 정보 (PID, uptime, restart count)
 * - GC 통계
 */

#ifndef FL_INTROSPECT_H
#define FL_INTROSPECT_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/resource.h>

/* 시스템 메트릭 스냅샷 */
typedef struct {
    /* 프로세스 */
    int      pid;
    uint64_t uptime_ms;         /* 프로세스 가동 시간 */

    /* 메모리 (KB) */
    uint64_t mem_rss_kb;        /* Resident Set Size */
    uint64_t mem_virt_kb;       /* Virtual memory */
    uint64_t mem_heap_kb;       /* 힙 사용량 추정 */

    /* CPU */
    double   cpu_user_ms;       /* 유저 공간 CPU 시간 (ms) */
    double   cpu_sys_ms;        /* 커널 공간 CPU 시간 (ms) */
    double   cpu_percent;       /* 최근 CPU 점유율 (%) */

    /* 이벤트 */
    uint64_t gc_collections;    /* GC 실행 횟수 */
    uint64_t gc_freed_bytes;    /* GC가 해제한 바이트 */

    /* I/O */
    uint64_t io_reads;
    uint64_t io_writes;
} fl_metrics_t;

/* ===== 공개 API ===== */

/* 메트릭 수집 */
fl_metrics_t fl_metrics_collect(void);

/* 특정 필드 조회 */
uint64_t fl_metrics_rss_kb(void);
double   fl_metrics_cpu_percent(void);
uint64_t fl_metrics_uptime_ms(void);

/* GC 카운터 업데이트 (gc.c에서 호출) */
void fl_metrics_record_gc(uint64_t freed_bytes);

/* 메트릭을 JSON 문자열로 직렬화 (malloc, 호출자가 free) */
char* fl_metrics_to_json(const fl_metrics_t* m);

/* 프로세스 시작 시각 초기화 (main에서 1회 호출) */
void fl_metrics_init(void);

#endif /* FL_INTROSPECT_H */
