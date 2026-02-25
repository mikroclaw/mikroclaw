/*
 * MikroClaw - Ultra-lightweight AI agent for MikroTik RouterOS
 * 
 * Target: <200KB static binary
 * Stack: mbedTLS + custom HTTP + jsmn JSON
 */

#ifndef MIKROCLAW_H
#define MIKROCLAW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "http.h"
#include "json.h"
#include "functions.h"
#include "gateway_auth.h"
#include "rate_limit.h"
#include "subagent.h"
#include "channel_supervisor.h"

/* Compile-time channel selection (define at build) */
#ifdef CHANNEL_TELEGRAM
#define HAS_TELEGRAM 1
#else
#define HAS_TELEGRAM 0
#endif

#ifdef CHANNEL_DISCORD
#define HAS_DISCORD 1
#else
#define HAS_DISCORD 0
#endif

#ifdef ENABLE_GATEWAY
#define HAS_GATEWAY 1
#else
#define HAS_GATEWAY 0
#endif

/* Size limits (embedded-friendly) */
#define MAX_RESPONSE_LEN    65536   /* 64KB max HTTP response */
#define MAX_JSON_TOKENS     256
#define MAX_CMD_LEN         4096
#define MAX_MESSAGE_LEN     4096
#define MAX_CHANNEL_NAME    32

/* Error codes */
enum mc_error {
    MC_OK = 0,
    MC_ERR_NOMEM = -1,
    MC_ERR_NETWORK = -2,
    MC_ERR_TLS = -3,
    MC_ERR_HTTP = -4,
    MC_ERR_JSON = -5,
    MC_ERR_CONFIG = -6,
    MC_ERR_ROUTEROS = -7,
};

/* HTTP client (blocking, synchronous) */
struct http_client;

/* HTTP compatibility macros */
#define http_new http_client_create
#define http_free http_client_destroy
#define http_response_free http_response_clear

/* Telegram channel */
#ifdef CHANNEL_TELEGRAM
#include "channels/telegram.h"
#endif
#ifdef CHANNEL_DISCORD
#include "channels/discord.h"
#endif
#ifdef CHANNEL_SLACK
#include "channels/slack.h"
#endif

/* RouterOS REST API */
struct routeros_ctx;

/* Gateway (OpenClaw compatible) */
#ifdef ENABLE_GATEWAY
#include "gateway.h"
#endif

/* Main context */
struct mikroclaw_ctx {
    /* Configuration (from env) */
    const char *openrouter_key;
    const char *model;
    
    /* LLM client */
    struct llm_ctx *llm;
    
    /* RouterOS connection */
    struct routeros_ctx *ros;
    
    /* Channels (compile-time selection) */
#ifdef CHANNEL_TELEGRAM
    struct telegram_ctx *telegram;
#endif
#ifdef CHANNEL_DISCORD
    struct discord_ctx *discord;
#endif
#ifdef CHANNEL_SLACK
    struct slack_ctx *slack;
#endif
    
    /* Gateway */
#ifdef ENABLE_GATEWAY
    struct gateway_ctx *gateway;
    struct gateway_auth_ctx *gateway_auth;
    struct rate_limit_ctx *rate_limit;
    struct subagent_ctx *subagent;
#endif

    struct channel_supervisor_ctx supervisor;
};

/* LLM interaction */
int llm_query(struct mikroclaw_ctx *ctx, const char *prompt,
              char *out_response, size_t max_response);

/* Main loop */
int mikroclaw_run(struct mikroclaw_ctx *ctx);

#endif /* MIKROCLAW_H */
