#ifndef MIKROCLAW_CLI_H
#define MIKROCLAW_CLI_H

enum cli_mode {
    CLI_MODE_AGENT = 0,
    CLI_MODE_GATEWAY = 1,
    CLI_MODE_DAEMON = 2,
    CLI_MODE_STATUS = 3,
    CLI_MODE_DOCTOR = 4,
    CLI_MODE_CHANNEL = 5,
    CLI_MODE_CONFIG = 6,
    CLI_MODE_INTEGRATIONS = 7,
    CLI_MODE_IDENTITY = 8,
};

int cli_parse_mode(int argc, char **argv, enum cli_mode *mode_out);
const char *cli_mode_name(enum cli_mode mode);

#endif
