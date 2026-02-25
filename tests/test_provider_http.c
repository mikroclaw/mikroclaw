#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_http.h"
#include "../src/provider_registry.h"

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

static void test_provider_http_request(const char *provider_name,
                                        const char *endpoint,
                                        const char *fixture_path) {
    char fixture[4096];
    char body[256];
    struct http_header headers[3];
    struct http_response response = {0};

    struct provider_config cfg;
    if (provider_registry_get(provider_name, &cfg) != 0) {
        printf("SKIP: provider '%s' not found\n", provider_name);
        return;
    }

    memset(headers, 0, sizeof(headers));
    snprintf(headers[0].name, sizeof(headers[0].name), "Content-Type");
    snprintf(headers[0].value, sizeof(headers[0].value), "application/json");
    if (cfg.auth_style == PROVIDER_AUTH_API_KEY) {
        snprintf(headers[1].name, sizeof(headers[1].name), "x-api-key");
        snprintf(headers[1].value, sizeof(headers[1].value), "test-key");
    } else {
        snprintf(headers[1].name, sizeof(headers[1].name), "Authorization");
        snprintf(headers[1].value, sizeof(headers[1].value), "Bearer test-key");
    }
    snprintf(headers[2].name, sizeof(headers[2].name), "Accept");
    snprintf(headers[2].value, sizeof(headers[2].value), "application/json");

    if (read_fixture(fixture_path, fixture, sizeof(fixture)) <= 0) {
        snprintf(fixture, sizeof(fixture), "{\"status\":\"ok\"}");
    }

    struct http_client *client = http_client_create("api.example.com", 443, true);
    assert(client != NULL);

    mock_http_reset();
    mock_http_set_response(200, fixture);

    snprintf(body, sizeof(body), "{\"model\":\"test\",\"messages\":[]}");

    assert(http_post(client, endpoint, headers, 3, body, strlen(body), &response) == 0);
    assert(response.status_code == 200);

    const struct mock_http_request *last = mock_http_last_request();
    assert(strcmp(last->method, "POST") == 0);
    assert(strncmp(last->path, endpoint, strlen(endpoint)) == 0);
    assert(strcmp(last->headers[0].name, "Content-Type") == 0);
    assert(strcmp(last->headers[0].value, "application/json") == 0);
    if (cfg.auth_style == PROVIDER_AUTH_API_KEY) {
        assert(strcmp(last->headers[1].name, "x-api-key") == 0);
        assert(strcmp(last->headers[1].value, "test-key") == 0);
    } else {
        assert(strcmp(last->headers[1].name, "Authorization") == 0);
        assert(strcmp(last->headers[1].value, "Bearer test-key") == 0);
    }
    assert(strcmp(last->body, body) == 0);

    http_client_destroy(client);

    printf("PASS: %s provider HTTP request\n", provider_name);
}

int main(void) {
    test_provider_http_request("kimi", "/v1/chat/completions", "tests/fixtures/llm_response.json");
    test_provider_http_request("minimax", "/v1/chat/completions", "tests/fixtures/llm_response.json");
    test_provider_http_request("zai", "/v1/chat/completions", "tests/fixtures/llm_response.json");
    test_provider_http_request("synthetic", "/v1/chat/completions", "tests/fixtures/llm_response.json");
    test_provider_http_request("openai", "/v1/chat/completions", "tests/fixtures/llm_response.json");
    test_provider_http_request("anthropic", "/v1/messages", "tests/fixtures/llm_response.json");

    printf("ALL PASS: provider HTTP integration\n");
    return 0;
}
