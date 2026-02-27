#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/channels/discord.h"

int main(void) {
    const char *webhook = getenv("TEST_DISCORD_WEBHOOK_URL");
    struct discord_config cfg = {0};

    if (!webhook || webhook[0] == '\0') {
        printf("SKIP: set TEST_DISCORD_WEBHOOK_URL to run discord webhook test\n");
        return 0;
    }
    snprintf(cfg.webhook_url, sizeof(cfg.webhook_url), "%s", webhook);

    struct discord_ctx *ctx = discord_init(&cfg);
    assert(ctx != NULL);

    assert(discord_send(ctx, "hello") == 0);
    discord_destroy(ctx);

    printf("ALL PASS: discord\n");
    return 0;
}
