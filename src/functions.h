#ifndef MIKROCLAW_FUNCTIONS_H
#define MIKROCLAW_FUNCTIONS_H

#include <stddef.h>

typedef int (*function_fn)(const char *args_json, char *result_buf, size_t result_len);

int functions_init(void);
void functions_destroy(void);

int function_register(const char *name, const char *description, function_fn fn);
int function_register_with_schema(const char *name, const char *description,
                                  const char *schema_json, function_fn fn);
int function_call(const char *name, const char *args_json,
                  char *result_buf, size_t result_len);
int function_list(char *out, size_t max_len);
int function_get_schema(const char *name, char *out, size_t max_len);

#endif
