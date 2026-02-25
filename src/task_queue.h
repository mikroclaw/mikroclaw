#ifndef MIKROCLAW_TASK_QUEUE_H
#define MIKROCLAW_TASK_QUEUE_H

#include <sys/types.h>
#include <time.h>

#define TASK_ID_MAX 32
#define TASK_TYPE_MAX 32
#define TASK_PARAMS_MAX 1024
#define TASK_RESULT_MAX 4096

enum task_status {
    TASK_QUEUED = 0,
    TASK_RUNNING = 1,
    TASK_COMPLETE = 2,
    TASK_FAILED = 3,
    TASK_CANCELLED = 4,
};

struct task_item {
    char id[TASK_ID_MAX];
    char type[TASK_TYPE_MAX];
    char params[TASK_PARAMS_MAX];
    enum task_status status;
    char result[TASK_RESULT_MAX];
    pid_t worker_pid;
    time_t created_at;
    time_t completed_at;
    int in_use;
};

struct task_queue_ctx;

struct task_queue_ctx *task_queue_init(int max_tasks);
void task_queue_destroy(struct task_queue_ctx *ctx);
int task_queue_add(struct task_queue_ctx *ctx, const char *type, const char *params,
                   char *task_id_out, size_t task_id_out_len);
struct task_item *task_queue_get(struct task_queue_ctx *ctx, const char *task_id);
struct task_item *task_queue_find_by_pid(struct task_queue_ctx *ctx, pid_t pid);
struct task_item *task_queue_next_queued(struct task_queue_ctx *ctx);
int task_queue_count_running(struct task_queue_ctx *ctx);
void task_queue_cleanup(struct task_queue_ctx *ctx, int ttl_seconds);
const char *task_status_name(enum task_status status);
int task_queue_list_json(struct task_queue_ctx *ctx, char *out, size_t out_len);
int task_queue_capacity(struct task_queue_ctx *ctx);
struct task_item *task_queue_item_at(struct task_queue_ctx *ctx, int index);

#endif
