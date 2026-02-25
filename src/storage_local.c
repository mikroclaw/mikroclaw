/*
 * MikroClaw - Local Storage Implementation
 */

#include "storage_local.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static int storage_local_is_safe_path(const char *path) {
    if (!path || path[0] == '\0') return 0;
    if (path[0] == '/') return 0;

    for (const char *p = path; *p; p++) {
        if (p[0] == '.' && p[1] == '.' && (p == path || p[-1] == '/') &&
            (p[2] == '\0' || p[2] == '/')) {
            return 0;
        }
    }

    return 1;
}

struct storage_local_ctx {
    char path[256];
};

struct storage_local_ctx *storage_local_init(const char *path) {
    if (!path) return NULL;
    
    struct storage_local_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    
    /* Create directory if needed */
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0755);
    }
    
    return ctx;
}

void storage_local_destroy(struct storage_local_ctx *ctx) {
    free(ctx);
}

int storage_local_read(struct storage_local_ctx *ctx, const char *path,
                       char *out, size_t max_len) {
    if (!ctx || !path || !out) return -1;
    if (!storage_local_is_safe_path(path)) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", ctx->path, path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) return -1;
    
    size_t n = fread(out, 1, max_len - 1, f);
    fclose(f);
    
    out[n] = '\0';
    return (n > 0) ? 0 : -1;
}

int storage_local_write(struct storage_local_ctx *ctx, const char *path,
                        const char *data, size_t len) {
    if (!ctx || !path || !data) return -1;
    if (!storage_local_is_safe_path(path)) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", ctx->path, path);
    
    /* Create parent dirs if needed */
    char *p = strrchr(full_path, '/');
    if (p) {
        *p = '\0';
        mkdir(full_path, 0755);
        *p = '/';
    }
    
    FILE *f = fopen(full_path, "w");
    if (!f) return -1;
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    return (written == len) ? 0 : -1;
}
