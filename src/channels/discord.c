#include "discord.h"
#include "allowlist.h"

#include "../http_client.h"
#include "../json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *http_body_ptr(const char *http_request) {
    const char *body;

    if (!http_request) {
        return NULL;
    }
    body = strstr(http_request, "\r\n\r\n");
    if (body) {
        return body + 4;
    }
    return http_request;
}

static int extract_json_string_field(const char *json, const char *field,
                                     char *out, size_t out_len) {
    char pattern[64];
    const char *p;
    const char *start;
    const char *end;
    size_t n;

    if (!json || !field || !out || out_len == 0) {
        return 0;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", field);
    p = strstr(json, pattern);
    if (!p) {
        return 0;
    }
    start = p + strlen(pattern);
    end = start;
    while (*end != '\0') {
        if (*end == '"' && (end == start || end[-1] != '\\')) {
            break;
        }
        end++;
    }
    if (*end == '\0') {
        return 0;
    }

    n = (size_t)(end - start);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return n > 0;
}

struct discord_ctx *discord_init(const struct discord_config *config) {
    struct discord_ctx *ctx;

    if (!config || config->webhook_url[0] == '\0') {
        return NULL;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(ctx->config));
    return ctx;
}

void discord_destroy(struct discord_ctx *ctx) {
    free(ctx);
}

int discord_send(struct discord_ctx *ctx, const char *message) {
    curl_http_client *http;
    curl_http_response response;
    char escaped[4096];
    char body[4352];

    if (!ctx || !message) {
        return -1;
    }

    if (json_escape(message, escaped, sizeof(escaped)) != 0) {
        return -1;
    }

    snprintf(body, sizeof(body), "{\"content\":\"%s\"}", escaped);

    http = curl_http_client_create();
    if (!http) {
        return -1;
    }

    if (curl_http_post(http, ctx->config.webhook_url, body, &response) != 0) {
        curl_http_client_destroy(http);
        return -1;
    }

    curl_http_response_free(&response);
    curl_http_client_destroy(http);
    return 0;
}

int discord_parse_inbound(const char *http_request, char *out_text, size_t out_len) {
    const char *body = http_body_ptr(http_request);
    char sender[128];
    const char *allowlist;

    if (!body || !out_text || out_len == 0) {
        return 0;
    }
    if (strstr(body, "\"bot\":true") != NULL) {
        return 0;
    }

    allowlist = getenv("DISCORD_ALLOWLIST");
    if (allowlist) {
        if (!extract_json_string_field(body, "username", sender, sizeof(sender))) {
            return 0;
        }
        if (!sender_allowed(allowlist, sender)) {
            return 0;
        }
    }

    return extract_json_string_field(body, "content", out_text, out_len) ? 1 : 0;
}

int discord_health_check(struct discord_ctx *ctx) {
    return (ctx && ctx->config.webhook_url[0] != '\0') ? 1 : 0;
}
