#include "functions.h"

#include "memu_client.h"
#include "json.h"
#include "routeros.h"
#include "buf.h"
#ifndef DISABLE_WEB_SEARCH
#include "http_client.h"
#endif

#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_FUNCTIONS 32
#define FILE_TOOL_MAX_BYTES 16384

struct function_entry {
    char name[64];
    char description[256];
    char schema[512];
    function_fn fn;
};

static struct function_entry g_registry[MAX_FUNCTIONS];
static int g_registry_count;

static const char *json_string_field(const char *args_json, const char *key,
                                     char *out, size_t out_len) {
    char pattern[64];
    const char *start;
    const char *value_start;
    const char *value_end;

    if (!args_json || !key || !out || out_len == 0) {
        return NULL;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    start = strstr(args_json, pattern);
    if (!start) {
        return NULL;
    }

    value_start = start + strlen(pattern);
    value_end = strchr(value_start, '"');
    if (!value_end) {
        return NULL;
    }

    size_t n = (size_t)(value_end - value_start);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, value_start, n);
    out[n] = '\0';
    return out;
}

static int fn_shell_exec(const char *args_json, char *result_buf, size_t result_len);
static int fn_file_read(const char *args_json, char *result_buf, size_t result_len);
static int fn_file_write(const char *args_json, char *result_buf, size_t result_len);
static int fn_composio_call(const char *args_json, char *result_buf, size_t result_len);
static int fn_memory_forget(const char *args_json, char *result_buf, size_t result_len);
static int fn_web_scrape(const char *args_json, char *result_buf, size_t result_len);

static int fn_parse_url(const char *args_json, char *result_buf, size_t result_len) {
    char url[512];
    const char *prefix = "https://";
    const char *p;
    const char *slash;
    char host[256] = {0};
    char path[256] = "/";

    if (!json_string_field(args_json, "url", url, sizeof(url))) {
        snprintf(result_buf, result_len, "error: missing url");
        return -1;
    }

    p = url;
    if (strncmp(url, prefix, strlen(prefix)) == 0) {
        p = url + strlen(prefix);
    }
    slash = strchr(p, '/');
    if (slash) {
        size_t host_len = (size_t)(slash - p);
        if (host_len >= sizeof(host)) {
            host_len = sizeof(host) - 1;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        snprintf(path, sizeof(path), "%s", slash);
    } else {
        size_t host_len = strlen(p);
        if (host_len >= sizeof(host)) {
            host_len = sizeof(host) - 1;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
    }

    snprintf(result_buf, result_len, "{\"host\":\"%s\",\"path\":\"%s\"}", host, path);
    return 0;
}

static int fn_health_check(const char *args_json, char *result_buf, size_t result_len) {
    (void)args_json;
    snprintf(result_buf, result_len, "{\"pid\":%d,\"status\":\"ok\"}", (int)getpid());
    return 0;
}

static int fn_memory_store(const char *args_json, char *result_buf, size_t result_len) {
    char key[128];
    char value[512];

    if (!json_string_field(args_json, "key", key, sizeof(key)) ||
        !json_string_field(args_json, "value", value, sizeof(value))) {
        snprintf(result_buf, result_len, "error: missing key/value");
        return -1;
    }

    {
        char payload[1024];
        snprintf(payload, sizeof(payload), "%s=%s", key, value);
        if (memu_memorize(payload, "conversation", "default-user") != 0) {
            snprintf(result_buf, result_len, "error: memu store failed");
            return -1;
        }
    }

    if (memu_memorize(value, "conversation", key) != 0) {
        snprintf(result_buf, result_len, "error: store failed");
        return -1;
    }
    snprintf(result_buf, result_len, "ok");
    return 0;
}

static int fn_memory_recall(const char *args_json, char *result_buf, size_t result_len) {
    char key[128];
    char value[512];

    if (!json_string_field(args_json, "key", key, sizeof(key))) {
        snprintf(result_buf, result_len, "error: missing key");
        return -1;
    }

    if (memu_retrieve(key, "rag", value, sizeof(value)) != 0) {
        snprintf(result_buf, result_len, "error: not found");
        return -1;
    }

    snprintf(result_buf, result_len, "%s", value);
    return 0;
}

static int fn_memory_forget(const char *args_json, char *result_buf, size_t result_len) {
    char key[128];

    if (!json_string_field(args_json, "key", key, sizeof(key))) {
        snprintf(result_buf, result_len, "error: missing key");
        return -1;
    }

    if (memu_forget(key) != 0) {
        snprintf(result_buf, result_len, "error: forget failed");
        return -1;
    }

    snprintf(result_buf, result_len, "ok");
    return 0;
}

static int fn_web_search(const char *args_json, char *result_buf, size_t result_len) {
#ifdef DISABLE_WEB_SEARCH
    (void)args_json;
    snprintf(result_buf, result_len, "error: web_search disabled in static build");
    return -1;
#else
    char query[256];
    char url[512];
    curl_http_client *http;
    curl_http_response response;

    if (!json_string_field(args_json, "query", query, sizeof(query))) {
        snprintf(result_buf, result_len, "error: missing query");
        return -1;
    }

    snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", query);
    http = curl_http_client_create();
    if (!http) {
        snprintf(result_buf, result_len, "error: http init failed");
        return -1;
    }

    if (curl_http_get(http, url, &response) != 0 || response.status_code != 200 || !response.body) {
        curl_http_client_destroy(http);
        snprintf(result_buf, result_len, "error: search failed");
        return -1;
    }

    snprintf(result_buf, result_len, "%.600s", response.body);
    curl_http_response_free(&response);
    curl_http_client_destroy(http);
    return 0;
#endif
}

static int fn_web_scrape(const char *args_json, char *result_buf, size_t result_len) {
#ifdef DISABLE_WEB_SEARCH
    (void)args_json;
    snprintf(result_buf, result_len, "error: web_scrape disabled in static build");
    return -1;
#else
    const char *mock = getenv("WEBSCRAPE_MOCK_RESPONSE");
    char url[512];
    char candidate[1024];
    char escaped_text[2048];
    curl_http_client *http;
    curl_http_response response;
    const char *services;
    const char *p;

    if (mock && mock[0] != '\0') {
        snprintf(result_buf, result_len, "%s", mock);
        return 0;
    }
    if (!json_string_field(args_json, "url", url, sizeof(url))) {
        snprintf(result_buf, result_len, "error: missing url");
        return -1;
    }

    services = getenv("WEBSCRAPE_SERVICES");
    if (!services || services[0] == '\0') {
        services = "jina,zai,firecrawl,scrapingbee";
    }

    http = curl_http_client_create();
    if (!http) {
        snprintf(result_buf, result_len, "error: http init failed");
        return -1;
    }

    p = services;
    response.body = NULL;
    response.body_len = 0;
    response.status_code = 0;

    while (*p != '\0') {
        const char *q = p;
        char service[32];
        size_t n;

        while (*q != '\0' && *q != ',') {
            q++;
        }
        n = (size_t)(q - p);
        if (n >= sizeof(service)) {
            n = sizeof(service) - 1;
        }
        memcpy(service, p, n);
        service[n] = '\0';

        if (strcmp(service, "jina") == 0) {
            snprintf(candidate, sizeof(candidate), "https://r.jina.ai/http://%s",
                     strncmp(url, "https://", 8) == 0 ? (url + 8) : url);
        } else if (strcmp(service, "zai") == 0) {
            snprintf(candidate, sizeof(candidate), "https://api.z.ai/web/scrape?url=%s", url);
        } else if (strcmp(service, "firecrawl") == 0) {
            snprintf(candidate, sizeof(candidate), "https://api.firecrawl.dev/v1/scrape?url=%s", url);
        } else if (strcmp(service, "scrapingbee") == 0) {
            const char *key = getenv("SCRAPINGBEE_API_KEY");
            if (key && key[0] != '\0') {
                snprintf(candidate, sizeof(candidate),
                         "https://app.scrapingbee.com/api/v1/?url=%s&api_key=%s", url, key);
            } else {
                candidate[0] = '\0';
            }
        } else {
            candidate[0] = '\0';
        }

        if (candidate[0] != '\0' && curl_http_get(http, candidate, &response) == 0 &&
            response.status_code >= 200 && response.status_code < 300 && response.body) {
            break;
        }

        curl_http_response_free(&response);
        if (*q == ',') {
            p = q + 1;
        } else {
            p = q;
        }
    }

    if (!response.body) {
        curl_http_client_destroy(http);
        snprintf(result_buf, result_len, "error: scrape failed");
        return -1;
    }

    if (json_escape(response.body, escaped_text, sizeof(escaped_text)) != 0) {
        curl_http_response_free(&response);
        curl_http_client_destroy(http);
        snprintf(result_buf, result_len, "error: scrape encode failed");
        return -1;
    }

    snprintf(result_buf, result_len,
             "{\"title\":\"\",\"text\":\"%.1800s\",\"links\":[]}", escaped_text);
    curl_http_response_free(&response);
    curl_http_client_destroy(http);
    return 0;
#endif
}

static int fn_skill_list(const char *args_json, char *result_buf, size_t result_len) {
    DIR *dir;
    struct dirent *entry;
    size_t used = 0;

    (void)args_json;

    dir = opendir("skills");
    if (!dir) {
        if (result_len > 0) {
            result_buf[0] = '\0';
        }
        return 0;
    }

    result_buf[0] = '\0';
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        int wrote = snprintf(result_buf + used, result_len - used, "%s\n", entry->d_name);
        if (wrote < 0 || (size_t)wrote >= (result_len - used)) {
            break;
        }
        used += (size_t)wrote;
    }
    closedir(dir);
    return 0;
}

static int skill_name_valid(const char *skill) {
    size_t i;

    if (!skill || skill[0] == '\0') {
        return 0;
    }
    if (strstr(skill, "..") != NULL) {
        return 0;
    }
    for (i = 0; skill[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)skill[i];
        if (!(isalnum(ch) || ch == '_' || ch == '-' || ch == '.')) {
            return 0;
        }
    }
    return 1;
}

static int skill_params_safe(const char *params) {
    const char *invalid = "&;|`$><\n\r\\\"'";

    if (!params) {
        return 1;
    }
    return strpbrk(params, invalid) == NULL;
}

static int fn_skill_invoke(const char *args_json, char *result_buf, size_t result_len) {
    char skill[128];
    char params[256];
    char cmd[512];
    char skill_path[256];
    char script_buf[2048];
    size_t script_len;
    FILE *fp;
    size_t n;

    if (!json_string_field(args_json, "skill", skill, sizeof(skill))) {
        snprintf(result_buf, result_len, "error: missing skill");
        return -1;
    }
    if (!json_string_field(args_json, "params", params, sizeof(params))) {
        params[0] = '\0';
    }

    if (!skill_name_valid(skill)) {
        snprintf(result_buf, result_len, "error: invalid skill name");
        return -1;
    }
    if (!skill_params_safe(params)) {
        snprintf(result_buf, result_len, "error: invalid skill params");
        return -1;
    }

    {
        const char *skills_mode = getenv("SKILLS_MODE");
        if (skills_mode && strcmp(skills_mode, "routeros") == 0) {
            const char *host = getenv("ROUTER_HOST");
            const char *user = getenv("ROUTER_USER");
            const char *pass = getenv("ROUTER_PASS");
            struct routeros_ctx *ros;

            if (!host || !user || !pass) {
                snprintf(result_buf, result_len, "error: missing router credentials");
                return -1;
            }

            snprintf(skill_path, sizeof(skill_path), "./skills/%s", skill);
            fp = fopen(skill_path, "r");
            if (!fp) {
                snprintf(result_buf, result_len, "error: skill not found");
                return -1;
            }
            script_len = fread(script_buf, 1, sizeof(script_buf) - 1, fp);
            fclose(fp);
            script_buf[script_len] = '\0';

            if (params[0] != '\0' && script_len + strlen(params) + 4 < sizeof(script_buf)) {
                int cmd_len = snprintf(script_buf + script_len,
                                       sizeof(script_buf) - script_len,
                                       "\n# %s",
                                       params);
                if (cmd_len < 0 || (size_t)cmd_len >= (sizeof(script_buf) - script_len)) {
                    snprintf(result_buf, result_len, "error: command too long");
                    return -1;
                }
            }

            ros = routeros_init(host, 443, user, pass);
            if (!ros) {
                snprintf(result_buf, result_len, "error: router init failed");
                return -1;
            }
            if (routeros_script_run_inline(ros, script_buf, result_buf, result_len) != 0) {
                routeros_destroy(ros);
                snprintf(result_buf, result_len, "error: router skill run failed");
                return -1;
            }
            routeros_destroy(ros);
            return 0;
        }
    }

    snprintf(cmd, sizeof(cmd), "./skills/%s %s", skill, params);
    fp = popen(cmd, "r");
    if (!fp) {
        snprintf(result_buf, result_len, "error: failed to invoke skill");
        return -1;
    }

    n = fread(result_buf, 1, result_len - 1, fp);
    result_buf[n] = '\0';
    pclose(fp);
    return 0;
}

static int fn_routeros_execute(const char *args_json, char *result_buf, size_t result_len) {
    const char *host = getenv("ROUTER_HOST");
    const char *user = getenv("ROUTER_USER");
    const char *pass = getenv("ROUTER_PASS");
    char command[1024];
    struct routeros_ctx *ros;
    int rc;

    if (!host || !user || !pass) {
        snprintf(result_buf, result_len, "error: missing ROUTER_HOST/ROUTER_USER/ROUTER_PASS");
        return -1;
    }
    if (!json_string_field(args_json, "command", command, sizeof(command))) {
        snprintf(result_buf, result_len, "error: missing command");
        return -1;
    }

    ros = routeros_init(host, 443, user, pass);
    if (!ros) {
        snprintf(result_buf, result_len, "error: router init failed");
        return -1;
    }

    rc = routeros_execute(ros, command, result_buf, result_len);
    routeros_destroy(ros);
    return rc == 0 ? 0 : -1;
}

int functions_init(void) {
    g_registry_count = 0;
    memset(g_registry, 0, sizeof(g_registry));
    {
        const char *api_key = getenv("MEMU_API_KEY");
        const char *base_url = getenv("MEMU_BASE_URL");
        if (api_key && api_key[0] != '\0') {
            memu_client_configure(api_key, (base_url && base_url[0]) ? base_url : "https://api.memu.so");
        }
    }

    function_register_with_schema("parse_url", "Parse URL host/path",
                                  "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"}},\"required\":[\"url\"]}", fn_parse_url);
    function_register_with_schema("health_check", "Return process health",
                                  "{\"type\":\"object\",\"properties\":{}}", fn_health_check);
    function_register_with_schema("memory_store", "Store key/value memory",
                                  "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"key\",\"value\"]}", fn_memory_store);
    function_register_with_schema("memory_recall", "Recall key memory",
                                  "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"}},\"required\":[\"key\"]}", fn_memory_recall);
    function_register_with_schema("memory_forget", "Forget key memory",
                                  "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"}},\"required\":[\"key\"]}", fn_memory_forget);
    function_register_with_schema("web_search", "Search web documents",
                                  "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}", fn_web_search);
    function_register_with_schema("web_scrape", "Scrape URL via cloud services",
                                  "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"}},\"required\":[\"url\"]}", fn_web_scrape);
    function_register_with_schema("skill_list", "List skills directory entries",
                                  "{\"type\":\"object\",\"properties\":{}}", fn_skill_list);
    function_register_with_schema("skill_invoke", "Invoke executable skill from skills directory",
                                  "{\"type\":\"object\",\"properties\":{\"skill\":{\"type\":\"string\"},\"params\":{\"type\":\"string\"}},\"required\":[\"skill\"]}", fn_skill_invoke);
    function_register_with_schema("routeros_execute", "Execute RouterOS command from args",
                                  "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}", fn_routeros_execute);
    function_register_with_schema("shell_exec", "Execute allowed shell command",
                                  "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}", fn_shell_exec);
    function_register_with_schema("file_read", "Read file in workspace",
                                  "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}", fn_file_read);
    function_register_with_schema("file_write", "Write file in workspace",
                                  "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}", fn_file_write);
    function_register_with_schema("composio_call", "Call Composio-compatible endpoint",
                                  "{\"type\":\"object\",\"properties\":{\"tool\":{\"type\":\"string\"},\"input\":{\"type\":\"string\"}},\"required\":[\"tool\",\"input\"]}", fn_composio_call);
    return 0;
}

void functions_destroy(void) {
    g_registry_count = 0;
    memset(g_registry, 0, sizeof(g_registry));
}

int function_register(const char *name, const char *description, function_fn fn) {
    return function_register_with_schema(name, description,
        "{\"type\":\"object\",\"properties\":{}}", fn);
}

int function_register_with_schema(const char *name, const char *description,
                                  const char *schema_json, function_fn fn) {
    if (!name || !description || !fn) {
        return -1;
    }
    if (g_registry_count >= MAX_FUNCTIONS) {
        return -1;
    }

    if (safe_snprintf(g_registry[g_registry_count].name,
                      sizeof(g_registry[g_registry_count].name),
                      "%s", name) != 0) {
        return -1;
    }
    if (safe_snprintf(g_registry[g_registry_count].description,
                      sizeof(g_registry[g_registry_count].description),
                      "%s", description) != 0) {
        return -1;
    }
    if (safe_snprintf(g_registry[g_registry_count].schema,
                      sizeof(g_registry[g_registry_count].schema),
                      "%s", schema_json ? schema_json : "{\"type\":\"object\",\"properties\":{}}") != 0) {
        return -1;
    }
    g_registry[g_registry_count].fn = fn;
    g_registry_count++;
    return 0;
}

int function_call(const char *name, const char *args_json,
                  char *result_buf, size_t result_len) {
    int i;

    if (!name || !result_buf || result_len == 0) {
        return -1;
    }

    for (i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) {
            return g_registry[i].fn(args_json ? args_json : "{}", result_buf, result_len);
        }
    }

    snprintf(result_buf, result_len, "error: function not found");
    return -1;
}

int function_list(char *out, size_t max_len) {
    size_t used = 0;
    int i;

    if (!out || max_len == 0) {
        return -1;
    }

    out[0] = '\0';
    for (i = 0; i < g_registry_count; i++) {
        int wrote = snprintf(out + used, max_len - used, "%s\n", g_registry[i].name);
        if (wrote < 0 || (size_t)wrote >= (max_len - used)) {
            return -1;
        }
        used += (size_t)wrote;
    }

    return 0;
}

int function_get_schema(const char *name, char *out, size_t max_len) {
    int i;

    if (!name || !out || max_len == 0) {
        return -1;
    }
    for (i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) {
            snprintf(out, max_len, "%s", g_registry[i].schema);
            return 0;
        }
    }
    return -1;
}

static int command_allowed(const char *command) {
    const char *allow = getenv("ALLOWED_SHELL_CMDS");
    const char *danger = "&;|`$><\n\r";
    char buf[512];
    char command_copy[512];
    char *command_tok;
    char *tok;
    size_t command_len;
    int allowed;

    if (!command || command[0] == '\0') {
        return 0;
    }
    if (strstr(command, "../") != NULL || strstr(command, "..\\") != NULL) {
        return 0;
    }
    command_len = strnlen(command, sizeof(command_copy));
    if (command_len >= sizeof(command_copy) ||
        memchr(command, '\0', command_len + 1) != command + command_len) {
        return 0;
    }
    if (strpbrk(command, danger) != NULL) {
        return 0;
    }
    if (!allow || allow[0] == '\0') {
        return 0;
    }

    snprintf(command_copy, sizeof(command_copy), "%s", command);
    command_tok = strtok(command_copy, " \t");
    if (!command_tok || command_tok[0] == '\0') {
        return 0;
    }

    snprintf(buf, sizeof(buf), "%s", allow);
    tok = strtok(buf, ",");
    allowed = 0;
    while (tok) {
        while (*tok == ' ' || *tok == '\t') {
            tok++;
        }
        if (tok[0] != '\0' && strcmp(command_tok, tok) == 0) {
            allowed = 1;
            break;
        }
        tok = strtok(NULL, ",");
    }
    if (allowed) {
        return 1;
    }

    if (command_tok[0] == '/') {
        return 0;
    }
    return 0;
}

static int safe_workspace_path(const char *path) {
    char cwd[PATH_MAX];
    char candidate[PATH_MAX];
    char resolved[PATH_MAX];
    char dirbuf[PATH_MAX];
    char *slash;
    const char *base;
    const char *forbidden_env;
    static const char *forbidden_defaults[] = {
        "/etc", "/root", "/proc", "/sys", "/dev", "/bin", "/sbin",
        "/lib", "/usr", "/var", "/boot", "/mnt", "/opt"
    };
    size_t i;

    if (!path || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || strstr(path, "..") != NULL) {
        return 0;
    }
    if (!getcwd(cwd, sizeof(cwd))) {
        return 0;
    }
    if (snprintf(candidate, sizeof(candidate), "%s/%s", cwd, path) >= (int)sizeof(candidate)) {
        return 0;
    }
    base = strrchr(path, '/');
    if (base && (strstr(base + 1, "..") != NULL)) {
        return 0;
    }

    for (i = 0; i < (sizeof(forbidden_defaults) / sizeof(forbidden_defaults[0])); i++) {
        size_t n = strlen(forbidden_defaults[i]);
        if (strncmp(candidate, forbidden_defaults[i], n) == 0 &&
            (candidate[n] == '\0' || candidate[n] == '/')) {
            return 0;
        }
    }

    forbidden_env = getenv("FORBIDDEN_PATHS");
    if (forbidden_env && forbidden_env[0] != '\0') {
        char envbuf[512];
        char *tok;
        snprintf(envbuf, sizeof(envbuf), "%s", forbidden_env);
        tok = strtok(envbuf, ",");
        while (tok) {
            while (*tok == ' ' || *tok == '\t') {
                tok++;
            }
            if (*tok != '\0') {
                size_t n = strlen(tok);
                if (strncmp(candidate, tok, n) == 0 &&
                    (candidate[n] == '\0' || candidate[n] == '/')) {
                    return 0;
                }
            }
            tok = strtok(NULL, ",");
        }
    }

    if (realpath(candidate, resolved)) {
        size_t n = strlen(cwd);
        if (strncmp(resolved, cwd, n) != 0 || (resolved[n] != '\0' && resolved[n] != '/')) {
            return 0;
        }
        return 1;
    }

    snprintf(dirbuf, sizeof(dirbuf), "%s", candidate);
    slash = strrchr(dirbuf, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    if (!realpath(dirbuf, resolved)) {
        return 0;
    }
    {
        size_t n = strlen(cwd);
        if (strncmp(resolved, cwd, n) != 0 || (resolved[n] != '\0' && resolved[n] != '/')) {
            return 0;
        }
    }

    return 1;
}

static int fn_shell_exec(const char *args_json, char *result_buf, size_t result_len) {
    char command[512];
    FILE *fp;
    size_t n;

    if (!json_string_field(args_json, "command", command, sizeof(command))) {
        snprintf(result_buf, result_len, "error: missing command");
        return -1;
    }
    if (!command_allowed(command)) {
        snprintf(result_buf, result_len, "error: command not allowed");
        return -1;
    }

    fp = popen(command, "r");
    if (!fp) {
        snprintf(result_buf, result_len, "error: popen failed");
        return -1;
    }
    n = fread(result_buf, 1, result_len - 1, fp);
    result_buf[n] = '\0';
    pclose(fp);
    return 0;
}

static int fn_file_read(const char *args_json, char *result_buf, size_t result_len) {
    char path[256];
    FILE *fp;
    long file_size;
    size_t n;

    if (!json_string_field(args_json, "path", path, sizeof(path))) {
        snprintf(result_buf, result_len, "error: missing path");
        return -1;
    }
    if (!safe_workspace_path(path)) {
        snprintf(result_buf, result_len, "error: invalid path");
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp) {
        snprintf(result_buf, result_len, "error: open failed");
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        snprintf(result_buf, result_len, "error: seek failed");
        return -1;
    }
    file_size = ftell(fp);
    if (file_size < 0 || file_size > FILE_TOOL_MAX_BYTES) {
        fclose(fp);
        snprintf(result_buf, result_len, "error: file too large");
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        snprintf(result_buf, result_len, "error: seek failed");
        return -1;
    }
    n = fread(result_buf, 1, result_len - 1, fp);
    result_buf[n] = '\0';
    fclose(fp);
    return 0;
}

static int fn_file_write(const char *args_json, char *result_buf, size_t result_len) {
    char path[256];
    char content[1024];
    FILE *fp;

    if (!json_string_field(args_json, "path", path, sizeof(path)) ||
        !json_string_field(args_json, "content", content, sizeof(content))) {
        snprintf(result_buf, result_len, "error: missing path/content");
        return -1;
    }
    if (!safe_workspace_path(path)) {
        snprintf(result_buf, result_len, "error: invalid path");
        return -1;
    }
    if (strlen(content) > FILE_TOOL_MAX_BYTES) {
        snprintf(result_buf, result_len, "error: content too large");
        return -1;
    }

    fp = fopen(path, "w");
    if (!fp) {
        snprintf(result_buf, result_len, "error: open failed");
        return -1;
    }
    if (fwrite(content, 1, strlen(content), fp) != strlen(content)) {
        fclose(fp);
        snprintf(result_buf, result_len, "error: write failed");
        return -1;
    }
    fclose(fp);
    snprintf(result_buf, result_len, "ok");
    return 0;
}

static int fn_composio_call(const char *args_json, char *result_buf, size_t result_len) {
#ifdef DISABLE_WEB_SEARCH
    (void)args_json;
    snprintf(result_buf, result_len, "error: composio disabled in static build");
    return -1;
#else
    const char *api = getenv("COMPOSIO_URL");
    const char *key = getenv("COMPOSIO_API_KEY");
    char tool[128];
    char input[512];
    char body[1024];
    curl_http_client *http;
    curl_http_response response;

    if (!api || !key) {
        snprintf(result_buf, result_len, "error: missing COMPOSIO_URL/COMPOSIO_API_KEY");
        return -1;
    }
    if (!json_string_field(args_json, "tool", tool, sizeof(tool)) ||
        !json_string_field(args_json, "input", input, sizeof(input))) {
        snprintf(result_buf, result_len, "error: missing tool/input");
        return -1;
    }

    snprintf(body, sizeof(body), "{\"tool\":\"%s\",\"input\":\"%s\",\"api_key\":\"%s\"}",
             tool, input, key);
    http = curl_http_client_create();
    if (!http) {
        snprintf(result_buf, result_len, "error: http init failed");
        return -1;
    }
    if (curl_http_post(http, api, body, &response) != 0) {
        curl_http_client_destroy(http);
        snprintf(result_buf, result_len, "error: composio call failed");
        return -1;
    }
    snprintf(result_buf, result_len, "%s", response.body ? response.body : "{}");
    curl_http_response_free(&response);
    curl_http_client_destroy(http);
    return 0;
#endif
}
