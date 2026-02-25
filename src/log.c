#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static enum log_level g_level = LOG_LEVEL_INFO;

static const char *level_name(enum log_level level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "error";
        case LOG_LEVEL_WARN: return "warn";
        case LOG_LEVEL_INFO: return "info";
        case LOG_LEVEL_DEBUG: return "debug";
        default: return "info";
    }
}

void log_set_level_from_env(void) {
    const char *v = getenv("LOG_LEVEL");
    if (!v) {
        return;
    }
    if (strcmp(v, "error") == 0) g_level = LOG_LEVEL_ERROR;
    else if (strcmp(v, "warn") == 0) g_level = LOG_LEVEL_WARN;
    else if (strcmp(v, "debug") == 0) g_level = LOG_LEVEL_DEBUG;
    else g_level = LOG_LEVEL_INFO;
}

void log_emit(enum log_level level, const char *component, const char *message) {
    const char *fmt = getenv("LOG_FORMAT");
    time_t now;
    struct tm tm_now;
    char ts[64];

    if (level > g_level) {
        return;
    }
    if (!component) component = "app";
    if (!message) message = "";

    now = time(NULL);
    gmtime_r(&now, &tm_now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_now);

    if (fmt && strcmp(fmt, "json") == 0) {
        printf("{\"ts\":\"%s\",\"level\":\"%s\",\"component\":\"%s\",\"msg\":\"%s\"}\n",
               ts, level_name(level), component, message);
    } else {
        printf("[%s] %-5s %s: %s\n", ts, level_name(level), component, message);
    }
}
