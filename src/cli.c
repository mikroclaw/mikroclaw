#include "cli.h"

#include <string.h>

int cli_parse_mode(int argc, char **argv, enum cli_mode *mode_out) {
    if (!mode_out) {
        return -1;
    }
    *mode_out = CLI_MODE_AGENT;
    if (argc < 2 || !argv || !argv[1]) {
        return 0;
    }
    if (strcmp(argv[1], "agent") == 0) {
        *mode_out = CLI_MODE_AGENT;
    } else if (strcmp(argv[1], "gateway") == 0) {
        *mode_out = CLI_MODE_GATEWAY;
    } else if (strcmp(argv[1], "daemon") == 0) {
        *mode_out = CLI_MODE_DAEMON;
    } else if (strcmp(argv[1], "status") == 0) {
        *mode_out = CLI_MODE_STATUS;
    } else if (strcmp(argv[1], "doctor") == 0) {
        *mode_out = CLI_MODE_DOCTOR;
    } else if (strcmp(argv[1], "channel") == 0) {
        *mode_out = CLI_MODE_CHANNEL;
    } else if (strcmp(argv[1], "config") == 0) {
        *mode_out = CLI_MODE_CONFIG;
    } else if (strcmp(argv[1], "integrations") == 0) {
        *mode_out = CLI_MODE_INTEGRATIONS;
    } else if (strcmp(argv[1], "identity") == 0) {
        *mode_out = CLI_MODE_IDENTITY;
    }
    return 0;
}

const char *cli_mode_name(enum cli_mode mode) {
    switch (mode) {
        case CLI_MODE_AGENT: return "agent";
        case CLI_MODE_GATEWAY: return "gateway";
        case CLI_MODE_DAEMON: return "daemon";
        case CLI_MODE_STATUS: return "status";
        case CLI_MODE_DOCTOR: return "doctor";
        case CLI_MODE_CHANNEL: return "channel";
        case CLI_MODE_CONFIG: return "config";
        case CLI_MODE_INTEGRATIONS: return "integrations";
        case CLI_MODE_IDENTITY: return "identity";
        default: return "agent";
    }
}
