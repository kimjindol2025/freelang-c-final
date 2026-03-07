/**
 * FreeLang Runtime Introspection 구현
 * System.metrics() → JSON 반환
 *
 * /proc/self/status  → RSS, VmSize
 * getrusage()        → CPU user/sys time
 * clock_gettime()    → uptime
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "../include/introspect.h"

/* ============================================================
   글로벌 상태
   ============================================================ */
static struct timespec g_start_time = {0, 0};
static uint64_t g_gc_collections  = 0;
static uint64_t g_gc_freed_bytes  = 0;

/* ============================================================
   초기화 (main에서 1회 호출)
   ============================================================ */
void fl_metrics_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);
}

/* ============================================================
   GC 카운터 업데이트
   ============================================================ */
void fl_metrics_record_gc(uint64_t freed_bytes) {
    g_gc_collections++;
    g_gc_freed_bytes += freed_bytes;
}

/* ============================================================
   /proc/self/status 파싱
   ============================================================ */
static void parse_proc_status(uint64_t* rss_kb, uint64_t* virt_kb) {
    *rss_kb  = 0;
    *virt_kb = 0;

    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %llu", (unsigned long long*)rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, " %llu", (unsigned long long*)virt_kb);
        }
    }
    fclose(f);
}

/* ============================================================
   Uptime 계산
   ============================================================ */
uint64_t fl_metrics_uptime_ms(void) {
    if (g_start_time.tv_sec == 0 && g_start_time.tv_nsec == 0) {
        fl_metrics_init();
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint64_t elapsed_ns =
        (uint64_t)(now.tv_sec  - g_start_time.tv_sec)  * 1000000000ULL +
        (uint64_t)(now.tv_nsec - g_start_time.tv_nsec);
    return elapsed_ns / 1000000ULL;
}

/* ============================================================
   RSS 메모리
   ============================================================ */
uint64_t fl_metrics_rss_kb(void) {
    uint64_t rss = 0, virt = 0;
    parse_proc_status(&rss, &virt);
    return rss;
}

/* ============================================================
   CPU 점유율 (단순 추정)
   ============================================================ */
double fl_metrics_cpu_percent(void) {
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) < 0) return 0.0;

    double cpu_ms = (double)ru.ru_utime.tv_sec * 1000.0 +
                    (double)ru.ru_utime.tv_usec / 1000.0 +
                    (double)ru.ru_stime.tv_sec * 1000.0 +
                    (double)ru.ru_stime.tv_usec / 1000.0;

    uint64_t uptime = fl_metrics_uptime_ms();
    if (uptime == 0) return 0.0;

    return (cpu_ms / (double)uptime) * 100.0;
}

/* ============================================================
   전체 메트릭 수집
   ============================================================ */
fl_metrics_t fl_metrics_collect(void) {
    fl_metrics_t m;
    memset(&m, 0, sizeof(m));

    m.pid        = (int)getpid();
    m.uptime_ms  = fl_metrics_uptime_ms();

    /* 메모리 */
    parse_proc_status(&m.mem_rss_kb, &m.mem_virt_kb);
    m.mem_heap_kb = m.mem_rss_kb; /* 근사값 */

    /* CPU */
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        m.cpu_user_ms = (double)ru.ru_utime.tv_sec * 1000.0 +
                        (double)ru.ru_utime.tv_usec / 1000.0;
        m.cpu_sys_ms  = (double)ru.ru_stime.tv_sec * 1000.0 +
                        (double)ru.ru_stime.tv_usec / 1000.0;
        if (m.uptime_ms > 0) {
            m.cpu_percent = ((m.cpu_user_ms + m.cpu_sys_ms) / (double)m.uptime_ms) * 100.0;
        }
    }

    /* GC */
    m.gc_collections = g_gc_collections;
    m.gc_freed_bytes = g_gc_freed_bytes;

    return m;
}

/* ============================================================
   JSON 직렬화
   ============================================================ */
char* fl_metrics_to_json(const fl_metrics_t* m) {
    if (!m) return NULL;

    char* buf = (char*)malloc(512);
    if (!buf) return NULL;

    snprintf(buf, 512,
        "{"
        "\"pid\":%d,"
        "\"uptime_ms\":%llu,"
        "\"mem_rss_kb\":%llu,"
        "\"mem_virt_kb\":%llu,"
        "\"cpu_user_ms\":%.2f,"
        "\"cpu_sys_ms\":%.2f,"
        "\"cpu_percent\":%.2f,"
        "\"gc_collections\":%llu,"
        "\"gc_freed_bytes\":%llu"
        "}",
        m->pid,
        (unsigned long long)m->uptime_ms,
        (unsigned long long)m->mem_rss_kb,
        (unsigned long long)m->mem_virt_kb,
        m->cpu_user_ms,
        m->cpu_sys_ms,
        m->cpu_percent,
        (unsigned long long)m->gc_collections,
        (unsigned long long)m->gc_freed_bytes
    );

    return buf;
}
