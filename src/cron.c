/*
 * MikroClaw - Cron/Scheduler Implementation
 */

#include "cron.h"
#include "routeros.h"
#include "json.h"
#include <string.h>
#include <stdio.h>

int cron_add(struct routeros_ctx *router, const char *name,
             const char *schedule, const char *command) {
    if (!router || !name || !schedule || !command) return -1;
    
    char body[1024];
    if (cron_build_add_body(name, schedule, command, body, sizeof(body)) != 0) {
        return -1;
    }
    
    char result[4096];
    return routeros_post(router, "/system/scheduler/add", body, result, sizeof(result));
}

int cron_list(struct routeros_ctx *router, char *out, size_t max_len) {
    if (!router || !out) return -1;
    return routeros_get(router, "/system/scheduler/print", out, max_len);
}

int cron_remove(struct routeros_ctx *router, const char *name) {
    if (!router || !name) return -1;
    
    char path[256];
    snprintf(path, sizeof(path), "/system/scheduler/remove=%s", name);
    
    char result[4096];
    return routeros_post(router, path, "", result, sizeof(result));
}

int cron_run(struct routeros_ctx *router, const char *name) {
    if (!router || !name) return -1;
    
    char path[256];
    snprintf(path, sizeof(path), "/system/scheduler/run=%s", name);
    
    char result[4096];
    return routeros_post(router, path, "", result, sizeof(result));
}

int cron_build_add_body(const char *name, const char *schedule, const char *command,
                        char *body, size_t body_len) {
    char esc_name[256];
    char esc_schedule[256];
    char esc_command[512];

    if (!name || !schedule || !command || !body || body_len == 0) {
        return -1;
    }
    if (json_escape(name, esc_name, sizeof(esc_name)) != 0) {
        return -1;
    }
    if (json_escape(schedule, esc_schedule, sizeof(esc_schedule)) != 0) {
        return -1;
    }
    if (json_escape(command, esc_command, sizeof(esc_command)) != 0) {
        return -1;
    }
    if (snprintf(body, body_len,
                 "{\"name\":\"%s\",\"interval\":\"%s\",\"script\":\"%s\"}",
                 esc_name, esc_schedule, esc_command) < 0) {
        return -1;
    }

    return 0;
}
