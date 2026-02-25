#include "subagent.h"

#include "task_handlers.h"
#include "task_queue.h"
#include "worker_pool.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct subagent_ctx {
    struct worker_pool_ctx *pool;
    struct task_queue_ctx *queue;
};

static void task_result_path(const char *task_id, char *out, size_t out_len) {
    snprintf(out, out_len, "/tmp/mikroclaw-%s.out", task_id ? task_id : "unknown");
}

static void spawn_task(struct task_item *task) {
    pid_t pid;
    task_handler_fn fn;

    if (!task) {
        return;
    }
    fn = task_handler_resolve(task->type);
    if (!fn) {
        task->status = TASK_FAILED;
        snprintf(task->result, sizeof(task->result), "unknown task type: %s", task->type);
        task->completed_at = time(NULL);
        return;
    }

    pid = fork();
    if (pid < 0) {
        task->status = TASK_FAILED;
        snprintf(task->result, sizeof(task->result), "fork failed");
        task->completed_at = time(NULL);
        return;
    }

    if (pid == 0) {
        char out[TASK_RESULT_MAX] = {0};
        char path[128];
        FILE *fp;
        int rc = fn(task->params, out, sizeof(out));
        task_result_path(task->id, path, sizeof(path));
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "%d\n%s", rc, out);
            fclose(fp);
        }
        _exit(rc == 0 ? 0 : 1);
    }

    task->worker_pid = pid;
    task->status = TASK_RUNNING;
}

struct subagent_ctx *subagent_init(int max_workers, int max_tasks) {
    struct subagent_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->pool = worker_pool_init(max_workers);
    ctx->queue = task_queue_init(max_tasks);
    if (!ctx->pool || !ctx->queue) {
        subagent_destroy(ctx);
        return NULL;
    }
    return ctx;
}

void subagent_destroy(struct subagent_ctx *ctx) {
    int i;
    if (!ctx) {
        return;
    }

    if (ctx->queue) {
        int cap = task_queue_capacity(ctx->queue);
        for (i = 0; i < cap; i++) {
            struct task_item *item = task_queue_item_at(ctx->queue, i);
            if (!item || !item->in_use) {
                continue;
            }
            if (item->status == TASK_RUNNING && item->worker_pid > 0) {
                kill(item->worker_pid, SIGTERM);
            }
        }
    }

    task_queue_destroy(ctx->queue);
    worker_pool_destroy(ctx->pool);
    free(ctx);
}

int subagent_submit(struct subagent_ctx *ctx, const char *type, const char *params,
                    char *task_id_out, size_t task_id_out_len) {
    if (!ctx || !ctx->queue) {
        return -1;
    }
    return task_queue_add(ctx->queue, type, params, task_id_out, task_id_out_len);
}

void subagent_poll(struct subagent_ctx *ctx) {
    int running;
    pid_t pid;
    int status;

    if (!ctx || !ctx->queue || !ctx->pool) {
        return;
    }

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct task_item *task = task_queue_find_by_pid(ctx->queue, pid);
        if (task && task->status == TASK_RUNNING) {
            char path[128];
            FILE *fp;
            int rc = 1;
            char line[TASK_RESULT_MAX];
            task_result_path(task->id, path, sizeof(path));
            fp = fopen(path, "r");
            if (fp) {
                if (fgets(line, sizeof(line), fp)) {
                    rc = atoi(line);
                }
                if (fgets(task->result, sizeof(task->result), fp) == NULL) {
                    task->result[0] = '\0';
                }
                fclose(fp);
                unlink(path);
            }
            task->status = (WIFEXITED(status) && WEXITSTATUS(status) == 0 && rc == 0)
                               ? TASK_COMPLETE
                               : TASK_FAILED;
            task->completed_at = time(NULL);
            task->worker_pid = 0;
        }
    }

    running = task_queue_count_running(ctx->queue);
    while (running < worker_pool_max(ctx->pool)) {
        struct task_item *next = task_queue_next_queued(ctx->queue);
        if (!next) {
            break;
        }
        spawn_task(next);
        running = task_queue_count_running(ctx->queue);
    }

    task_queue_cleanup(ctx->queue, 300);
}

int subagent_get_json(struct subagent_ctx *ctx, const char *task_id,
                      char *out, size_t out_len) {
    struct task_item *task;
    if (!ctx || !ctx->queue || !out || out_len == 0) {
        return -1;
    }
    task = task_queue_get(ctx->queue, task_id);
    if (!task) {
        return -1;
    }
    snprintf(out, out_len,
             "{\"task_id\":\"%s\",\"status\":\"%s\",\"result\":\"%s\"}",
             task->id, task_status_name(task->status), task->result);
    return 0;
}

int subagent_list_json(struct subagent_ctx *ctx, char *out, size_t out_len) {
    if (!ctx || !ctx->queue || !out || out_len == 0) {
        return -1;
    }
    return task_queue_list_json(ctx->queue, out, out_len);
}

int subagent_cancel(struct subagent_ctx *ctx, const char *task_id) {
    struct task_item *task;
    if (!ctx || !ctx->queue || !task_id) {
        return -1;
    }
    task = task_queue_get(ctx->queue, task_id);
    if (!task) {
        return -1;
    }
    if (task->status == TASK_RUNNING && task->worker_pid > 0) {
        kill(task->worker_pid, SIGTERM);
    }
    task->status = TASK_CANCELLED;
    task->completed_at = time(NULL);
    return 0;
}
