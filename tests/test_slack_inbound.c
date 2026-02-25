#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int slack_parse_inbound(const char *http_request, char *out_text, size_t out_len);

int main(void) {
    char out[256];
    const char *req_ok =
        "POST /slack HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"event\":{\"type\":\"message\",\"text\":\"hello slack\"},\"user\":\"alice\"}";
    const char *req_bot =
        "POST /slack HTTP/1.1\r\n\r\n"
        "{\"event\":{\"type\":\"message\",\"text\":\"bot\",\"bot_id\":\"B123\"}}";

    setenv("SLACK_ALLOWLIST", "alice", 1);
    memset(out, 0, sizeof(out));
    assert(slack_parse_inbound(req_ok, out, sizeof(out)) == 1);
    assert(strcmp(out, "hello slack") == 0);

    memset(out, 0, sizeof(out));
    assert(slack_parse_inbound(req_bot, out, sizeof(out)) == 0);

    setenv("SLACK_ALLOWLIST", "bob", 1);
    memset(out, 0, sizeof(out));
    assert(slack_parse_inbound(req_ok, out, sizeof(out)) == 0);

    printf("ALL PASS: slack inbound parse\n");
    return 0;
}
