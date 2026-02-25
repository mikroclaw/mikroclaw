/*
 * MikroClaw - JSON Utility Functions Header
 */

#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <stdbool.h>
#include "mikroclaw_config.h"
#include "../vendor/jsmn.h"

/* JSON context */
struct json_ctx {
    jsmn_parser parser;
    jsmntok_t tokens[JSON_MAX_TOKENS];
    int num_tokens;
    const char *data;
    int data_len;
};

/* Initialize JSON parser context */
void json_init(struct json_ctx *ctx);

/* Parse JSON string */
int json_parse(struct json_ctx *ctx, const char *data, size_t len);

/* Find key in JSON object */
const jsmntok_t *json_find_key(const struct json_ctx *ctx, const char *key);

/* Get string value by key */
const char *json_get_string(const struct json_ctx *ctx, const char *key, const char *default_val);

/* Get integer value by key */
int json_get_int(const struct json_ctx *ctx, const char *key, int default_val);

/* Get boolean value by key */
bool json_get_bool(const struct json_ctx *ctx, const char *key, bool default_val);

/* Get token by key */
const jsmntok_t *json_get_token(const struct json_ctx *ctx, const char *key);

/* Array utilities */
int json_array_len(const struct json_ctx *ctx, const jsmntok_t *array);
const jsmntok_t *json_array_get(const struct json_ctx *ctx,
                                 const jsmntok_t *array, int index);

/* Extract string from token */
int json_extract_string(const struct json_ctx *ctx, const jsmntok_t *token,
                        char *out, size_t max_len);

int extract_json_string(const char *json, const char *key,
                        char *out, size_t out_len);

/* Escape string for JSON inclusion */
int json_escape(const char *input, char *output, size_t output_size);

#endif /* JSON_H */
