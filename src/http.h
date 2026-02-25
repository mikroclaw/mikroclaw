/*
 * MikroClaw - Minimal HTTP/HTTPS Client
 * Designed for embedded/RouterOS environment
 */

#ifndef MIKROCLAW_HTTP_H
#define MIKROCLAW_HTTP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Maximum sizes */
#define HTTP_MAX_RESPONSE_SIZE  65536   /* 64KB max response */
#define HTTP_MAX_HEADER_NAME    64
#define HTTP_MAX_HEADER_VALUE   512
#define HTTP_MAX_HEADERS        16
#define HTTP_MAX_HOSTNAME       256
#define HTTP_TIMEOUT_MS         30000

/* HTTP header structure */
struct http_header {
    char name[HTTP_MAX_HEADER_NAME];
    char value[HTTP_MAX_HEADER_VALUE];
};

/* HTTP response structure */
struct http_response {
    int status_code;
    char body[HTTP_MAX_RESPONSE_SIZE];
    size_t body_len;
    struct http_header headers[HTTP_MAX_HEADERS];
    int num_headers;
};

/* HTTP client handle */
struct http_client;

/* Create HTTP client
 * hostname: server hostname
 * port: server port
 * use_tls: use HTTPS (TLS)
 * returns: client handle or NULL on error
 */
struct http_client *http_client_create(const char *hostname, uint16_t port, bool use_tls);

/* Destroy HTTP client and free resources */
void http_client_destroy(struct http_client *client);

/* Perform HTTP GET request
 * client: HTTP client handle
 * path: URL path (e.g., "/api/v1/resource")
 * headers: additional headers (can be NULL)
 * num_headers: number of additional headers
 * response: response structure (output)
 * returns: 0 on success, negative error code on failure
 */
int http_get(struct http_client *client, const char *path,
             const struct http_header *headers, int num_headers,
             struct http_response *response);

/* Perform HTTP POST request
 * client: HTTP client handle
 * path: URL path
 * headers: additional headers (can be NULL)
 * num_headers: number of additional headers
 * body: request body
 * body_len: request body length
 * response: response structure (output)
 * returns: 0 on success, negative error code on failure
 */
int http_post(struct http_client *client, const char *path,
              const struct http_header *headers, int num_headers,
              const char *body, size_t body_len,
              struct http_response *response);

/* Clear/reset response structure */
void http_response_clear(struct http_response *response);

/* Get header value from response by name
 * returns: header value or NULL if not found
 */
const char *http_response_get_header(const struct http_response *response, const char *name);

/* Error codes */
enum http_error {
    HTTP_OK = 0,
    HTTP_ERR_NOMEM = -1,
    HTTP_ERR_RESOLVE = -2,
    HTTP_ERR_CONNECT = -3,
    HTTP_ERR_TLS = -4,
    HTTP_ERR_SEND = -5,
    HTTP_ERR_RECV = -6,
    HTTP_ERR_TIMEOUT = -7,
    HTTP_ERR_PARSE = -8,
};

#endif /* MIKROCLAW_HTTP_H */
