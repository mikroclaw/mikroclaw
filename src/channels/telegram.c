/*
 * MikroClaw - Telegram Bot Channel Implementation
 */

#include "telegram.h"
#include "allowlist.h"
#include "../http.h"
#include "../json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define TELEGRAM_API_HOST "api.telegram.org"

struct telegram_ctx {
    struct http_client *http;
    char bot_token[128];
    long last_update_id;
};

struct telegram_ctx *telegram_init(const struct telegram_config *config) {
    if (!config || !config->bot_token[0]) return NULL;
    
    struct telegram_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    strncpy(ctx->bot_token, config->bot_token, sizeof(ctx->bot_token) - 1);
    
    ctx->http = http_client_create(TELEGRAM_API_HOST, 443, 1);
    if (!ctx->http) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void telegram_destroy(struct telegram_ctx *ctx) {
    if (!ctx) return;
    http_client_destroy(ctx->http);
    free(ctx);
}

int telegram_poll(struct telegram_ctx *ctx, struct telegram_message *msg) {
    if (!ctx || !msg) return -1;
    
    char path[1024];
    snprintf(path, sizeof(path), "/bot%s/getUpdates?offset=%ld&limit=1",
             ctx->bot_token, ctx->last_update_id + 1);
    
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    int ret = http_get(ctx->http, path, NULL, 0, &resp);
    if (ret != 0 || resp.status_code != 200) {
        http_response_clear(&resp);
        return -1;
    }
    
    ret = telegram_parse_message(resp.body, msg);
    if (ret == 1) {
        ctx->last_update_id = msg->update_id;
    }

    http_response_clear(&resp);
    return ret;
}

int telegram_send(struct telegram_ctx *ctx, const char *chat_id,
                  const char *message) {
    if (!ctx || !chat_id || !message) return -1;
    
    char path[1024];
    snprintf(path, sizeof(path), "/bot%s/sendMessage", ctx->bot_token);
    
    char body[TELEGRAM_MAX_MESSAGE * 2];
    if (telegram_build_send_body(chat_id, message, body, sizeof(body)) != 0) {
        return -1;
    }
    
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    struct http_header headers[1] = {
        {"Content-Type", "application/json"}
    };
    
    int ret = http_post(ctx->http, path, headers, 1,
                        body, strlen(body), &resp);
    
    http_response_clear(&resp);
    return ret;
}

int telegram_build_send_body(const char *chat_id, const char *message,
                             char *body, size_t body_len) {
    char esc_chat[128];
    char esc_message[TELEGRAM_MAX_MESSAGE * 2];

    if (!chat_id || !message || !body || body_len == 0) {
        return -1;
    }
    if (json_escape(chat_id, esc_chat, sizeof(esc_chat)) != 0) {
        return -1;
    }
    if (json_escape(message, esc_message, sizeof(esc_message)) != 0) {
        return -1;
    }
    if (snprintf(body, body_len, "{\"chat_id\":\"%s\",\"text\":\"%s\"}",
                 esc_chat, esc_message) < 0) {
        return -1;
    }
    return 0;
}

int telegram_parse_message(const char *json_response, struct telegram_message *msg) {
    const char *p;
    const char *start;
    const char *end;
    size_t len;

    if (!json_response || !msg) {
        return -1;
    }

    if (strstr(json_response, "\"result\":[]") != NULL) {
        return 0;
    }

    p = strstr(json_response, "\"update_id\":");
    if (!p) {
        return 0;
    }
    p += strlen("\"update_id\":");
    msg->update_id = atoi(p);

    p = strstr(json_response, "\"chat\":{\"id\":");
    if (!p) {
        return 0;
    }
    p += strlen("\"chat\":{\"id\":");
    end = p;
    while (*end >= '0' && *end <= '9') {
        end++;
    }
    len = (size_t)(end - p);
    if (len == 0 || len >= sizeof(msg->chat_id)) {
        return 0;
    }
    memcpy(msg->chat_id, p, len);
    msg->chat_id[len] = '\0';

    start = strstr(json_response, "\"text\":\"");
    if (!start) {
        return 0;
    }
    start += strlen("\"text\":\"");
    end = start;
    while (*end != '\0') {
        if (*end == '"' && (end == start || end[-1] != '\\')) {
            break;
        }
        end++;
    }
    len = (size_t)(end - start);
    if (len == 0 || len >= sizeof(msg->text)) {
        return 0;
    }
    memcpy(msg->text, start, len);
    msg->text[len] = '\0';

    start = strstr(json_response, "\"username\":\"");
    if (start) {
        start += strlen("\"username\":\"");
        end = start;
        while (*end != '\0') {
            if (*end == '"' && (end == start || end[-1] != '\\')) {
                break;
            }
            end++;
        }
        len = (size_t)(end - start);
        if (len < sizeof(msg->sender)) {
            memcpy(msg->sender, start, len);
            msg->sender[len] = '\0';
        }
    }

    if (msg->chat_id[0] == '\0' || msg->text[0] == '\0') {
        return 0;
    }

    {
        const char *allowlist = getenv("TELEGRAM_ALLOWLIST");
        if (!sender_allowed(allowlist, msg->sender)) {
            return 0;
        }
    }

    return 1;
}

int telegram_health_check(struct telegram_ctx *ctx) {
    char path[512];
    struct http_response resp;
    int ret;

    if (!ctx || !ctx->http || ctx->bot_token[0] == '\0') {
        return 0;
    }

    snprintf(path, sizeof(path), "/bot%s/getMe", ctx->bot_token);
    memset(&resp, 0, sizeof(resp));
    ret = http_get(ctx->http, path, NULL, 0, &resp);
    if (ret != 0) {
        http_response_clear(&resp);
        return 0;
    }
    ret = (resp.status_code == 200) ? 1 : 0;
    http_response_clear(&resp);
    return ret;
}
