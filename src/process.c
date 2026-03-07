/**
 * FreeLang Process Manager - Phoenix-Spawn 구현
 * PM2를 대체하는 네이티브 프로세스 관리 엔진
 *
 * Phoenix-Spawn 원리:
 *   1. parent가 SIGCHLD 핸들러 설치
 *   2. 자식 프로세스 종료 시 → waitpid로 PID + 종료코드 수집
 *   3. autorestart=true 이면 → 즉시 fork+exec으로 재시작
 *   4. max_restarts 초과 시 → 영구 중단
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "../include/process.h"

/* ============================================================
   글로벌 싱글턴
   ============================================================ */
static fl_process_manager_t* g_pm = NULL;

fl_process_manager_t* fl_pm_global(void) {
    return g_pm;
}

/* ============================================================
   내부 헬퍼: 타임스탬프 (ms)
   ============================================================ */
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ============================================================
   Phoenix-Spawn SIGCHLD 핸들러
   ============================================================ */
static void sigchld_handler(int signo) {
    (void)signo;
    fl_process_manager_t* pm = fl_pm_global();
    if (!pm) return;

    pid_t pid;
    int   status;

    /* Non-blocking으로 모든 종료된 자식 수거 */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fl_process_entry_t* entry = pm->head;
        while (entry) {
            if (entry->pid == pid) {
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                entry->exit_code = exit_code;

                if (exit_code != 0 || WIFSIGNALED(status)) {
                    entry->status = FL_PROC_CRASHED;
                } else {
                    entry->status = FL_PROC_STOPPED;
                }

                /* Phoenix-Spawn: autorestart 처리 */
                if (entry->autorestart &&
                    (entry->max_restarts < 0 || entry->restarts < entry->max_restarts)) {
                    entry->status   = FL_PROC_RESTARTING;
                    entry->restarts++;
                    entry->last_restart = time(NULL);

                    /* re-fork */
                    pid_t new_pid = fork();
                    if (new_pid == 0) {
                        /* 자식 프로세스: exec */
                        char* argv[3];
                        argv[0] = entry->script;
                        argv[1] = NULL;
                        execv(entry->script, argv);
                        _exit(127);  /* exec 실패 */
                    } else if (new_pid > 0) {
                        entry->pid    = new_pid;
                        entry->status = FL_PROC_RUNNING;
                    }
                }
                break;
            }
            entry = entry->next;
        }
    }
}

/* ============================================================
   SIGTERM/SIGINT 핸들러 (그레이스풀 셧다운)
   ============================================================ */
static void sigterm_handler(int signo) {
    (void)signo;
    fl_process_manager_t* pm = fl_pm_global();
    if (!pm) { _exit(0); }

    pm->running = false;

    /* 모든 자식 프로세스에 SIGTERM 전송 */
    fl_process_entry_t* entry = pm->head;
    while (entry) {
        if (entry->pid > 0 && entry->status == FL_PROC_RUNNING) {
            kill(entry->pid, SIGTERM);
            entry->status = FL_PROC_STOPPING;
        }
        entry = entry->next;
    }
}

/* ============================================================
   매니저 생성/해제
   ============================================================ */
fl_process_manager_t* fl_pm_create(void) {
    fl_process_manager_t* pm = (fl_process_manager_t*)malloc(sizeof(*pm));
    if (!pm) return NULL;

    pm->head        = NULL;
    pm->count       = 0;
    pm->running     = false;
    pm->daemon_mode = false;

    g_pm = pm;
    return pm;
}

void fl_pm_destroy(fl_process_manager_t* pm) {
    if (!pm) return;

    fl_process_entry_t* entry = pm->head;
    while (entry) {
        fl_process_entry_t* next = entry->next;
        if (entry->args) {
            for (int i = 0; i < entry->argc; i++) {
                free(entry->args[i]);
            }
            free(entry->args);
        }
        free(entry);
        entry = next;
    }
    free(pm);
    if (g_pm == pm) g_pm = NULL;
}

/* ============================================================
   프로세스 등록
   ============================================================ */
fl_process_entry_t* fl_pm_register(fl_process_manager_t* pm,
                                    const char* name,
                                    const char* script,
                                    bool autorestart,
                                    int max_restarts) {
    if (!pm || !name || !script) return NULL;

    fl_process_entry_t* entry = (fl_process_entry_t*)calloc(1, sizeof(*entry));
    if (!entry) return NULL;

    strncpy(entry->name,   name,   sizeof(entry->name)   - 1);
    strncpy(entry->script, script, sizeof(entry->script) - 1);
    entry->pid          = -1;
    entry->status       = FL_PROC_STOPPED;
    entry->autorestart  = autorestart;
    entry->max_restarts = max_restarts;
    entry->restarts     = 0;

    /* 연결 리스트에 추가 */
    entry->next = pm->head;
    pm->head    = entry;
    pm->count++;

    return entry;
}

/* ============================================================
   프로세스 시작 (fork + exec)
   ============================================================ */
int fl_pm_start(fl_process_manager_t* pm, const char* name) {
    fl_process_entry_t* entry = fl_pm_find(pm, name);
    if (!entry) {
        fprintf(stderr, "[PM] start: '%s' not found\n", name);
        return -1;
    }
    if (entry->status == FL_PROC_RUNNING) {
        fprintf(stderr, "[PM] start: '%s' already running (pid=%d)\n", name, entry->pid);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[PM] fork");
        return -1;
    }

    if (pid == 0) {
        /* 자식 프로세스 */
        char* argv[2] = { entry->script, NULL };
        execv(entry->script, argv);
        fprintf(stderr, "[PM] execv failed: %s\n", entry->script);
        _exit(127);
    }

    /* 부모 */
    entry->pid        = pid;
    entry->status     = FL_PROC_RUNNING;
    entry->started_at = time(NULL);
    fprintf(stderr, "[PM] started '%s' (pid=%d)\n", name, pid);
    return 0;
}

/* ============================================================
   프로세스 중지
   ============================================================ */
int fl_pm_stop(fl_process_manager_t* pm, const char* name) {
    fl_process_entry_t* entry = fl_pm_find(pm, name);
    if (!entry || entry->pid <= 0) return -1;

    /* 자동 재시작 일시 중단 */
    bool saved_autorestart = entry->autorestart;
    entry->autorestart = false;

    kill(entry->pid, SIGTERM);
    entry->status = FL_PROC_STOPPING;

    /* 최대 2초 대기 */
    for (int i = 0; i < 20; i++) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000L };
        nanosleep(&ts, NULL);
        if (entry->status == FL_PROC_STOPPED) break;
    }

    /* 아직 살아있으면 SIGKILL */
    if (entry->status != FL_PROC_STOPPED && entry->pid > 0) {
        kill(entry->pid, SIGKILL);
    }

    entry->autorestart = saved_autorestart;
    return 0;
}

/* ============================================================
   프로세스 재시작
   ============================================================ */
int fl_pm_restart(fl_process_manager_t* pm, const char* name) {
    fl_pm_stop(pm, name);
    return fl_pm_start(pm, name);
}

/* ============================================================
   Watchdog 설치 + 이벤트 루프
   ============================================================ */
void fl_pm_install_watchdog(fl_process_manager_t* pm) {
    (void)pm;

    struct sigaction sa_chld, sa_term;

    /* SIGCHLD: 자식 종료 감지 */
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    /* SIGTERM/SIGINT: 그레이스풀 셧다운 */
    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa_term, NULL);
    sigaction(SIGINT,  &sa_term, NULL);
}

/* ============================================================
   메인 이벤트 루프 (블로킹)
   ============================================================ */
void fl_pm_run_forever(fl_process_manager_t* pm) {
    pm->running = true;
    fprintf(stderr, "[PM] Phoenix-Spawn watchdog started (%d processes)\n", pm->count);

    while (pm->running) {
        /* sigsuspend 대신 poll: 100ms 슬립 후 상태 점검 */
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000L }; /* 100ms */
        nanosleep(&ts, NULL);
    }

    fprintf(stderr, "[PM] shutting down...\n");

    /* 모든 자식 종료 대기 */
    fl_process_entry_t* entry = pm->head;
    while (entry) {
        if (entry->pid > 0) {
            waitpid(entry->pid, NULL, 0);
        }
        entry = entry->next;
    }
}

/* ============================================================
   데몬화 (이중 fork 기법)
   ============================================================ */
int fl_pm_daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  /* 부모 종료 */

    /* 새 세션 리더 */
    if (setsid() < 0) return -1;

    /* 2차 fork: 터미널 재획득 방지 */
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    /* 작업 디렉토리 + umask */
    chdir("/");
    umask(0);

    /* stdin/stdout/stderr → /dev/null */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }

    return 0;
}

/* ============================================================
   상태 조회
   ============================================================ */
fl_process_entry_t* fl_pm_find(fl_process_manager_t* pm, const char* name) {
    if (!pm || !name) return NULL;
    fl_process_entry_t* entry = pm->head;
    while (entry) {
        if (strcmp(entry->name, name) == 0) return entry;
        entry = entry->next;
    }
    return NULL;
}

static const char* status_str(fl_proc_status_t s) {
    switch (s) {
        case FL_PROC_STOPPED:    return "stopped";
        case FL_PROC_RUNNING:    return "running";
        case FL_PROC_CRASHED:    return "crashed";
        case FL_PROC_RESTARTING: return "restarting";
        case FL_PROC_STOPPING:   return "stopping";
        default:                 return "unknown";
    }
}

void fl_pm_list(fl_process_manager_t* pm) {
    if (!pm) return;
    printf("%-20s %-6s %-10s %-8s %-10s\n",
           "name", "pid", "status", "restarts", "uptime");
    printf("%-20s %-6s %-10s %-8s %-10s\n",
           "----", "---", "------", "--------", "------");

    fl_process_entry_t* entry = pm->head;
    while (entry) {
        uint64_t uptime = 0;
        if (entry->started_at > 0) {
            uptime = (uint64_t)(time(NULL) - entry->started_at);
        }
        printf("%-20s %-6d %-10s %-8d %lus\n",
               entry->name,
               entry->pid,
               status_str(entry->status),
               entry->restarts,
               (unsigned long)uptime);
        entry = entry->next;
    }
}
