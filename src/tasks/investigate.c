#include "../task_handlers.h"

#include "../llm.h"
#include "../provider_registry.h"
#include "../routeros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int extract_json_string(const char *json, const char *key, char *out, size_t out_len) {
    char pattern[64];
    const char *p;
    const char *start;
    const char *end;
    size_t n;

    if (!json || !key || !out || out_len == 0) {
        return -1;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    start = p + strlen(pattern);
    end = strchr(start, '"');
    if (!end) {
        return -1;
    }
    n = (size_t)(end - start);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return 0;
}

static void append_text(char *dst, size_t dst_len, const char *text) {
    size_t used;
    size_t remain;

    if (!dst || dst_len == 0 || !text) {
        return;
    }
    used = strlen(dst);
    if (used >= dst_len - 1) {
        return;
    }
    remain = dst_len - used - 1;
    strncat(dst, text, remain);
}

static void append_query(struct routeros_ctx *ros,
                         char *ctx,
                         size_t ctx_len,
                         const char *label,
                         const char *path) {
    char buf[1024];
    char line[256];

    if (!ros || !ctx || ctx_len == 0 || !label || !path) {
        return;
    }

    snprintf(line, sizeof(line), "\n[%s] %s\n", label, path);
    append_text(ctx, ctx_len, line);

    if (routeros_get(ros, path, buf, sizeof(buf)) == 0) {
        append_text(ctx, ctx_len, buf);
    } else {
        append_text(ctx, ctx_len, "<query_failed>");
    }
    append_text(ctx, ctx_len, "\n");
}

static int llm_config_from_env(struct llm_config *cfg) {
    const char *provider_name;
    const char *fallback_key;
    struct provider_config provider;

    if (!cfg) {
        return -1;
    }
    memset(cfg, 0, sizeof(*cfg));

    provider_name = getenv("LLM_PROVIDER");
    if (!provider_name || provider_name[0] == '\0') {
        provider_name = "openrouter";
    }

    if (provider_registry_get(provider_name, &provider) == 0) {
        const char *provider_key = getenv(provider.api_key_env_var);
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", provider.base_url);
        cfg->auth_style = provider.auth_style;
        fallback_key = getenv("LLM_API_KEY");
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s",
                 (provider_key && provider_key[0] != '\0') ? provider_key :
                 (fallback_key ? fallback_key : ""));
    } else {
        const char *base = getenv("LLM_BASE_URL");
        const char *key = getenv("LLM_API_KEY");
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s",
                 (base && base[0] != '\0') ? base : "https://openrouter.ai/api/v1");
        cfg->auth_style = PROVIDER_AUTH_BEARER;
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key ? key : "");
    }

    if (cfg->api_key[0] == '\0') {
        const char *openrouter = getenv("OPENROUTER_KEY");
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", openrouter ? openrouter : "");
    }

    snprintf(cfg->model, sizeof(cfg->model), "%s",
             getenv("MODEL") ? getenv("MODEL") : "google/gemini-flash");
    cfg->temperature = 0.3f;
    cfg->max_tokens = 1024;
    cfg->timeout_ms = 30000;

    return (cfg->api_key[0] == '\0') ? -1 : 0;
}

int task_handle_investigate(const char *params_json, char *result, size_t result_len) {
    const char *json = params_json ? params_json : "{}";
    char target[128] = "system";
    char issue[256] = "";
    char user_msg[4096];
    char context[7000] = "";
    const char *host = getenv("ROUTER_HOST");
    const char *user = getenv("ROUTER_USER");
    const char *pass = getenv("ROUTER_PASS");
    struct routeros_ctx *ros;
    struct llm_config llm_cfg;
    struct llm_ctx *llm;

    if (!result || result_len == 0) {
        return -1;
    }
    result[0] = '\0';

    (void)extract_json_string(json, "target", target, sizeof(target));
    (void)extract_json_string(json, "issue", issue, sizeof(issue));

    ros = routeros_init(host, 443, user, pass);
    if (!ros) {
        snprintf(result, result_len, "error: RouterOS unavailable");
        return -1;
    }

    append_query(ros, context, sizeof(context), "system", "/rest/system/resource");
    append_query(ros, context, sizeof(context), "logs", "/rest/log");

    if (strstr(target, "ether") || strstr(target, "wlan") || strstr(target, "bridge") || strstr(target, "vlan")) {
        append_query(ros, context, sizeof(context), "interfaces", "/rest/interface");
        append_query(ros, context, sizeof(context), "ip_addresses", "/rest/ip/address");
    } else if (strchr(target, '.')) {
        append_query(ros, context, sizeof(context), "arp", "/rest/ip/arp");
        append_query(ros, context, sizeof(context), "dhcp_leases", "/rest/ip/dhcp-server/lease");
        append_query(ros, context, sizeof(context), "routes", "/rest/ip/route");
    } else if (strstr(target, "firewall")) {
        append_query(ros, context, sizeof(context), "fw_filter", "/rest/ip/firewall/filter");
        append_query(ros, context, sizeof(context), "fw_conn", "/rest/ip/firewall/connection");
    } else if (strstr(target, "dhcp")) {
        append_query(ros, context, sizeof(context), "dhcp_leases", "/rest/ip/dhcp-server/lease");
        append_query(ros, context, sizeof(context), "ip_addresses", "/rest/ip/address");
    } else if (strstr(target, "routing")) {
        append_query(ros, context, sizeof(context), "routes", "/rest/ip/route");
        append_query(ros, context, sizeof(context), "arp", "/rest/ip/arp");
        append_query(ros, context, sizeof(context), "neighbors", "/rest/ip/neighbor");
    } else {
        append_query(ros, context, sizeof(context), "health", "/rest/system/health");
    }

    if (llm_config_from_env(&llm_cfg) != 0) {
        routeros_destroy(ros);
        snprintf(result, result_len, "error: LLM key missing");
        return -1;
    }

    llm = llm_init(&llm_cfg);
    if (!llm) {
        routeros_destroy(ros);
        snprintf(result, result_len, "error: LLM unavailable");
        return -1;
    }

    snprintf(user_msg, sizeof(user_msg),
             "Target: %s\nIssue: %s\n\nRouterOS Data:\n",
             target,
             issue[0] ? issue : "(not provided)");
    append_text(user_msg, sizeof(user_msg), context);

    if (llm_chat(llm,
                 "You are a MikroTik network engineer. Diagnose the target based on live RouterOS data. "
                 "Reference exact values. Identify likely cause and recommended fix in concise bullet points.",
                 user_msg,
                 result,
                 result_len) != 0) {
        snprintf(result, result_len, "error: investigate llm call failed");
        llm_destroy(llm);
        routeros_destroy(ros);
        return -1;
    }

    llm_destroy(llm);
    routeros_destroy(ros);
    return 0;
}
