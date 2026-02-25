#include "worker_pool.h"

#include <stdlib.h>

struct worker_pool_ctx {
    int max_workers;
};

struct worker_pool_ctx *worker_pool_init(int max_workers) {
    struct worker_pool_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->max_workers = max_workers > 0 ? max_workers : 4;
    return ctx;
}

void worker_pool_destroy(struct worker_pool_ctx *ctx) {
    free(ctx);
}

int worker_pool_max(const struct worker_pool_ctx *ctx) {
    return ctx ? ctx->max_workers : 0;
}
