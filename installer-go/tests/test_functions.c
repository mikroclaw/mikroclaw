#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/functions.h"

static int test_fn(const char *args_json, char *result_buf, size_t result_len) {
    (void)args_json;
    snprintf(result_buf, result_len, "ok");
    return 0;
}

int main(void) {
    assert(functions_init() == 0);
    assert(function_register("test", "test function", test_fn) == 0);

    char result[64];
    assert(function_call("test", "{}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);

    assert(function_call("parse_url", "{\"url\":\"https://example.com/a\"}", result, sizeof(result)) == 0);
    assert(strstr(result, "\"host\":\"example.com\"") != NULL);
    assert(strstr(result, "\"path\":\"/a\"") != NULL);

    assert(function_call("memory_store", "{\"key\":\"k\",\"value\":\"v\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);
    assert(function_call("memory_recall", "{\"key\":\"k\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "v") == 0);

    assert(function_call("memory_forget", "{\"key\":\"k\"}", result, sizeof(result)) == 0);
    assert(strcmp(result, "ok") == 0);

    setenv("WEBSCRAPE_MOCK_RESPONSE", "{\"title\":\"Example\",\"text\":\"Hello\",\"links\":[]}", 1);
    assert(function_call("web_scrape", "{\"url\":\"https://example.com\"}", result, sizeof(result)) == 0);
    assert(strstr(result, "\"title\"") != NULL);

    char listed[256];
    assert(function_list(listed, sizeof(listed)) == 0);
    assert(strstr(listed, "test") != NULL);

    functions_destroy();
    printf("ALL PASS: functions registry\n");
    return 0;
}
