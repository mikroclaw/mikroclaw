#include "buf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int safe_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;

    if (!buf || !fmt || size == 0) {
        return -1;
    }

    va_start(args, fmt);
    ret = vsnprintf(buf, size, fmt, args);
    va_end(args);

    if (ret < 0 || (size_t)ret >= size) {
        return -1;
    }

    return 0;
}

int buf_append(char *dst, size_t dst_size, const char *src) {
    size_t dst_len;
    size_t src_len;

    if (!dst || !src || dst_size == 0) {
        return -1;
    }

    dst_len = strnlen(dst, dst_size);
    if (dst_len >= dst_size) {
        return -1;
    }

    src_len = strlen(src);
    if (src_len >= (dst_size - dst_len)) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}
