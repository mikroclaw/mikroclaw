#include "rate_limit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RL_MAX_CLIENTS 128

struct rl_entry {
    char ip[64];
    time_t window_start;
    int request_count;
    int failed_auth_count;
    time_t locked_until;
    int in_use;
};

struct rate_limit_ctx {
    int max_requests;
    int window_seconds;
    int lockout_seconds;
    struct rl_entry entries[RL_MAX_CLIENTS];
};

static struct rl_entry *lookup_entry(struct rate_limit_ctx *ctx, const char *ip) {
    int i;
    int free_idx = -1;

    if (!ctx || !ip || ip[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < RL_MAX_CLIENTS; i++) {
        if (ctx->entries[i].in_use) {
            if (strcmp(ctx->entries[i].ip, ip) == 0) {
                return &ctx->entries[i];
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    if (free_idx >= 0) {
        struct rl_entry *e = &ctx->entries[free_idx];
        memset(e, 0, sizeof(*e));
        snprintf(e->ip, sizeof(e->ip), "%s", ip);
        e->window_start = time(NULL);
        e->in_use = 1;
        return e;
    }
    return NULL;
}

struct rate_limit_ctx *rate_limit_init(int max_requests, int window_seconds, int lockout_seconds) {
    struct rate_limit_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->max_requests = max_requests > 0 ? max_requests : 10;
    ctx->window_seconds = window_seconds > 0 ? window_seconds : 60;
    ctx->lockout_seconds = lockout_seconds > 0 ? lockout_seconds : 60;
    return ctx;
}

void rate_limit_destroy(struct rate_limit_ctx *ctx) {
    free(ctx);
}

int rate_limit_allow_request(struct rate_limit_ctx *ctx, const char *ip) {
    struct rl_entry *e;
    time_t now = time(NULL);

    if (!ctx || !ip) {
        return 1;
    }
    e = lookup_entry(ctx, ip);
    if (!e) {
        return 0;
    }

    if (e->locked_until > now) {
        return 0;
    }

    if (now - e->window_start >= ctx->window_seconds) {
        e->window_start = now;
        e->request_count = 0;
    }

    e->request_count++;
    return e->request_count <= ctx->max_requests;
}

int rate_limit_record_auth_failure(struct rate_limit_ctx *ctx, const char *ip) {
    struct rl_entry *e;
    time_t now = time(NULL);
    int backoff;

    if (!ctx || !ip) {
        return -1;
    }
    e = lookup_entry(ctx, ip);
    if (!e) {
        return -1;
    }

    e->failed_auth_count++;
    if (e->failed_auth_count >= 5) {
        backoff = e->failed_auth_count - 4;
        if (backoff > 5) {
            backoff = 5;
        }
        e->locked_until = now + (ctx->lockout_seconds * backoff);
    }
    return 0;
}

void rate_limit_record_auth_success(struct rate_limit_ctx *ctx, const char *ip) {
    struct rl_entry *e;
    if (!ctx || !ip) {
        return;
    }
    e = lookup_entry(ctx, ip);
    if (!e) {
        return;
    }
    e->failed_auth_count = 0;
    e->locked_until = 0;
}

int rate_limit_is_locked(struct rate_limit_ctx *ctx, const char *ip, int *retry_after_seconds) {
    struct rl_entry *e;
    time_t now = time(NULL);

    if (retry_after_seconds) {
        *retry_after_seconds = 0;
    }
    if (!ctx || !ip) {
        return 0;
    }
    e = lookup_entry(ctx, ip);
    if (!e) {
        return 0;
    }

    if (e->locked_until > now) {
        if (retry_after_seconds) {
            *retry_after_seconds = (int)(e->locked_until - now);
        }
        return 1;
    }
    return 0;
}
