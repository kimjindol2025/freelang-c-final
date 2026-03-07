/**
 * FreeLang Cluster Manager
 * @cluster(n) 어노테이션 런타임 구현
 *
 * 기능:
 * - CPU 코어 수 자동 감지 (auto 모드)
 * - SO_REUSEPORT 기반 포트 공유 (로드밸런싱)
 * - Parent-Worker IPC (파이프 기반)
 * - Worker 크래시 시 Phoenix-Spawn 자동 교체
 */

#ifndef FL_CLUSTER_H
#define FL_CLUSTER_H

#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#define FL_CLUSTER_MAX_WORKERS 64
#define FL_CLUSTER_PIPE_READ   0
#define FL_CLUSTER_PIPE_WRITE  1
/* Compatibility aliases used in cluster.c */
#define FL_CLUSTER_READ  FL_CLUSTER_PIPE_READ
#define FL_CLUSTER_WRITE FL_CLUSTER_PIPE_WRITE

/* Worker 상태 */
typedef enum {
    FL_WORKER_IDLE     = 0,
    FL_WORKER_RUNNING  = 1,
    FL_WORKER_CRASHED  = 2,
    FL_WORKER_STOPPING = 3
} fl_worker_status_t;

/* Worker 엔트리 */
typedef struct {
    int                id;          /* Worker ID (0-based) */
    pid_t              pid;
    fl_worker_status_t status;
    int                ipc_pipe[2]; /* parent→worker IPC 파이프 */
    int                restarts;
    uint64_t           requests_handled;
} fl_worker_t;

/* IPC 메시지 타입 */
typedef enum {
    FL_IPC_SHUTDOWN   = 0,
    FL_IPC_RELOAD     = 1,
    FL_IPC_STATUS_REQ = 2,
    FL_IPC_STATUS_RSP = 3
} fl_ipc_msg_type_t;

/* IPC 메시지 */
typedef struct {
    fl_ipc_msg_type_t type;
    uint32_t          worker_id;
    uint64_t          payload;
} fl_ipc_msg_t;

/* 클러스터 컨텍스트 */
typedef struct {
    int          worker_count;      /* 실제 워커 수 */
    fl_worker_t  workers[FL_CLUSTER_MAX_WORKERS];

    bool         is_master;         /* 현재 프로세스가 master인지 */
    int          worker_id;         /* -1 = master, 0+ = worker ID */

    /* 콜백: worker 프로세스에서 실행할 함수 */
    void (*worker_main)(int worker_id, void* userdata);
    void* userdata;
} fl_cluster_t;

/* ===== 공개 API ===== */

/* CPU 코어 수 반환 (auto 모드용) */
int fl_cluster_cpu_count(void);

/* 클러스터 초기화 및 시작
 * n=0 이면 자동 (CPU 코어 수)
 * worker_fn: 각 worker 프로세스에서 실행될 함수
 * 반환값: master면 fl_cluster_t*, worker면 NULL (worker_fn 호출 후 종료)
 */
fl_cluster_t* fl_cluster_spawn(int n,
                                void (*worker_fn)(int id, void* data),
                                void* userdata);

/* Master: Worker 재시작 (크래시 복구) */
int fl_cluster_respawn_worker(fl_cluster_t* cluster, int worker_id);

/* Master: 모든 Worker에게 IPC 메시지 전송 */
int fl_cluster_broadcast(fl_cluster_t* cluster, fl_ipc_msg_t msg);

/* Master: SIGCHLD 핸들러 + Phoenix-Spawn 루프 */
void fl_cluster_run_master(fl_cluster_t* cluster);

/* 정리 */
void fl_cluster_destroy(fl_cluster_t* cluster);

#endif /* FL_CLUSTER_H */
