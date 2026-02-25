#include "channel_supervisor.h"

#include <string.h>
#include <time.h>

void channel_supervisor_init(struct channel_supervisor_ctx *ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
}

int channel_supervisor_record_failure(struct channel_supervisor_state *state) {
    int backoff;
    if (!state) {
        return 0;
    }
    state->failures++;
    backoff = 1 << (state->failures > 5 ? 5 : state->failures - 1);
    state->next_retry_at = time(NULL) + backoff;
    return backoff;
}

void channel_supervisor_record_success(struct channel_supervisor_state *state) {
    if (!state) {
        return;
    }
    state->failures = 0;
    state->next_retry_at = 0;
}

int channel_supervisor_should_retry(struct channel_supervisor_state *state) {
    if (!state) {
        return 0;
    }
    if (state->next_retry_at == 0) {
        return 1;
    }
    return time(NULL) >= state->next_retry_at;
}
