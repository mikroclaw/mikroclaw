#include "../task_handlers.h"
#include "../memu_client.h"

#include <stdio.h>

int task_handle_summarize(const char *params_json, char *result, size_t result_len) {
    char buf[2048];
    (void)params_json;
    if (memu_retrieve("summarize recent conversation", "rag", buf, sizeof(buf)) != 0) {
        snprintf(result, result_len, "summarize failed: memu unavailable");
        return -1;
    }
    snprintf(result, result_len, "%s", buf);
    return 0;
}
