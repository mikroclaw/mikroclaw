#ifndef MIKROCLAW_WORKER_POOL_H
#define MIKROCLAW_WORKER_POOL_H

struct worker_pool_ctx;

struct worker_pool_ctx *worker_pool_init(int max_workers);
void worker_pool_destroy(struct worker_pool_ctx *ctx);
int worker_pool_max(const struct worker_pool_ctx *ctx);

#endif
