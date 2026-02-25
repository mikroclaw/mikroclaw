#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_http.h"

static int read_fixture(const char *path, char *out, size_t out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }

    size_t n = fread(out, 1, out_size - 1, f);
    out[n] = '\0';
    fclose(f);
    return (int)n;
}

int main(void) {
    char fixture[4096];
    char body[128] = "{\"hello\":\"world\"}";
    struct http_header headers[2] = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer test"},
    };
    struct http_response response = {0};

    struct http_client *client = http_client_create("api.example.com", 443, true);
    assert(client != NULL);

    assert(read_fixture("tests/fixtures/llm_response.json", fixture, sizeof(fixture)) > 0);
    mock_http_reset();
    mock_http_set_response(200, fixture);

    assert(http_post(client, "/v1/chat/completions", headers, 2, body, strlen(body), &response) == 0);
    assert(response.status_code == 200);
    assert(strcmp(response.body, fixture) == 0);

    const struct mock_http_request *last = mock_http_last_request();
    assert(strcmp(last->method, "POST") == 0);
    assert(strcmp(last->path, "/v1/chat/completions") == 0);
    assert(strcmp(last->headers[0].name, "Content-Type") == 0);
    assert(strcmp(last->headers[0].value, "application/json") == 0);
    assert(strcmp(last->body, body) == 0);

    mock_http_set_response(204, "{}");
    assert(http_get(client, "/status", headers, 0, &response) == 0);
    assert(response.status_code == 204);
    assert(strcmp(response.body, "{}") == 0);
    assert(mock_http_request_count() == 2);

    http_client_destroy(client);

    printf("ALL PASS: mock_http framework\n");
    return 0;
}
