#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../src/mikroclaw_config.h"
#undef ENABLE_GATEWAY

#define static
#include "../src/mikroclaw.c"
#undef static

static int mock_telegram_send_calls;
static char mock_telegram_last_message[4096];

int telegram_send(struct telegram_ctx *ctx, const char *chat_id, const char *message) {
    (void)ctx;
    (void)chat_id;
    mock_telegram_send_calls++;
    if (message) {
        snprintf(mock_telegram_last_message, sizeof(mock_telegram_last_message), "%s", message);
    } else {
        mock_telegram_last_message[0] = '\0';
    }
    return 0;
}

int telegram_health_check(struct telegram_ctx *ctx) {
    (void)ctx;
    return 1;
}

int telegram_poll(struct telegram_ctx *ctx, struct telegram_message *msg) {
    (void)ctx;
    (void)msg;
    return 0;
}

int function_call(const char *name, const char *args_json,
                 char *result_buf, size_t result_len) {
    (void)name;
    (void)args_json;
    (void)result_buf;
    (void)result_len;
    return -1;
}

int memu_memorize(const char *content, const char *modality, const char *user_id) {
    (void)content;
    (void)modality;
    (void)user_id;
    return 0;
}

int llm_chat_reliable(struct llm_ctx *ctx, const char *system_prompt, const char *prompt,
                      char *out, size_t out_len) {
    (void)ctx;
    (void)system_prompt;
    (void)prompt;
    if (out && out_len > 0) {
        out[0] = '\0';
    }
    return 0;
}

int routeros_execute(struct routeros_ctx *ctx, const char *command,
                    char *output, size_t max_output) {
    (void)ctx;
    (void)command;
    if (output && max_output > 0) {
        output[0] = '\0';
    }
    return 0;
}

int channel_supervisor_record_failure(struct channel_supervisor_state *state) {
    (void)state;
    return 0;
}

void channel_supervisor_record_success(struct channel_supervisor_state *state) {
    (void)state;
}

static void test_build_session_id(void) {
    char session[64];

    build_session_id(session, sizeof(session), "12345");
    assert(strcmp(session, "telegram:12345") == 0);

    build_session_id(session, sizeof(session), NULL);
    assert(strcmp(session, "telegram:") == 0);
}

static void test_http_body_from_request(void) {
    const char *request;

    request = "POST /tasks HTTP/1.1\r\n\r\nalpha=1";
    assert(strcmp(http_body_from_request(request), "alpha=1") == 0);

    request = "PING\r\nno_separator";
    assert(strcmp(http_body_from_request(request), request) == 0);
}

static void test_http_response_builders(void) {
    char response[256];

    build_http_text_response("ok", response, sizeof(response));
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "Content-Length: 2") != NULL);
    assert(strstr(response, "\r\n\r\nok") != NULL);

    build_http_json_response(201, "Created", "{\"status\":\"x\"}", response, sizeof(response));
    assert(strstr(response, "HTTP/1.1 201 Created") != NULL);
    assert(strstr(response, "{\"status\":\"x\"}") != NULL);
}

static void test_parse_and_route_request(void) {
    char method[16];
    char path[64];
    const char *request;
    int route;

    request = "GET /health HTTP/1.1";
    assert(parse_request_line(request, method, sizeof(method), path, sizeof(path)) == 0);
    assert(strcmp(method, "GET") == 0);
    assert(strcmp(path, "/health") == 0);

    if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        route = 1;
    } else {
        route = 0;
    }
    assert(route == 1);

    request = "GET";
    assert(parse_request_line(request, method, sizeof(method), path, sizeof(path)) != 0);
}

static void test_extract_json_string(void) {
    char out[64];

    assert(extract_json_string("{\"type\":\"task\",\"v\":1}", "type", out, sizeof(out)) == 0);
    assert(strcmp(out, "task") == 0);

    assert(extract_json_string("{\"type\":\"task\",\"v\":1}", "missing", out, sizeof(out)) == -1);
    assert(extract_json_string("{\"type\":\"broken", "type", out, sizeof(out)) == -1);
}

static void test_send_reply_routes_to_telegram(void) {
    struct mikroclaw_ctx ctx = {0};

    ctx.telegram = (struct telegram_ctx *)(uintptr_t)1;

    mock_telegram_send_calls = 0;
    memset(mock_telegram_last_message, 0, sizeof(mock_telegram_last_message));

    send_reply(NULL, REPLY_TELEGRAM, NULL, -1, "hello");
    assert(mock_telegram_send_calls == 0);

    send_reply(&ctx, REPLY_TELEGRAM, "123", -1, "hello");
    assert(mock_telegram_send_calls == 1);
    assert(strstr(mock_telegram_last_message, "hello") != NULL);
}

int main(void) {
    test_build_session_id();
    test_http_body_from_request();
    test_http_response_builders();
    test_parse_and_route_request();
    test_extract_json_string();
    test_send_reply_routes_to_telegram();

    printf("ALL PASS: mikroclaw helpers\n");
    return 0;
}
