#include <stdio.h>
#include <string.h>

#include "../src/routeros.h"

struct routeros_ctx_layout {
    void *http;
    char host[256];
    char user[64];
    char pass[128];
    char auth_header[256];
};

int main(void) {
    struct routeros_ctx *ctx = routeros_init("127.0.0.1", 443, "admin", "testpass123");
    if (!ctx) {
        printf("FAIL: routeros_init failed\n");
        return 1;
    }

    struct routeros_ctx_layout *layout = (struct routeros_ctx_layout *) ctx;
    const char *h = layout->auth_header;

    if (strncmp(h, "Basic ", 6) != 0) {
        printf("FAIL: missing Basic prefix (%s)\n", h);
        routeros_destroy(ctx);
        return 1;
    }

    if (strchr(h + 6, ':') != NULL) {
        printf("FAIL: plaintext credentials detected (%s)\n", h);
        routeros_destroy(ctx);
        return 1;
    }

    printf("PASS: routeros auth header encoded\n");
    routeros_destroy(ctx);
    return 0;
}
