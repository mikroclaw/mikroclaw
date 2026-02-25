/*
 * MikroClaw - LLM API Client Implementation
 */

#include "llm.h"
#include "http.h"
#include "json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct llm_ctx *llm_init(struct llm_config *config) {
    if (!config) return NULL;
    
    struct llm_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    ctx->config = *config;
    
    /* Parse hostname from URL */
    char hostname[256];
    bool use_tls = (strncmp(config->base_url, "https", 5) == 0);
    int port = use_tls ? 443 : 80;
    
    const char *proto_end = strstr(config->base_url, "://");
    if (!proto_end) {
        free(ctx);
        return NULL;
    }
    proto_end += 3;
    
    const char *path_start = strchr(proto_end, '/');
    if (path_start) {
        size_t host_len = path_start - proto_end;
        strncpy(hostname, proto_end, host_len);
        hostname[host_len] = '\0';
    } else {
        strncpy(hostname, proto_end, sizeof(hostname) - 1);
    }
    
    ctx->http = http_client_create(hostname, port, use_tls);
    if (!ctx->http) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void llm_destroy(struct llm_ctx *ctx) {
    if (!ctx) return;
    http_client_destroy(ctx->http);
    free(ctx);
}

int llm_chat(struct llm_ctx *ctx,
             const char *system_prompt,
             const char *user_message,
             char *response, size_t max_response) {
    
    if (!ctx || !user_message || !response) return -1;
    
    /* Escape input strings for safe JSON inclusion */
    char escaped_system[4096];
    char escaped_user[4096];
    
    if (system_prompt && *system_prompt) {
        if (json_escape(system_prompt, escaped_system, sizeof(escaped_system)) != 0) {
            return -1;
        }
    } else {
        escaped_system[0] = '\0';
    }
    
    if (json_escape(user_message, escaped_user, sizeof(escaped_user)) != 0) {
        return -1;
    }
    
    /* Build request body */
    char body[8192];
    int body_len;
    
    if (escaped_system[0]) {
        body_len = snprintf(body, sizeof(body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":["
            "{\"role\":\"system\",\"content\":\"%s\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
            "],"
            "\"temperature\":%.1f,"
            "\"max_tokens\":%d"
            "}",
            ctx->config.model,
            escaped_system,
            escaped_user,
            ctx->config.temperature,
            ctx->config.max_tokens);
    } else {
        body_len = snprintf(body, sizeof(body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":["
            "{\"role\":\"user\",\"content\":\"%s\"}"
            "],"
            "\"temperature\":%.1f,"
            "\"max_tokens\":%d"
            "}",
            ctx->config.model,
            escaped_user,
            ctx->config.temperature,
            ctx->config.max_tokens);
    }
    
    struct http_header headers[2];
    memset(headers, 0, sizeof(headers));
    strncpy(headers[0].name, "Content-Type", sizeof(headers[0].name) - 1);
    strncpy(headers[0].value, "application/json", sizeof(headers[0].value) - 1);
    if (ctx->config.auth_style == PROVIDER_AUTH_API_KEY) {
        strncpy(headers[1].name, "x-api-key", sizeof(headers[1].name) - 1);
        strncpy(headers[1].value, ctx->config.api_key, sizeof(headers[1].value) - 1);
    } else {
        char auth_value[HTTP_MAX_HEADER_VALUE];
        strncpy(headers[1].name, "Authorization", sizeof(headers[1].name) - 1);
        snprintf(auth_value, sizeof(auth_value), "Bearer %s", ctx->config.api_key);
        strncpy(headers[1].value, auth_value, sizeof(headers[1].value) - 1);
    }
    
    /* Send request */
    struct http_response resp;
    memset(&resp, 0, sizeof(resp));
    
    int ret = http_post(ctx->http, "/v1/chat/completions", headers, 2, 
                        body, body_len, &resp);
    
    if (ret != 0 || resp.status_code != 200) {
        http_response_clear(&resp);
        return -1;
    }
    
    /* Parse response */
    struct json_ctx json;
    json_init(&json);
    if (json_parse(&json, resp.body, resp.body_len) < 0) {
        http_response_clear(&resp);
        return -1;
    }
    
    /* Extract content */
    const char *content = json_get_string(&json, "content", NULL);
    if (content) {
        strncpy(response, content, max_response - 1);
        response[max_response - 1] = '\0';
    } else {
        response[0] = '\0';
    }
    
    http_response_clear(&resp);
    return 0;
}

int llm_chat_stream(struct llm_ctx *ctx,
                    const char *system_prompt,
                    const char *user_message,
                    llm_stream_chunk_cb cb,
                    void *user_data,
                    char *response, size_t max_response) {
    const char *streaming = getenv("LLM_STREAMING");

    if (!ctx || !cb || !response || max_response == 0) {
        return -1;
    }

    if (llm_chat(ctx, system_prompt, user_message, response, max_response) != 0) {
        return -1;
    }

    if (streaming && streaming[0] == '1' && strstr(response, "data:") != NULL) {
        if (llm_sse_for_each_chunk(response, cb, user_data) != 0) {
            return -1;
        }
        return llm_sse_extract_text(response, response, max_response);
    }

    return cb(response, user_data);
}

int llm_chat_reliable(struct llm_ctx *ctx,
                      const char *system_prompt,
                      const char *user_message,
                      char *response, size_t max_response) {
    const char *providers;
    char list[512];
    char *tok;

    if (!ctx || !user_message || !response) {
        return -1;
    }

    if (llm_chat(ctx, system_prompt, user_message, response, max_response) == 0) {
        return 0;
    }

    providers = getenv("RELIABLE_PROVIDERS");
    if (!providers || providers[0] == '\0') {
        return -1;
    }

    snprintf(list, sizeof(list), "%s", providers);
    tok = strtok(list, ",");
    while (tok) {
        struct provider_config provider;
        struct llm_config cfg = ctx->config;
        struct llm_ctx *fallback;
        const char *key;

        while (*tok == ' ' || *tok == '\t') {
            tok++;
        }
        if (provider_registry_get(tok, &provider) == 0) {
            key = getenv(provider.api_key_env_var);
            if (key && key[0] != '\0') {
                snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", provider.base_url);
                snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", key);
                cfg.auth_style = provider.auth_style;
                fallback = llm_init(&cfg);
                if (fallback) {
                    int ok = llm_chat(fallback, system_prompt, user_message, response, max_response);
                    llm_destroy(fallback);
                    if (ok == 0) {
                        return 0;
                    }
                }
            }
        }

        tok = strtok(NULL, ",");
    }

    return -1;
}
