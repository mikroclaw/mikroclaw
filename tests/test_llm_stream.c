#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/llm_stream.h"

static int count_chunks(const char *chunk, void *user_data) {
    int *count = (int *)user_data;
    if (chunk && chunk[0] != '\0') {
        (*count)++;
    }
    return 0;
}

int main(void) {
    const char *sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n"
        "data: [DONE]\n\n";
    char out[64];
    int seen = 0;

    assert(llm_sse_extract_text(sse, out, sizeof(out)) == 0);
    assert(strcmp(out, "Hello") == 0);

    assert(llm_sse_for_each_chunk(sse, count_chunks, &seen) == 0);
    assert(seen == 2);

    printf("ALL PASS: llm stream\n");
    return 0;
}
