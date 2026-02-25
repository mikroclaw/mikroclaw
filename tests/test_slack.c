#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/channels/slack.h"

int main(void) {
    const char *webhook = getenv("TEST_SLACK_WEBHOOK_URL");
    struct slack_config cfg = {0};

    if (!webhook || webhook[0] == '\0') {
        printf("SKIP: set TEST_SLACK_WEBHOOK_URL to run slack webhook test\n");
        return 0;
    }
    snprintf(cfg.webhook_url, sizeof(cfg.webhook_url), "%s", webhook);

    struct slack_ctx *ctx = slack_init(&cfg);
    assert(ctx != NULL);

    assert(slack_send(ctx, "hello") == 0);
    slack_destroy(ctx);

    printf("ALL PASS: slack\n");
    return 0;
}
