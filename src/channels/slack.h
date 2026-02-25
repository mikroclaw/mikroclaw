#ifndef MIKROCLAW_SLACK_H
#define MIKROCLAW_SLACK_H

#include <stddef.h>

struct slack_config {
    char webhook_url[512];
};

struct slack_ctx {
    struct slack_config config;
};

struct slack_ctx *slack_init(const struct slack_config *config);
void slack_destroy(struct slack_ctx *ctx);
int slack_send(struct slack_ctx *ctx, const char *message);
int slack_parse_inbound(const char *http_request, char *out_text, size_t out_len);
int slack_health_check(struct slack_ctx *ctx);

#endif
