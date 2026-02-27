#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int discord_parse_inbound(const char *http_request, char *out_text, size_t out_len);

int main(void) {
    char out[256];
    const char *req_ok =
        "POST /discord HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"content\":\"hello discord\",\"author\":{\"bot\":false},\"username\":\"alice\"}";
    const char *req_bot =
        "POST /discord HTTP/1.1\r\n\r\n"
        "{\"content\":\"ignore\",\"author\":{\"bot\":true}}";

    setenv("DISCORD_ALLOWLIST", "alice", 1);
    memset(out, 0, sizeof(out));
    assert(discord_parse_inbound(req_ok, out, sizeof(out)) == 1);
    assert(strcmp(out, "hello discord") == 0);

    memset(out, 0, sizeof(out));
    assert(discord_parse_inbound(req_bot, out, sizeof(out)) == 0);

    setenv("DISCORD_ALLOWLIST", "bob", 1);
    memset(out, 0, sizeof(out));
    assert(discord_parse_inbound(req_ok, out, sizeof(out)) == 0);

    printf("ALL PASS: discord inbound parse\n");
    return 0;
}
