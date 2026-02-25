#ifndef MIKROCLAW_SUBAGENT_H
#define MIKROCLAW_SUBAGENT_H

#include <stddef.h>

struct subagent_ctx;

struct subagent_ctx *subagent_init(int max_workers, int max_tasks);
void subagent_destroy(struct subagent_ctx *ctx);
int subagent_submit(struct subagent_ctx *ctx, const char *type, const char *params,
                    char *task_id_out, size_t task_id_out_len);
void subagent_poll(struct subagent_ctx *ctx);
int subagent_get_json(struct subagent_ctx *ctx, const char *task_id,
                      char *out, size_t out_len);
int subagent_list_json(struct subagent_ctx *ctx, char *out, size_t out_len);
int subagent_cancel(struct subagent_ctx *ctx, const char *task_id);

#endif
