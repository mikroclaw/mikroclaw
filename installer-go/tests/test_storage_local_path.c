#include <stdio.h>
#include <string.h>

#include "../src/storage_local.h"

int main(void) {
    struct storage_local_ctx *ctx;
    char out[256];
    int ret;

    ctx = storage_local_init("/tmp/mikroclaw-storage-test");
    if (!ctx) {
        printf("FAIL: init\n");
        return 1;
    }

    ret = storage_local_write(ctx, "../escape.txt", "x", 1);
    if (ret == 0) {
        printf("FAIL: traversal write allowed\n");
        storage_local_destroy(ctx);
        return 1;
    }

    ret = storage_local_read(ctx, "../escape.txt", out, sizeof(out));
    if (ret == 0) {
        printf("FAIL: traversal read allowed\n");
        storage_local_destroy(ctx);
        return 1;
    }

    ret = storage_local_write(ctx, "safe/file.txt", "ok", 2);
    if (ret != 0) {
        printf("FAIL: safe write blocked\n");
        storage_local_destroy(ctx);
        return 1;
    }

    memset(out, 0, sizeof(out));
    ret = storage_local_read(ctx, "safe/file.txt", out, sizeof(out));
    if (ret != 0 || strcmp(out, "ok") != 0) {
        printf("FAIL: safe read failed\n");
        storage_local_destroy(ctx);
        return 1;
    }

    storage_local_destroy(ctx);
    printf("ALL PASS\n");
    return 0;
}
