#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_http.h"
#include "../src/llm.h"
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

static void reset_environment(void) {
    mock_http_reset();
    unsetenv("RELIABLE_PROVIDERS");
    unsetenv("OPENROUTER_KEY");
    unsetenv("OPENAI_KEY");
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("MINIMAX_API_KEY");
}

static void test_llm_chat_bearer_auth(void) {
    struct llm_config cfg = {
        .base_url = "https://api.test/v1",
        .model = "test-model",
        .api_key = "bearer-token",
        .auth_style = PROVIDER_AUTH_BEARER,
        .temperature = 0.2f,
        .max_tokens = 32,
        .timeout_ms = 1000,
    };
    struct llm_ctx *ctx = llm_init(&cfg);
    assert(ctx != NULL);

    char response[4096];
    char fixture[1024];
    assert(read_fixture("tests/fixtures/llm_response.json", fixture, sizeof(fixture)) > 0);

    mock_http_set_response(200, fixture);
    assert(llm_chat(ctx, NULL, "Hello", response, sizeof(response)) == 0);
    assert(strcmp(response, "mock response from fixture") == 0);

    const struct mock_http_request *last = mock_http_last_request();
    assert(strcmp(last->method, "POST") == 0);
    assert(strcmp(last->path, "/v1/chat/completions") == 0);
    assert(strcmp(last->headers[1].name, "Authorization") == 0);
    assert(strcmp(last->headers[1].value, "Bearer bearer-token") == 0);

    llm_destroy(ctx);
    reset_environment();
}

static void test_llm_chat_api_key_auth(void) {
    struct llm_config cfg = {
        .base_url = "https://api.test/v1",
        .model = "test-model",
        .api_key = "api-key-token",
        .auth_style = PROVIDER_AUTH_API_KEY,
        .temperature = 0.2f,
        .max_tokens = 32,
        .timeout_ms = 1000,
    };
    struct llm_ctx *ctx = llm_init(&cfg);
    assert(ctx != NULL);

    char response[4096];
    char fixture[1024];
    assert(read_fixture("tests/fixtures/llm_response.json", fixture, sizeof(fixture)) > 0);

    mock_http_set_response(200, fixture);
    assert(llm_chat(ctx, "system prompt", "Hi", response, sizeof(response)) == 0);
    assert(strcmp(response, "mock response from fixture") == 0);

    const struct mock_http_request *last = mock_http_last_request();
    assert(strcmp(last->headers[1].name, "x-api-key") == 0);
    assert(strcmp(last->headers[1].value, "api-key-token") == 0);

    llm_destroy(ctx);
    reset_environment();
}

static void test_llm_chat_429_error(void) {
    struct llm_config cfg = {
        .base_url = "https://api.test/v1",
        .model = "test-model",
        .api_key = "k",
        .auth_style = PROVIDER_AUTH_BEARER,
        .temperature = 0.2f,
        .max_tokens = 32,
        .timeout_ms = 1000,
    };
    struct llm_ctx *ctx = llm_init(&cfg);
    assert(ctx != NULL);

    char response[4096];
    mock_http_set_response(429, "{}");
    assert(llm_chat(ctx, NULL, "Hello", response, sizeof(response)) == -1);

    llm_destroy(ctx);
    reset_environment();
}

static void test_llm_chat_reliable_fallback_attempted(void) {
    struct llm_config cfg = {
        .base_url = "https://primary.provider.test",
        .model = "test-model",
        .api_key = "primary-key",
        .auth_style = PROVIDER_AUTH_BEARER,
        .temperature = 0.2f,
        .max_tokens = 32,
        .timeout_ms = 1000,
    };
    struct llm_ctx *ctx = llm_init(&cfg);
    assert(ctx != NULL);

    char response[4096];
    setenv("RELIABLE_PROVIDERS", "openrouter", 1);
    setenv("OPENROUTER_KEY", "fallback-key", 1);

    mock_http_set_response(503, "{}");
    int rc = llm_chat_reliable(ctx, NULL, "Hello", response, sizeof(response));
    assert(rc == -1);
    assert(mock_http_request_count() >= 2);

    llm_destroy(ctx);
    reset_environment();
}

static void test_llm_chat_empty_response(void) {
    struct llm_config cfg = {
        .base_url = "https://api.test/v1",
        .model = "test-model",
        .api_key = "any-key",
        .auth_style = PROVIDER_AUTH_BEARER,
        .temperature = 0.2f,
        .max_tokens = 32,
        .timeout_ms = 1000,
    };
    struct llm_ctx *ctx = llm_init(&cfg);
    assert(ctx != NULL);

    char response[4096];
    mock_http_set_response(200, "{}");
    assert(llm_chat(ctx, NULL, "Hello", response, sizeof(response)) == 0);
    assert(response[0] == '\0');

    llm_destroy(ctx);
    reset_environment();
}

int main(void) {
    reset_environment();

    test_llm_chat_bearer_auth();
    test_llm_chat_api_key_auth();
    test_llm_chat_429_error();
    test_llm_chat_reliable_fallback_attempted();
    test_llm_chat_empty_response();

    printf("ALL PASS: llm tests\n");
    return 0;
}
