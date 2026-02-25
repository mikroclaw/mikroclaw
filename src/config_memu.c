#include "config_memu.h"

#include "memu_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int extract_json_string(const char *json, const char *key, char *out, size_t out_len) {
    char pattern[128];
    const char *p;
    const char *start;
    const char *end;
    size_t n;

    if (!json || !key || !out || out_len == 0) {
        return 0;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return 0;
    }

    start = p + strlen(pattern);
    end = start;
    while (*end) {
        if (*end == '"' && (end == start || end[-1] != '\\')) {
            break;
        }
        end++;
    }
    if (*end != '"') {
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

int config_memu_load(const char *device_id, struct memu_boot_config *cfg) {
    const char *mock = getenv("MEMU_BOOT_CONFIG_JSON");
    char response[8192];
    char query[512];
    const char *json = response;

    if (!cfg) {
        return -1;
    }
    memset(cfg, 0, sizeof(*cfg));

    if (mock && mock[0]) {
        snprintf(response, sizeof(response), "%s", mock);
    } else {
        snprintf(query, sizeof(query),
                 "Return JSON config for device_id %s with keys telegram_bot_token,llm_api_key,routeros_host,routeros_user,routeros_pass,model,discord_webhook_url,slack_webhook_url",
                 (device_id && device_id[0]) ? device_id : "mikroclaw-default");
        if (memu_retrieve(query, "llm", response, sizeof(response)) != 0) {
            return -1;
        }
    }

    {
        const char *obj_start = strchr(response, '{');
        const char *obj_end = strrchr(response, '}');
        if (obj_start && obj_end && obj_end > obj_start) {
            json = obj_start;
        }
    }

    extract_json_string(json, "telegram_bot_token", cfg->telegram_bot_token, sizeof(cfg->telegram_bot_token));
    extract_json_string(json, "llm_api_key", cfg->llm_api_key, sizeof(cfg->llm_api_key));
    extract_json_string(json, "routeros_host", cfg->routeros_host, sizeof(cfg->routeros_host));
    extract_json_string(json, "routeros_user", cfg->routeros_user, sizeof(cfg->routeros_user));
    extract_json_string(json, "routeros_pass", cfg->routeros_pass, sizeof(cfg->routeros_pass));
    extract_json_string(json, "model", cfg->model, sizeof(cfg->model));
    extract_json_string(json, "discord_webhook_url", cfg->discord_webhook_url, sizeof(cfg->discord_webhook_url));
    extract_json_string(json, "slack_webhook_url", cfg->slack_webhook_url, sizeof(cfg->slack_webhook_url));

    return 0;
}
