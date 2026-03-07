/**
 * FreeLang Process Manager - Phoenix-Spawn
 * PM2 대체: 외부 도구 없는 네이티브 프로세스 관리
 *
 * 기능:
 * - Phoenix-Spawn: 자식 프로세스 크래시 감지 → 0.1ms 내 재시작
 * - 프로세스 상태 추적 (PID, 상태, 재시작 횟수)
 * - SIGCHLD/SIGTERM/SIGINT 시그널 핸들링
 * - 데몬화 (백그라운드 실행)
 */

#ifndef FL_PROCESS_H
#define FL_PROCESS_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* 프로세스 상태 */
typedef enum {
    FL_PROC_STOPPED   = 0,
    FL_PROC_RUNNING   = 1,
    FL_PROC_CRASHED   = 2,
    FL_PROC_RESTARTING = 3,
    FL_PROC_STOPPING  = 4
} fl_proc_status_t;

/* 단일 관리 프로세스 엔트리 */
typedef struct fl_process_entry {
    char     name[128];         /* 프로세스 이름 */
    char     script[512];       /* 실행할 파일 경로 */
    char**   args;              /* 인수 배열 */
    int      argc;

    pid_t    pid;               /* 현재 PID */
    fl_proc_status_t status;
    int      exit_code;         /* 마지막 종료 코드 */

    int      restarts;          /* 총 재시작 횟수 */
    int      max_restarts;      /* -1 = 무제한 */
    bool     autorestart;       /* 자동 재시작 여부 */

    time_t   started_at;        /* 최초 시작 시각 */
    time_t   last_restart;      /* 마지막 재시작 시각 */
    uint64_t uptime_ms;         /* 누적 가동 시간 (ms) */

    struct fl_process_entry* next;
} fl_process_entry_t;

/* 프로세스 매니저 (싱글턴) */
typedef struct {
    fl_process_entry_t* head;   /* 관리 프로세스 연결 리스트 */
    int                 count;

    bool                running;
    bool                daemon_mode;
} fl_process_manager_t;

/* ===== 공개 API ===== */

/* 매니저 초기화/해제 */
fl_process_manager_t* fl_pm_create(void);
void                  fl_pm_destroy(fl_process_manager_t* pm);

/* 프로세스 등록 및 시작 */
fl_process_entry_t* fl_pm_register(fl_process_manager_t* pm,
                                    const char* name,
                                    const char* script,
                                    bool autorestart,
                                    int max_restarts);

int  fl_pm_start(fl_process_manager_t* pm, const char* name);
int  fl_pm_stop(fl_process_manager_t* pm, const char* name);
int  fl_pm_restart(fl_process_manager_t* pm, const char* name);

/* Phoenix-Spawn watchdog: SIGCHLD 핸들러 설치 + 메인 이벤트 루프 */
void fl_pm_install_watchdog(fl_process_manager_t* pm);
void fl_pm_run_forever(fl_process_manager_t* pm);   /* 블로킹: 시그널 대기 */

/* 데몬화 */
int  fl_pm_daemonize(void);

/* 상태 조회 */
fl_process_entry_t* fl_pm_find(fl_process_manager_t* pm, const char* name);
void                fl_pm_list(fl_process_manager_t* pm);

/* 글로벌 싱글턴 접근 (SIGCHLD 핸들러에서 사용) */
fl_process_manager_t* fl_pm_global(void);

#endif /* FL_PROCESS_H */
