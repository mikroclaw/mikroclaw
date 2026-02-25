/**
 * MikroClaw Configuration
 * Compile-time feature selection
 */

#ifndef MIKROCLAW_CONFIG_H
#define MIKROCLAW_CONFIG_H

/* Version */
#define MIKROCLAW_VERSION "2026.02.25:BETA"
#define MIKROCLAW_NAME    "MikroClaw"

/* ============================================================================
 * LLM PROVIDER SELECTION (choose one)
 * ============================================================================ */
#define LLM_PROVIDER_OPENAI     1
// #define LLM_PROVIDER_OPENROUTER 0
// #define LLM_PROVIDER_CUSTOM     0

#if LLM_PROVIDER_OPENAI
#define LLM_DEFAULT_BASE_URL    "https://api.openai.com/v1"
#define LLM_DEFAULT_MODEL       "gpt-4o-mini"
#elif LLM_PROVIDER_OPENROUTER
#define LLM_DEFAULT_BASE_URL    "https://openrouter.ai/api/v1"
#define LLM_DEFAULT_MODEL       "google/gemini-flash"
#elif LLM_PROVIDER_CUSTOM
#define LLM_DEFAULT_BASE_URL    ""  /* Must be set via env */
#define LLM_DEFAULT_MODEL       ""
#endif

#define LLM_DEFAULT_TEMPERATURE 0.7f
#define LLM_DEFAULT_MAX_TOKENS  2048
#define LLM_TIMEOUT_MS          30000
#define MAX_URL_LEN             2048    /* Max URL length for LLM base_url */
#define LLM_MAX_RESPONSE        8192

/* ============================================================================
 * CHANNELS (compile-time selection)
 * ============================================================================ */
#define CHANNEL_TELEGRAM        1
// #define CHANNEL_DISCORD         0
// #define CHANNEL_SLACK           0
// #define CHANNEL_WEBHOOK         0
// #define CHANNEL_MATRIX          0

/* ============================================================================
 * STORAGE BACKENDS
 * ============================================================================ */
#define STORAGE_LOCAL_ENABLED   1
#define STORAGE_LOCAL_MAX       (2ULL * 1024 * 1024 + 901120)  /* 2.88MB */
#define STORAGE_LOCAL_PATH      "/data"
#define STORAGE_LOCAL_TEXT_ONLY 1

// #define STORAGE_ROUTEROS_ENABLED 0

/* ============================================================================
 * TOOLS (compile-time selection)
 * ============================================================================ */
#define TOOL_FILE_READ          1
#define TOOL_FILE_WRITE         1
#define TOOL_FILE_EDIT          1
#define TOOL_FILE_APPEND        1

#define TOOL_MEMORY_STORE       1
#define TOOL_MEMORY_RECALL      1
#define TOOL_MEMORY_FORGET      1

#define TOOL_CRON               1
#define TOOL_WEB_FETCH          1

// #define TOOL_SHELL              0
// #define TOOL_GIT                0
// #define TOOL_DELEGATE           0
// #define TOOL_SPAWN              0
// #define TOOL_WEB_SEARCH         0
// #define TOOL_HARDWARE           0

/* ============================================================================
 * FEATURES (compile-time selection)
 * ============================================================================ */
#define ENABLE_GATEWAY          1
#define ENABLE_HEARTBEAT        1
#define ENABLE_HEALTH           1
// #define ENABLE_SKILLS           0
// #define ENABLE_DOCTOR           0
/* WebSocket, MCP, Metrics, Sessions: not yet implemented */

/* ============================================================================
 * GATEWAY CONFIGURATION
 * ============================================================================ */
#define GATEWAY_PORT            18789
#define GATEWAY_BIND            "127.0.0.1"  /* Localhost only for security */
#define GATEWAY_MAX_REQUEST     8192

/* ============================================================================
 * BUFFER SIZES
 * ============================================================================ */
#define HTTP_MAX_RESPONSE       65536   /* 64KB */
#define HTTP_MAX_HEADERS        16
#define HTTP_TIMEOUT_MS         30000

#define JSON_MAX_TOKENS         256

#define MESSAGE_MAX             4096
#define COMMAND_MAX             4096
#define RESPONSE_MAX            8192
#ifndef PATH_MAX
#define PATH_MAX                256
#endif

#define STORAGE_MAX_VALUE       65536
#define MEMORY_MAX_ENTRIES      100

/* ============================================================================
 * BEHAVIOR
 * ============================================================================ */
#define POLL_INTERVAL_MS        100     /* 100ms between channel polls */
#define IDLE_SLEEP_MS           10      /* 10ms sleep when idle */
#define HEARTBEAT_INTERVAL_S    300     /* 5 minutes */

/* ============================================================================
 * ROUTEROS
 * ============================================================================ */
#define ROUTEROS_DEFAULT_HOST   "172.17.0.1"
#define ROUTEROS_DEFAULT_PORT   443
#define ROUTEROS_TIMEOUT_MS     10000

/* ============================================================================
 * SYSTEM INTEGRATION
 * ============================================================================ */
#define USE_SYSLOG              1
#define LOG_LEVEL               LOG_INFO  /* LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR */

#endif /* MIKROCLAW_CONFIG_H */
