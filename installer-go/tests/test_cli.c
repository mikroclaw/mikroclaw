#include <assert.h>
#include <stdio.h>

#include "../src/cli.h"

int main(void) {
    enum cli_mode mode;
    char *argv1[] = {"mikroclaw", "status", NULL};
    char *argv2[] = {"mikroclaw", "daemon", NULL};
    char *argv3[] = {"mikroclaw", "config", NULL};
    char *argv4[] = {"mikroclaw", "integrations", NULL};
    char *argv5[] = {"mikroclaw", "identity", NULL};

    assert(cli_parse_mode(2, argv1, &mode) == 0);
    assert(mode == CLI_MODE_STATUS);
    assert(cli_parse_mode(2, argv2, &mode) == 0);
    assert(mode == CLI_MODE_DAEMON);
    assert(cli_parse_mode(2, argv3, &mode) == 0);
    assert(mode == CLI_MODE_CONFIG);
    assert(cli_mode_name(CLI_MODE_CONFIG)[0] != '\0');
    assert(cli_parse_mode(2, argv4, &mode) == 0);
    assert(mode == CLI_MODE_INTEGRATIONS);
    assert(cli_parse_mode(2, argv5, &mode) == 0);
    assert(mode == CLI_MODE_IDENTITY);
    assert(cli_mode_name(CLI_MODE_AGENT)[0] != '\0');

    printf("ALL PASS: cli\n");
    return 0;
}
