#ifndef TEST_MOCK_HTTP_H
#define TEST_MOCK_HTTP_H

#include <stddef.h>

#include "../src/http.h"

struct mock_http_request {
    char method[8];
    char path[HTTP_MAX_HEADER_NAME + HTTP_MAX_HOSTNAME];
    struct http_header headers[HTTP_MAX_HEADERS];
    int num_headers;
    char body[4096];
    size_t body_len;
};

void mock_http_reset(void);
void mock_http_set_response(int status_code, const char *body);
const struct mock_http_request *mock_http_last_request(void);
int mock_http_request_count(void);

#endif
