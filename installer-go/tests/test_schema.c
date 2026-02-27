#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/functions.h"

int main(void) {
    char schema[512];

    assert(functions_init() == 0);
    assert(function_get_schema("parse_url", schema, sizeof(schema)) == 0);
    assert(strstr(schema, "type") != NULL);
    assert(strstr(schema, "url") != NULL);
    assert(function_get_schema("shell_exec", schema, sizeof(schema)) == 0);
    assert(strstr(schema, "command") != NULL);
    assert(function_get_schema("file_write", schema, sizeof(schema)) == 0);
    assert(strstr(schema, "content") != NULL);
    assert(function_get_schema("does_not_exist", schema, sizeof(schema)) != 0);
    functions_destroy();

    printf("ALL PASS: schema\n");
    return 0;
}
