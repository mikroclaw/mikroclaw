#ifndef MIKROCLAW_LLM_STREAM_H
#define MIKROCLAW_LLM_STREAM_H

#include <stddef.h>

typedef int (*llm_stream_chunk_cb)(const char *chunk, void *user_data);

int llm_sse_extract_text(const char *sse_body, char *out, size_t out_len);
int llm_sse_for_each_chunk(const char *sse_body, llm_stream_chunk_cb cb, void *user_data);

#endif
