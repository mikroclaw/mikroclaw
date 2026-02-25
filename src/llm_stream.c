#include "llm_stream.h"

#include <stdio.h>
#include <string.h>

static const char *next_content_value(const char *start, char *out, size_t out_len, const char **next) {
    const char *needle = "\"content\":\"";
    const char *p;
    const char *q;
    size_t n;

    if (!start || !out || out_len == 0) {
        return NULL;
    }

    p = strstr(start, needle);
    if (!p) {
        return NULL;
    }
    p += strlen(needle);
    q = strchr(p, '"');
    if (!q) {
        return NULL;
    }

    n = (size_t)(q - p);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, p, n);
    out[n] = '\0';
    if (next) {
        *next = q + 1;
    }

    return out;
}

int llm_sse_extract_text(const char *sse_body, char *out, size_t out_len) {
    const char *cursor;
    char chunk[512];
    size_t used = 0;

    if (!sse_body || !out || out_len == 0) {
        return -1;
    }

    out[0] = '\0';
    cursor = sse_body;
    while (next_content_value(cursor, chunk, sizeof(chunk), &cursor)) {
        int wrote = snprintf(out + used, out_len - used, "%s", chunk);
        if (wrote < 0 || (size_t)wrote >= (out_len - used)) {
            return -1;
        }
        used += (size_t)wrote;
    }

    return 0;
}

int llm_sse_for_each_chunk(const char *sse_body, llm_stream_chunk_cb cb, void *user_data) {
    const char *cursor;
    char chunk[512];

    if (!sse_body || !cb) {
        return -1;
    }

    cursor = sse_body;
    while (next_content_value(cursor, chunk, sizeof(chunk), &cursor)) {
        if (cb(chunk, user_data) != 0) {
            return -1;
        }
    }

    return 0;
}
