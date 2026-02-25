#ifndef MIKROCLAW_HTTP_CLIENT_H
#define MIKROCLAW_HTTP_CLIENT_H

#include <curl/curl.h>
#include <stddef.h>

typedef struct curl_http_client {
    CURL *curl;
} curl_http_client;

typedef struct curl_http_response {
    long status_code;
    char *body;
    size_t body_len;
} curl_http_response;

curl_http_client *curl_http_client_create(void);
void curl_http_client_destroy(curl_http_client *client);
int curl_http_get(curl_http_client *client, const char *url, curl_http_response *response);
int curl_http_post(curl_http_client *client, const char *url, const char *body, curl_http_response *response);
void curl_http_response_free(curl_http_response *response);

#endif
