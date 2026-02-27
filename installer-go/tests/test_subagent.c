#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/subagent.h"

int main(void) {
    struct subagent_ctx *ctx = subagent_init(2, 8);
    char id[64];
    char out[4096];
    int i;

    assert(ctx != NULL);
    assert(subagent_submit(ctx, "analyze", "{\"target\":\"ether1\"}", id, sizeof(id)) == 0);
    assert(id[0] != '\0');

    for (i = 0; i < 50; i++) {
        subagent_poll(ctx);
        usleep(20000);
    }

    assert(subagent_get_json(ctx, id, out, sizeof(out)) == 0);
    assert(strstr(out, "task_id") != NULL);

    assert(subagent_list_json(ctx, out, sizeof(out)) == 0);
    assert(strstr(out, id) != NULL);

    subagent_destroy(ctx);
    printf("ALL PASS: subagent\n");
    return 0;
}
