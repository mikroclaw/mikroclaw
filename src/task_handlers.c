#include "task_handlers.h"

#include <stdio.h>
#include <string.h>

task_handler_fn task_handler_resolve(const char *task_type) {
    if (!task_type) {
        return NULL;
    }
    if (strcmp(task_type, "investigate") == 0) {
        return task_handle_investigate;
    }
    if (strcmp(task_type, "analyze") == 0) {
        return task_handle_analyze;
    }
    if (strcmp(task_type, "summarize") == 0) {
        return task_handle_summarize;
    }
    if (strcmp(task_type, "skill_invoke") == 0) {
        return task_handle_skill_invoke;
    }
    return NULL;
}
