/*
 * MikroClaw - LLM API Client
 */

#ifndef LLM_H
#define LLM_H

#include <stddef.h>
#include "mikroclaw_config.h"
#include "provider_registry.h"
#include "llm_stream.h"

/* LLM configuration */
struct llm_config {
    char base_url[MAX_URL_LEN];
    char model[64];
    char api_key[256];
    enum provider_auth_style auth_style;
    float temperature;
    int max_tokens;
    int timeout_ms;
};

/* LLM context */
struct llm_ctx {
    struct llm_config config;
    struct http_client *http;
};

/* Initialize LLM client */
struct llm_ctx *llm_init(struct llm_config *config);

/* Cleanup LLM client */
void llm_destroy(struct llm_ctx *ctx);

/* Send chat message and get response */
int llm_chat(struct llm_ctx *ctx,
             const char *system_prompt,
             const char *user_message,
             char *response, size_t max_response);

int llm_chat_reliable(struct llm_ctx *ctx,
                      const char *system_prompt,
                      const char *user_message,
                      char *response, size_t max_response);

int llm_chat_stream(struct llm_ctx *ctx,
                    const char *system_prompt,
                    const char *user_message,
                    llm_stream_chunk_cb cb,
                    void *user_data,
                    char *response, size_t max_response);

#endif /* LLM_H */
