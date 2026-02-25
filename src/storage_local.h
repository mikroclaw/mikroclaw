/*
 * MikroClaw - Local Storage Header
 */

#ifndef STORAGE_LOCAL_H
#define STORAGE_LOCAL_H

#include <stddef.h>

struct storage_local_ctx;

struct storage_local_ctx *storage_local_init(const char *path);
void storage_local_destroy(struct storage_local_ctx *ctx);
int storage_local_read(struct storage_local_ctx *ctx, const char *path,
                       char *out, size_t max_len);
int storage_local_write(struct storage_local_ctx *ctx, const char *path,
                        const char *data, size_t len);

#endif /* STORAGE_LOCAL_H */
