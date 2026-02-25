#ifndef MIKROCLAW_RATE_LIMIT_H
#define MIKROCLAW_RATE_LIMIT_H

#include <stddef.h>

struct rate_limit_ctx;

struct rate_limit_ctx *rate_limit_init(int max_requests, int window_seconds, int lockout_seconds);
void rate_limit_destroy(struct rate_limit_ctx *ctx);
int rate_limit_allow_request(struct rate_limit_ctx *ctx, const char *ip);
int rate_limit_record_auth_failure(struct rate_limit_ctx *ctx, const char *ip);
void rate_limit_record_auth_success(struct rate_limit_ctx *ctx, const char *ip);
int rate_limit_is_locked(struct rate_limit_ctx *ctx, const char *ip, int *retry_after_seconds);

#endif
