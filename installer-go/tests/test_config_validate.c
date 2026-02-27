#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/config_validate.h"

int main(void) {
    char err[256];
    char dump[2048];

    unsetenv("BOT_TOKEN");
    unsetenv("OPENROUTER_KEY");
    unsetenv("ROUTER_HOST");
    unsetenv("ROUTER_USER");
    unsetenv("ROUTER_PASS");

    assert(config_validate_required(err, sizeof(err)) != 0);
    assert(strstr(err, "BOT_TOKEN") != NULL);

    setenv("BOT_TOKEN", "bot", 1);
    setenv("OPENROUTER_KEY", "openrouter-secret", 1);
    setenv("ROUTER_HOST", "10.0.0.1", 1);
    setenv("ROUTER_USER", "admin", 1);
    setenv("ROUTER_PASS", "pass", 1);
    setenv("MEMU_API_KEY", "memu-secret", 1);

    assert(config_validate_required(err, sizeof(err)) == 0);
    assert(config_dump_redacted(dump, sizeof(dump)) == 0);
    assert(strstr(dump, "BOT_TOKEN=***") != NULL);
    assert(strstr(dump, "OPENROUTER_KEY=***") != NULL);
    assert(strstr(dump, "MEMU_API_KEY=***") != NULL);
    assert(strstr(dump, "ROUTER_HOST=10.0.0.1") != NULL);

    printf("ALL PASS: config validate\n");
    return 0;
}
