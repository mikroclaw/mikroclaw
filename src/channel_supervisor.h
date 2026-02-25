#ifndef MIKROCLAW_CHANNEL_SUPERVISOR_H
#define MIKROCLAW_CHANNEL_SUPERVISOR_H

#include <time.h>

struct channel_supervisor_state {
    int failures;
    time_t next_retry_at;
};

struct channel_supervisor_ctx {
    struct channel_supervisor_state telegram;
    struct channel_supervisor_state discord;
    struct channel_supervisor_state slack;
};

void channel_supervisor_init(struct channel_supervisor_ctx *ctx);
int channel_supervisor_record_failure(struct channel_supervisor_state *state);
void channel_supervisor_record_success(struct channel_supervisor_state *state);
int channel_supervisor_should_retry(struct channel_supervisor_state *state);

#endif
