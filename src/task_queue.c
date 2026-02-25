#include "task_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct task_queue_ctx {
    struct task_item *items;
    int max_tasks;
    unsigned int seq;
};

static const char *status_to_string(enum task_status status) {
    switch (status) {
        case TASK_QUEUED: return "queued";
        case TASK_RUNNING: return "running";
        case TASK_COMPLETE: return "complete";
        case TASK_FAILED: return "failed";
        case TASK_CANCELLED: return "cancelled";
        default: return "failed";
    }
}

struct task_queue_ctx *task_queue_init(int max_tasks) {
    struct task_queue_ctx *ctx;
    if (max_tasks <= 0) {
        max_tasks = 100;
    }
    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->items = calloc((size_t)max_tasks, sizeof(*ctx->items));
    if (!ctx->items) {
        free(ctx);
        return NULL;
    }
    ctx->max_tasks = max_tasks;
    return ctx;
}

void task_queue_destroy(struct task_queue_ctx *ctx) {
    if (!ctx) {
        return;
    }
    free(ctx->items);
    free(ctx);
}

int task_queue_add(struct task_queue_ctx *ctx, const char *type, const char *params,
                   char *task_id_out, size_t task_id_out_len) {
    int i;
    struct task_item *item;

    if (!ctx || !type || type[0] == '\0' || !task_id_out || task_id_out_len == 0) {
        return -1;
    }

    for (i = 0; i < ctx->max_tasks; i++) {
        if (!ctx->items[i].in_use) {
            item = &ctx->items[i];
            memset(item, 0, sizeof(*item));
            item->in_use = 1;
            item->status = TASK_QUEUED;
            item->created_at = time(NULL);
            snprintf(item->type, sizeof(item->type), "%s", type);
            snprintf(item->params, sizeof(item->params), "%s", params ? params : "{}");
            snprintf(item->id, sizeof(item->id), "task_%u_%d", ++ctx->seq, (int)getpid());
            snprintf(task_id_out, task_id_out_len, "%s", item->id);
            return 0;
        }
    }
    return -1;
}

struct task_item *task_queue_get(struct task_queue_ctx *ctx, const char *task_id) {
    int i;
    if (!ctx || !task_id) {
        return NULL;
    }
    for (i = 0; i < ctx->max_tasks; i++) {
        if (ctx->items[i].in_use && strcmp(ctx->items[i].id, task_id) == 0) {
            return &ctx->items[i];
        }
    }
    return NULL;
}

struct task_item *task_queue_find_by_pid(struct task_queue_ctx *ctx, pid_t pid) {
    int i;
    if (!ctx || pid <= 0) {
        return NULL;
    }
    for (i = 0; i < ctx->max_tasks; i++) {
        if (ctx->items[i].in_use && ctx->items[i].worker_pid == pid) {
            return &ctx->items[i];
        }
    }
    return NULL;
}

struct task_item *task_queue_next_queued(struct task_queue_ctx *ctx) {
    int i;
    if (!ctx) {
        return NULL;
    }
    for (i = 0; i < ctx->max_tasks; i++) {
        if (ctx->items[i].in_use && ctx->items[i].status == TASK_QUEUED) {
            return &ctx->items[i];
        }
    }
    return NULL;
}

int task_queue_count_running(struct task_queue_ctx *ctx) {
    int i;
    int count = 0;
    if (!ctx) {
        return 0;
    }
    for (i = 0; i < ctx->max_tasks; i++) {
        if (ctx->items[i].in_use && ctx->items[i].status == TASK_RUNNING) {
            count++;
        }
    }
    return count;
}

void task_queue_cleanup(struct task_queue_ctx *ctx, int ttl_seconds) {
    int i;
    time_t now = time(NULL);
    if (!ctx) {
        return;
    }
    if (ttl_seconds <= 0) {
        ttl_seconds = 300;
    }

    for (i = 0; i < ctx->max_tasks; i++) {
        struct task_item *it = &ctx->items[i];
        if (!it->in_use) {
            continue;
        }
        if ((it->status == TASK_COMPLETE || it->status == TASK_FAILED || it->status == TASK_CANCELLED) &&
            it->completed_at > 0 && (now - it->completed_at) > ttl_seconds) {
            memset(it, 0, sizeof(*it));
        }
    }
}

const char *task_status_name(enum task_status status) {
    return status_to_string(status);
}

int task_queue_list_json(struct task_queue_ctx *ctx, char *out, size_t out_len) {
    int i;
    size_t used = 0;

    if (!ctx || !out || out_len == 0) {
        return -1;
    }
    used += (size_t)snprintf(out + used, out_len - used, "[");
    for (i = 0; i < ctx->max_tasks; i++) {
        struct task_item *t = &ctx->items[i];
        if (!t->in_use) {
            continue;
        }
        if (used + 128 >= out_len) {
            break;
        }
        used += (size_t)snprintf(out + used, out_len - used,
                                 "%s{\"task_id\":\"%s\",\"status\":\"%s\"}",
                                 used > 1 ? "," : "", t->id,
                                 task_status_name(t->status));
    }
    snprintf(out + used, out_len - used, "]");
    return 0;
}

int task_queue_capacity(struct task_queue_ctx *ctx) {
    return ctx ? ctx->max_tasks : 0;
}

struct task_item *task_queue_item_at(struct task_queue_ctx *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->max_tasks) {
        return NULL;
    }
    return &ctx->items[index];
}
