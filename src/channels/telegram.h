/*
 * MikroClaw - Telegram Bot Channel Header
 */

#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <stddef.h>

#define TELEGRAM_MAX_MESSAGE 4096

struct telegram_ctx;
struct telegram_config {
    char bot_token[128];
};

struct telegram_message {
    char sender[256];
    char chat_id[64];
    char text[4096];
    int update_id;
};

struct telegram_ctx *telegram_init(const struct telegram_config *config);
void telegram_destroy(struct telegram_ctx *ctx);
int telegram_poll(struct telegram_ctx *ctx, struct telegram_message *msg);
int telegram_parse_message(const char *json_response, struct telegram_message *msg);
int telegram_build_send_body(const char *chat_id, const char *message,
                             char *body, size_t body_len);
int telegram_send(struct telegram_ctx *ctx, const char *chat_id, 
                  const char *message);
int telegram_health_check(struct telegram_ctx *ctx);

#endif /* TELEGRAM_H */
