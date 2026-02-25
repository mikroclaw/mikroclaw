#ifndef MIKROCLAW_TASK_HANDLERS_H
#define MIKROCLAW_TASK_HANDLERS_H

#include <stddef.h>

typedef int (*task_handler_fn)(const char *params_json, char *result, size_t result_len);

task_handler_fn task_handler_resolve(const char *task_type);

int task_handle_investigate(const char *params_json, char *result, size_t result_len);
int task_handle_analyze(const char *params_json, char *result, size_t result_len);
int task_handle_summarize(const char *params_json, char *result, size_t result_len);
int task_handle_skill_invoke(const char *params_json, char *result, size_t result_len);

#endif
