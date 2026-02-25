/*
 * MikroClaw - RouterOS REST API Client Implementation
 */

#include "routeros.h"
#include "http.h"
#include "base64.h"
#include "mikroclaw.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct routeros_ctx {
    struct http_client *http;
    char host[256];
    char user[64];
    char pass[128];
    char auth_header[256];
};

static void build_auth_header(struct routeros_ctx *ctx) {
    char creds[256];
    char encoded[256];
    if (snprintf(creds, sizeof(creds), "%s:%s", ctx->user, ctx->pass) < 0) {
        ctx->auth_header[0] = '\0';
        return;
    }
    if (base64_encode((const unsigned char *)creds, strlen(creds), encoded, sizeof(encoded)) != 0) {
        ctx->auth_header[0] = '\0';
        return;
    }
    if (snprintf(ctx->auth_header, sizeof(ctx->auth_header), "Basic %s", encoded) < 0) {
        ctx->auth_header[0] = '\0';
    }
}

struct routeros_ctx *routeros_init(const char *host, int port,
                                   const char *user, const char *pass) {
    if (!host || !user || !pass) return NULL;
    
    struct routeros_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    strncpy(ctx->user, user, sizeof(ctx->user) - 1);
    strncpy(ctx->pass, pass, sizeof(ctx->pass) - 1);
    
    build_auth_header(ctx);
    
    ctx->http = http_client_create(host, port, true);
    if (!ctx->http) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void routeros_destroy(struct routeros_ctx *ctx) {
    if (!ctx) return;
    http_client_destroy(ctx->http);
    free(ctx);
}

int routeros_execute(struct routeros_ctx *ctx, const char *command,
                     char *output, size_t max_output) {
    if (!ctx || !command || !output) return -1;
    
    char escaped[8192];
    if (json_escape(command, escaped, sizeof(escaped)) != 0) {
        return -1;
    }
    
    /* Build request body */
    char body[4096];
    const char *prefix = "{\"script\":\"";
    const char *suffix = "\"}";
    size_t escaped_len = strlen(escaped);
    size_t body_needed = strlen(prefix) + escaped_len + strlen(suffix) + 1;
    if (body_needed > sizeof(body)) {
        return -1;
    }
    memcpy(body, prefix, strlen(prefix));
    memcpy(body + strlen(prefix), escaped, escaped_len);
    memcpy(body + strlen(prefix) + escaped_len, suffix, strlen(suffix));
    body[body_needed - 1] = '\0';
    
    /* Send request */
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    struct http_header headers[2] = {
        {"Content-Type", "application/json"},
        {"Authorization", ""}
    };
    strncpy(headers[1].value, ctx->auth_header, sizeof(headers[1].value) - 1);
    
    int ret = http_post(ctx->http, "/rest/execute", headers, 2,
                        body, strlen(body), &resp);
    
    if (ret != 0) {
        http_response_clear(&resp);
        return -1;
    }
    
    /* Copy output */
    strncpy(output, resp.body, max_output - 1);
    output[max_output - 1] = '\0';
    
    http_response_clear(&resp);
    return 0;
}

int routeros_get(struct routeros_ctx *ctx, const char *path,
                 char *output, size_t max_output) {
    if (!ctx || !path || !output) return -1;
    
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    struct http_header headers[2] = {
        {"Accept", "application/json"},
        {"Authorization", ""}
    };
    strncpy(headers[1].value, ctx->auth_header, sizeof(headers[1].value) - 1);
    
    int ret = http_get(ctx->http, path, headers, 2, &resp);
    
    if (ret != 0) {
        http_response_clear(&resp);
        return -1;
    }
    
    strncpy(output, resp.body, max_output - 1);
    output[max_output - 1] = '\0';
    
    http_response_clear(&resp);
    return 0;
}

int routeros_post(struct routeros_ctx *ctx, const char *path,
                  const char *data, char *output, size_t max_output) {
    if (!ctx || !path || !data || !output) return -1;
    
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    struct http_header headers[2] = {
        {"Content-Type", "application/json"},
        {"Authorization", ""}
    };
    strncpy(headers[1].value, ctx->auth_header, sizeof(headers[1].value) - 1);
    
    int ret = http_post(ctx->http, path, headers, 2,
                        data, strlen(data), &resp);
    
    if (ret != 0) {
        http_response_clear(&resp);
        return -1;
    }
    
    strncpy(output, resp.body, max_output - 1);
    output[max_output - 1] = '\0';
    
    http_response_clear(&resp);
    return 0;
}

int routeros_firewall_allow_subnets(struct routeros_ctx *ctx, const char *comment,
                                    const char *subnets_csv, int port) {
    char body[1024];
    char out[2048];
    char esc_comment[256];
    char esc_subnets[256];
    const char *c = (comment && comment[0]) ? comment : "mikroclaw-auto";
    const char *s = (subnets_csv && subnets_csv[0]) ? subnets_csv : "10.0.0.0/8,172.16.0.0/12,192.168.0.0/16";

    if (!ctx) {
        return -1;
    }

    if (json_escape(c, esc_comment, sizeof(esc_comment)) != 0) {
        return -1;
    }
    if (json_escape(s, esc_subnets, sizeof(esc_subnets)) != 0) {
        return -1;
    }

    snprintf(body, sizeof(body),
             "{\"chain\":\"input\",\"action\":\"accept\",\"protocol\":\"tcp\",\"dst-port\":\"%d\",\"src-address-list\":\"%s\",\"comment\":\"%s\"}",
             port, esc_subnets, esc_comment);
    return routeros_post(ctx, "/rest/ip/firewall/filter/add", body, out, sizeof(out));
}

int routeros_firewall_remove_comment(struct routeros_ctx *ctx, const char *comment) {
    char body[256];
    char out[2048];
    const char *c = (comment && comment[0]) ? comment : "mikroclaw-auto";

    if (!ctx) {
        return -1;
    }

    snprintf(body, sizeof(body), "{\"comment\":\"%s\"}", c);
    return routeros_post(ctx, "/rest/ip/firewall/filter/remove", body, out, sizeof(out));
}

int routeros_script_run_inline(struct routeros_ctx *ctx, const char *script, char *output, size_t max_output) {
    char body[4096];
    char esc[3072];

    if (!ctx || !script || !output || max_output == 0) {
        return -1;
    }
    if (json_escape(script, esc, sizeof(esc)) != 0) {
        return -1;
    }
    snprintf(body, sizeof(body), "{\"script\":\"%s\"}", esc);
    return routeros_post(ctx, "/rest/system/script/run", body, output, max_output);
}

int routeros_scheduler_add(struct routeros_ctx *ctx, const char *name,
                           const char *interval, const char *on_event,
                           char *output, size_t max_output) {
    char body[4096];
    char esc_event[3072];

    if (!ctx || !name || !interval || !on_event || !output || max_output == 0) {
        return -1;
    }
    if (json_escape(on_event, esc_event, sizeof(esc_event)) != 0) {
        return -1;
    }
    snprintf(body, sizeof(body),
             "{\"name\":\"%s\",\"interval\":\"%s\",\"on-event\":\"%s\",\"disabled\":\"false\"}",
             name, interval, esc_event);
    return routeros_post(ctx, "/rest/system/scheduler/add", body, output, max_output);
}

int routeros_scheduler_remove(struct routeros_ctx *ctx, const char *name,
                              char *output, size_t max_output) {
    char body[256];

    if (!ctx || !name || !output || max_output == 0) {
        return -1;
    }
    snprintf(body, sizeof(body), "{\"name\":\"%s\"}", name);
    return routeros_post(ctx, "/rest/system/scheduler/remove", body, output, max_output);
}
