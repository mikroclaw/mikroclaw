#include "mock_http.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct http_client {
    char hostname[HTTP_MAX_HOSTNAME];
    unsigned short port;
    bool use_tls;
};

static int g_next_status = HTTP_OK;
static char g_next_body[HTTP_MAX_RESPONSE_SIZE];
static size_t g_next_body_len;
static struct mock_http_request g_last_request;
static int g_request_count;

void mock_http_reset(void) {
    memset(&g_last_request, 0, sizeof(g_last_request));
    g_next_status = HTTP_OK;
    g_next_body[0] = '\0';
    g_next_body_len = 0;
    g_request_count = 0;
}

void mock_http_set_response(int status_code, const char *body) {
    if (status_code != 0) {
        g_next_status = status_code;
    }

    if (body) {
        g_next_body_len = strlen(body);
        if (g_next_body_len >= sizeof(g_next_body)) {
            g_next_body_len = sizeof(g_next_body) - 1;
        }
        strncpy(g_next_body, body, g_next_body_len);
        g_next_body[g_next_body_len] = '\0';
    } else {
        g_next_body[0] = '\0';
        g_next_body_len = 0;
    }
}

const struct mock_http_request *mock_http_last_request(void) {
    return &g_last_request;
}

int mock_http_request_count(void) {
    return g_request_count;
}

struct http_client *http_client_create(const char *hostname, uint16_t port, bool use_tls) {
    struct http_client *client = calloc(1, sizeof(*client));
    if (!client) {
        return NULL;
    }

    if (hostname) {
        strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    }
    client->port = port;
    client->use_tls = use_tls;
    return client;
}

void http_client_destroy(struct http_client *client) {
    free(client);
}

void http_response_clear(struct http_response *response) {
    if (!response) {
        return;
    }

    response->status_code = 0;
    response->body_len = 0;
    response->body[0] = '\0';
    response->num_headers = 0;
}

static void copy_headers(struct http_response *response, const struct http_header *headers, int num_headers) {
    if (!response) {
        return;
    }

    for (int i = 0; i < num_headers && i < HTTP_MAX_HEADERS; i++) {
        if (headers[i].name[0] == '\0') {
            continue;
        }

        strncpy(response->headers[response->num_headers].name,
                headers[i].name,
                sizeof(response->headers[response->num_headers].name) - 1);
        strncpy(response->headers[response->num_headers].value,
                headers[i].value,
                sizeof(response->headers[response->num_headers].value) - 1);
        response->num_headers++;
    }
}

static void set_mock_response(struct http_response *response) {
    if (!response) {
        return;
    }

    response->status_code = g_next_status;
    response->body_len = g_next_body_len;
    memcpy(response->body, g_next_body, g_next_body_len);
    response->body[g_next_body_len] = '\0';
}

static void record_request(const char *method,
                          const char *path,
                          const struct http_header *headers,
                          int num_headers,
                          const char *body,
                          size_t body_len) {
    memset(&g_last_request, 0, sizeof(g_last_request));
    g_request_count++;

    strncpy(g_last_request.method, method, sizeof(g_last_request.method) - 1);
    if (path) {
        strncpy(g_last_request.path, path, sizeof(g_last_request.path) - 1);
    }

    for (int i = 0; i < num_headers && i < HTTP_MAX_HEADERS; i++) {
        strncpy(g_last_request.headers[g_last_request.num_headers].name,
                headers[i].name,
                sizeof(g_last_request.headers[g_last_request.num_headers].name) - 1);
        strncpy(g_last_request.headers[g_last_request.num_headers].value,
                headers[i].value,
                sizeof(g_last_request.headers[g_last_request.num_headers].value) - 1);
        g_last_request.num_headers++;
    }

    if (body && body_len > 0) {
        if (body_len >= sizeof(g_last_request.body)) {
            body_len = sizeof(g_last_request.body) - 1;
        }
        memcpy(g_last_request.body, body, body_len);
        g_last_request.body[body_len] = '\0';
        g_last_request.body_len = body_len;
    }
}

int http_get(struct http_client *client, const char *path,
            const struct http_header *headers, int num_headers,
            struct http_response *response) {
    (void)client;
    record_request("GET", path, headers, num_headers, NULL, 0);
    set_mock_response(response);
    return 0;
}

int http_post(struct http_client *client, const char *path,
             const struct http_header *headers, int num_headers,
             const char *body, size_t body_len,
             struct http_response *response) {
    (void)client;
    record_request("POST", path, headers, num_headers, body, body_len);
    set_mock_response(response);
    return 0;
}

const char *http_response_get_header(const struct http_response *response, const char *name) {
    if (!response || !name) {
        return NULL;
    }

    for (int i = 0; i < response->num_headers; i++) {
        if (strncmp(response->headers[i].name, name, sizeof(response->headers[i].name)) == 0) {
            return response->headers[i].value;
        }
    }
    return NULL;
}
