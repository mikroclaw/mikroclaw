#include <assert.h>
#include <stdio.h>

#include "../src/channel_supervisor.h"

int main(void) {
    struct channel_supervisor_ctx ctx;
    int backoff;

    channel_supervisor_init(&ctx);
    assert(ctx.telegram.failures == 0);

    backoff = channel_supervisor_record_failure(&ctx.telegram);
    assert(backoff >= 1);
    assert(ctx.telegram.failures == 1);

    channel_supervisor_record_success(&ctx.telegram);
    assert(ctx.telegram.failures == 0);
    assert(channel_supervisor_should_retry(&ctx.telegram) == 1);

    printf("ALL PASS: channel supervisor\n");
    return 0;
}
