/*
 * MikroClaw - JSON Utility Functions Implementation
 */

#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void json_init(struct json_ctx *ctx) {
    jsmn_init(&ctx->parser);
    ctx->data = NULL;
    ctx->num_tokens = 0;
}

int json_parse(struct json_ctx *ctx, const char *data, size_t len) {
    ctx->data = data;
    ctx->num_tokens = jsmn_parse(&ctx->parser, data, len, 
                                ctx->tokens, JSON_MAX_TOKENS);
    return ctx->num_tokens;
}

const jsmntok_t *json_find_key(const struct json_ctx *ctx, const char *key) {
    if (ctx->num_tokens < 1) return NULL;
    
    /* Assume first token is object */
    if (ctx->tokens[0].type != JSMN_OBJECT) return NULL;
    
    int obj_size = ctx->tokens[0].size;
    int idx = 1;  /* Start after root object */
    
    for (int i = 0; i < obj_size && idx < ctx->num_tokens; i++) {
        /* Current token should be key (string) */
        if (ctx->tokens[idx].type != JSMN_STRING) {
            idx++;
            continue;
        }
        
        /* Compare key */
        int key_len = ctx->tokens[idx].end - ctx->tokens[idx].start;
        if ((int)strlen(key) == key_len &&
            strncmp(ctx->data + ctx->tokens[idx].start, key, key_len) == 0) {
            /* Return value token (next token) */
            if (idx + 1 < ctx->num_tokens) {
                return &ctx->tokens[idx + 1];
            }
        }
        
        /* Skip key and value */
        idx++;
        if (idx < ctx->num_tokens) {
            /* Skip value - it may have children */
            idx += 1 + ctx->tokens[idx].size;
        }
    }
    
    return NULL;
}

const char *json_get_string(const struct json_ctx *ctx, const char *key,
                            const char *default_val) {
    const jsmntok_t *token = json_find_key(ctx, key);
    if (!token) return default_val;
    
    if (token->type != JSMN_STRING && token->type != JSMN_PRIMITIVE) {
        return default_val;
    }
    
    return ctx->data + token->start;
}

int json_get_int(const struct json_ctx *ctx, const char *key, int default_val) {
    const char *str = json_get_string(ctx, key, NULL);
    if (!str) return default_val;
    
    return atoi(str);
}

bool json_get_bool(const struct json_ctx *ctx, const char *key, bool default_val) {
    const char *str = json_get_string(ctx, key, NULL);
    if (!str) return default_val;
    
    if (strcmp(str, "true") == 0) return 1;
    if (strcmp(str, "false") == 0) return 0;
    return default_val;
}

const jsmntok_t *json_get_token(const struct json_ctx *ctx, const char *key) {
    return json_find_key(ctx, key);
}

int json_array_len(const struct json_ctx *ctx, const jsmntok_t *array) {
    (void)ctx;
    if (!array || array->type != JSMN_ARRAY) return 0;
    return array->size;
}

const jsmntok_t *json_array_get(const struct json_ctx *ctx,
                                 const jsmntok_t *array, int index) {
    (void)ctx;
    if (!array || array->type != JSMN_ARRAY) return NULL;
    if (index < 0 || index >= array->size) return NULL;
    
    /* Find the indexed element */
    /* This is simplified - real implementation needs to walk tokens */
    return NULL;  /* TODO: Implement properly */
}

int json_extract_string(const struct json_ctx *ctx, const jsmntok_t *token,
                        char *out, size_t max_len) {
    if (!token || !out || max_len == 0) return -1;
    
    int len = token->end - token->start;
    if (len >= (int)max_len) len = max_len - 1;
    
    strncpy(out, ctx->data + token->start, len);
    out[len] = '\0';
    return len;
}

int extract_json_string(const char *json, const char *key,
                       char *out, size_t out_len) {
    struct json_ctx ctx;
    const jsmntok_t *token;

    if (!json || !key || !out || out_len == 0) {
        return -1;
    }

    json_init(&ctx);
    if (json_parse(&ctx, json, strlen(json)) < 0) {
        return -1;
    }

    token = json_get_token(&ctx, key);
    if (!token) {
        return -1;
    }

    return json_extract_string(&ctx, token, out, out_len);
}

/* Escape a string for safe JSON inclusion
 * Replaces: " -> \", \ -> \\, control chars -> \b, \f, \n, \r, \t or \uXXXX
 * Returns: 0 on success, -1 if output buffer too small
 */
int json_escape(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) return -1;
    
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        const char *escape = NULL;
        char unicode_escape[7];
        
        /* Check if character needs escaping */
        switch (c) {
            case '"':  escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\b': escape = "\\b";  break;
            case '\f': escape = "\\f";  break;
            case '\n': escape = "\\n";  break;
            case '\r': escape = "\\r";  break;
            case '\t': escape = "\\t";  break;
            default:
                /* Control characters (0x00-0x1F) need unicode escape */
                if ((unsigned char)c < 0x20) {
                    snprintf(unicode_escape, sizeof(unicode_escape), 
                             "\\u%04X", (unsigned char)c);
                    escape = unicode_escape;
                }
                break;
        }
        
        if (escape) {
            size_t escape_len = strlen(escape);
            if (j + escape_len >= output_size) {
                output[0] = '\0';
                return -1; /* Buffer too small */
            }
            memcpy(output + j, escape, escape_len);
            j += escape_len;
        } else {
            if (j + 1 >= output_size) {
                output[0] = '\0';
                return -1; /* Buffer too small */
            }
            output[j++] = c;
        }
    }
    
    output[j] = '\0';
    return 0;
}
