#ifndef MIKROCLAW_DISCORD_H
#define MIKROCLAW_DISCORD_H

#include <stddef.h>

struct discord_config {
    char webhook_url[512];
};

struct discord_ctx {
    struct discord_config config;
};

struct discord_ctx *discord_init(const struct discord_config *config);
void discord_destroy(struct discord_ctx *ctx);
int discord_send(struct discord_ctx *ctx, const char *message);
int discord_parse_inbound(const char *http_request, char *out_text, size_t out_len);
int discord_health_check(struct discord_ctx *ctx);

#endif
