#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/functions.h"

int main(void) {
    char result[1024];

    setenv("ALLOWED_SHELL_CMDS", "printf,ls", 1);
    assert(functions_init() == 0);

    assert(function_call("shell_exec", "{\"command\":\"printf ok\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);

    assert(function_call("shell_exec", "{\"command\":\"printf ok;uname -a\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "not allowed") != NULL);

    assert(function_call("shell_exec", "{\"command\":\"cat /etc/passwd\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "not allowed") != NULL);

    assert(function_call("shell_exec", "{\"command\":\"printf ../etc/passwd\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "not allowed") != NULL);

    assert(function_call("shell_exec", "{\"command\":\"printf ..\\etc\\passwd\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "not allowed") != NULL);

    assert(function_call("shell_exec", "{\"command\":\"/bin/printf ok\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "not allowed") != NULL);

    setenv("ALLOWED_SHELL_CMDS", "printf,/bin/printf", 1);
    assert(function_call("shell_exec", "{\"command\":\"/bin/printf ok\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);

    assert(function_call("skill_invoke", "{\"skill\":\"demo;uname\",\"params\":\"x\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "invalid skill") != NULL);

    assert(function_call("skill_invoke", "{\"skill\":\"demo\",\"params\":\"x;uname\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "invalid skill") != NULL);

    assert(function_call("file_write", "{\"path\":\"../escape.txt\",\"content\":\"x\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "invalid path") != NULL);

    assert(function_call("file_read", "{\"path\":\"/etc/passwd\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "invalid path") != NULL);

    assert(function_call("file_write", "{\"path\":\"tool_test.txt\",\"content\":\"hello\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);

    assert(function_call("file_read", "{\"path\":\"tool_test.txt\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "hello") == 0);

    unlink("tool_link");
    symlink("/etc/passwd", "tool_link");
    assert(function_call("file_read", "{\"path\":\"tool_link\"}", result, sizeof(result)) != 0);
    assert(strstr(result, "invalid path") != NULL);

    unlink("tool_test.txt");
    unlink("tool_link");
    functions_destroy();

    printf("ALL PASS: tool security\n");
    return 0;
}
