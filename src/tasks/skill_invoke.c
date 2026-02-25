#include "../task_handlers.h"

#include <stdio.h>
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

int task_handle_skill_invoke(const char *params_json, char *result, size_t result_len) {
    char skill[128];
    char cmd[256];
    FILE *fp;
    size_t n;

    if (extract_json_string(params_json ? params_json : "{}", "skill", skill, sizeof(skill)) != 0) {
        snprintf(result, result_len, "skill_invoke failed: missing skill");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "./skills/%s", skill);
    fp = popen(cmd, "r");
    if (!fp) {
        snprintf(result, result_len, "skill_invoke failed: popen error");
        return -1;
    }

    n = fread(result, 1, result_len - 1, fp);
    result[n] = '\0';
    pclose(fp);
    return 0;
}
