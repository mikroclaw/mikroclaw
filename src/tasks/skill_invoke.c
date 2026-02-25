#include "../task_handlers.h"

#include "../json.h"
#include <stdio.h>
#include <string.h>

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
