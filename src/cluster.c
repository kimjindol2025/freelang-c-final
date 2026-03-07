/**
 * FreeLang Cluster Manager 구현
 * @cluster(n) 어노테이션 런타임
 *
 * SO_REUSEPORT: 모든 워커가 동일 포트 바인딩 → OS가 자동 로드밸런싱
 * IPC: parent↔worker 파이프 통신
 * Phoenix-Spawn: 워커 크래시 → 즉시 교체
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#include "../include/cluster.h"

/* ============================================================
   글로벌 클러스터 상태 (SIGCHLD 핸들러용)
   ============================================================ */
static fl_cluster_t* g_cluster = NULL;

/* ============================================================
   CPU 코어 수 감지
   ============================================================ */
int fl_cluster_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n <= 0) n = 1;
    if (n > FL_CLUSTER_MAX_WORKERS) n = FL_CLUSTER_MAX_WORKERS;
    return (int)n;
}

/* ============================================================
   Worker 재시작 (크래시 복구)
   ============================================================ */
int fl_cluster_respawn_worker(fl_cluster_t* cluster, int worker_id) {
    if (!cluster || worker_id < 0 || worker_id >= cluster->worker_count) return -1;

    fl_worker_t* w = &cluster->workers[worker_id];

    /* 기존 파이프 닫기 */
    close(w->ipc_pipe[FL_CLUSTER_READ]);
    close(w->ipc_pipe[FL_CLUSTER_WRITE]);

    /* 새 IPC 파이프 */
    if (pipe(w->ipc_pipe) < 0) {
        perror("[CLUSTER] pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[CLUSTER] fork");
        return -1;
    }

    if (pid == 0) {
        /* Worker 프로세스 */
        close(w->ipc_pipe[FL_CLUSTER_WRITE]); /* 쓰기 쪽 닫기 */

        /* cluster 컨텍스트 업데이트 */
        cluster->is_master = false;
        cluster->worker_id = worker_id;

        /* worker_main 호출 */
        if (cluster->worker_main) {
            cluster->worker_main(worker_id, cluster->userdata);
        }
        _exit(0);
    }

    /* Master */
    close(w->ipc_pipe[FL_CLUSTER_READ]); /* 읽기 쪽 닫기 (master는 쓰기만) */
    w->pid      = pid;
    w->status   = FL_WORKER_RUNNING;
    w->restarts++;
    fprintf(stderr, "[CLUSTER] worker #%d (pid=%d) started\n", worker_id, pid);
    return 0;
}

/* ============================================================
   SIGCHLD 핸들러 (Phoenix-Spawn)
   ============================================================ */
static void cluster_sigchld(int signo) {
    (void)signo;
    if (!g_cluster) return;

    pid_t pid;
    int   status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < g_cluster->worker_count; i++) {
            fl_worker_t* w = &g_cluster->workers[i];
            if (w->pid == pid) {
                int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                fprintf(stderr, "[CLUSTER] worker #%d (pid=%d) died (exit=%d) → respawning\n",
                        i, pid, code);
                w->status = FL_WORKER_CRASHED;
                w->pid    = -1;
                fl_cluster_respawn_worker(g_cluster, i);
                break;
            }
        }
    }
}

/* ============================================================
   클러스터 생성 및 Worker 포크
   ============================================================ */
fl_cluster_t* fl_cluster_spawn(int n,
                                void (*worker_fn)(int id, void* data),
                                void* userdata) {
    if (n <= 0) n = fl_cluster_cpu_count();
    if (n > FL_CLUSTER_MAX_WORKERS) n = FL_CLUSTER_MAX_WORKERS;

    fl_cluster_t* cluster = (fl_cluster_t*)calloc(1, sizeof(*cluster));
    if (!cluster) return NULL;

    cluster->worker_count = n;
    cluster->is_master    = true;
    cluster->worker_id    = -1;
    cluster->worker_main  = worker_fn;
    cluster->userdata     = userdata;

    g_cluster = cluster;

    fprintf(stderr, "[CLUSTER] spawning %d workers\n", n);

    for (int i = 0; i < n; i++) {
        fl_worker_t* w = &cluster->workers[i];
        w->id          = i;
        w->pid         = -1;
        w->status      = FL_WORKER_IDLE;
        w->restarts    = 0;

        /* IPC 파이프 생성 */
        if (pipe(w->ipc_pipe) < 0) {
            perror("[CLUSTER] pipe");
            fl_cluster_destroy(cluster);
            return NULL;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("[CLUSTER] fork");
            fl_cluster_destroy(cluster);
            return NULL;
        }

        if (pid == 0) {
            /* Worker 프로세스 */
            close(w->ipc_pipe[FL_CLUSTER_WRITE]);

            cluster->is_master = false;
            cluster->worker_id = i;

            if (worker_fn) {
                worker_fn(i, userdata);
            }
            _exit(0);
        }

        /* Master */
        close(w->ipc_pipe[FL_CLUSTER_READ]);
        w->pid    = pid;
        w->status = FL_WORKER_RUNNING;
        fprintf(stderr, "[CLUSTER] worker #%d (pid=%d) spawned\n", i, pid);
    }

    return cluster; /* master만 여기 도달 */
}

/* ============================================================
   Master 이벤트 루프
   ============================================================ */
void fl_cluster_run_master(fl_cluster_t* cluster) {
    if (!cluster || !cluster->is_master) return;

    /* SIGCHLD 핸들러 설치 */
    struct sigaction sa;
    sa.sa_handler = cluster_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    fprintf(stderr, "[CLUSTER] master running, managing %d workers\n",
            cluster->worker_count);

    /* 무한 대기 (SIGCHLD가 자동 복구) */
    while (1) {
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);

        /* 상태 출력 (1초마다) */
        int alive = 0;
        for (int i = 0; i < cluster->worker_count; i++) {
            if (cluster->workers[i].status == FL_WORKER_RUNNING) alive++;
        }
        fprintf(stderr, "[CLUSTER] alive workers: %d/%d\n", alive, cluster->worker_count);
    }
}

/* ============================================================
   IPC 브로드캐스트
   ============================================================ */
int fl_cluster_broadcast(fl_cluster_t* cluster, fl_ipc_msg_t msg) {
    if (!cluster) return -1;
    int sent = 0;
    for (int i = 0; i < cluster->worker_count; i++) {
        fl_worker_t* w = &cluster->workers[i];
        if (w->status == FL_WORKER_RUNNING && w->ipc_pipe[FL_CLUSTER_WRITE] >= 0) {
            ssize_t n = write(w->ipc_pipe[FL_CLUSTER_WRITE], &msg, sizeof(msg));
            if (n == sizeof(msg)) sent++;
        }
    }
    return sent;
}

/* ============================================================
   정리
   ============================================================ */
void fl_cluster_destroy(fl_cluster_t* cluster) {
    if (!cluster) return;

    for (int i = 0; i < cluster->worker_count; i++) {
        fl_worker_t* w = &cluster->workers[i];
        if (w->pid > 0 && w->status == FL_WORKER_RUNNING) {
            kill(w->pid, SIGTERM);
        }
        if (w->ipc_pipe[FL_CLUSTER_WRITE] >= 0) close(w->ipc_pipe[FL_CLUSTER_WRITE]);
    }

    if (g_cluster == cluster) g_cluster = NULL;
    free(cluster);
}
