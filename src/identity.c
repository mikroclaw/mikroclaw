#include "identity.h"

#include "memu_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void random_id(char *out, size_t out_len) {
    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);
    snprintf(out, out_len, "%08x-%04x-%04x-%04x-%08x",
             rand(), rand() & 0xFFFF, rand() & 0xFFFF,
             rand() & 0xFFFF, rand());
}

int identity_get(char *out, size_t out_len) {
    const char *env;

    if (!out || out_len == 0) {
        return -1;
    }

    env = getenv("AGENT_ID");
    if (env && env[0] != '\0') {
        snprintf(out, out_len, "%s", env);
        return 0;
    }

    if (memu_retrieve("agent_id", "keyword", out, out_len) == 0 && out[0] != '\0') {
        return 0;
    }

    random_id(out, out_len);
    (void)memu_memorize(out, "identity", "agent");
    return 0;
}

int identity_rotate(char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return -1;
    }
    random_id(out, out_len);
    (void)memu_memorize(out, "identity", "agent");
    return 0;
}
