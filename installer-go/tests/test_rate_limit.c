#include <assert.h>
#include <stdio.h>

#include "../src/rate_limit.h"

int main(void) {
    struct rate_limit_ctx *ctx = rate_limit_init(3, 60, 1);
    int retry_after = 0;

    assert(ctx != NULL);

    assert(rate_limit_allow_request(ctx, "127.0.0.1") == 1);
    assert(rate_limit_allow_request(ctx, "127.0.0.1") == 1);
    assert(rate_limit_allow_request(ctx, "127.0.0.1") == 1);
    assert(rate_limit_allow_request(ctx, "127.0.0.1") == 0);

    assert(rate_limit_record_auth_failure(ctx, "127.0.0.1") == 0);
    assert(rate_limit_record_auth_failure(ctx, "127.0.0.1") == 0);
    assert(rate_limit_record_auth_failure(ctx, "127.0.0.1") == 0);
    assert(rate_limit_record_auth_failure(ctx, "127.0.0.1") == 0);
    assert(rate_limit_record_auth_failure(ctx, "127.0.0.1") == 0);
    assert(rate_limit_is_locked(ctx, "127.0.0.1", &retry_after) == 1);
    assert(retry_after > 0);

    rate_limit_record_auth_success(ctx, "127.0.0.1");
    assert(rate_limit_is_locked(ctx, "127.0.0.1", &retry_after) == 0);

    rate_limit_destroy(ctx);
    printf("ALL PASS: rate limit\n");
    return 0;
}
