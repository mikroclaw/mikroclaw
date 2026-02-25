#include "config_validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct config_item {
    const char *name;
    int secret;
    int required;
};

static const struct config_item g_items[] = {
    {"BOT_TOKEN", 1, 1},
    {"OPENROUTER_KEY", 1, 1},
    {"ROUTER_HOST", 0, 1},
    {"ROUTER_USER", 0, 1},
    {"ROUTER_PASS", 1, 1},
    {"LLM_PROVIDER", 0, 0},
    {"LLM_BASE_URL", 0, 0},
    {"LLM_API_KEY", 1, 0},
    {"MEMU_API_KEY", 1, 0},
    {"MEMU_BASE_URL", 0, 0},
    {"MEMU_DEVICE_ID", 0, 0},
    {"GATEWAY_PORT", 0, 0},
    {"DISCORD_WEBHOOK_URL", 1, 0},
    {"SLACK_WEBHOOK_URL", 1, 0}
};

int config_validate_required(char *err_out, size_t err_len) {
    size_t i;

    if (!err_out || err_len == 0) {
        return -1;
    }

    err_out[0] = '\0';
    for (i = 0; i < (sizeof(g_items) / sizeof(g_items[0])); i++) {
        const char *v;
        if (!g_items[i].required) {
            continue;
        }
        v = getenv(g_items[i].name);
        if (!v || v[0] == '\0') {
            snprintf(err_out, err_len, "%s is required", g_items[i].name);
            return -1;
        }
    }

    return 0;
}

int config_dump_redacted(char *out, size_t out_len) {
    size_t i;
    size_t used = 0;

    if (!out || out_len == 0) {
        return -1;
    }

    out[0] = '\0';
    for (i = 0; i < (sizeof(g_items) / sizeof(g_items[0])); i++) {
        const char *v = getenv(g_items[i].name);
        const char *shown = "";
        int wrote;

        if (g_items[i].secret) {
            shown = (v && v[0] != '\0') ? "***" : "";
        } else if (v) {
            shown = v;
        }

        wrote = snprintf(out + used, out_len - used, "%s=%s\n", g_items[i].name, shown);
        if (wrote < 0 || (size_t)wrote >= (out_len - used)) {
            return -1;
        }
        used += (size_t)wrote;
    }

    return 0;
}
