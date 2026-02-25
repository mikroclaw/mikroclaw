/*
 * MikroClaw - Minimal HTTP/HTTPS Client Implementation
 * Using mbedTLS for TLS support
 */

#include "http.h"
#include "../vendor/mbedtls_integration.h"
#include "../src/mikroclaw_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

/* HTTP client structure */
struct http_client {
    char hostname[HTTP_MAX_HOSTNAME];
    uint16_t port;
    bool use_tls;
    int socket_fd;
    struct mbedtls_ctx tls_ctx;
    int connected;
};

/* Set socket timeout */
static int set_timeout(int fd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    return 0;
}

/* Create HTTP client */
struct http_client *http_client_create(const char *hostname, uint16_t port, bool use_tls) {
    struct http_client *client = calloc(1, sizeof(*client));
    if (!client) return NULL;
    
    strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    client->port = port;
    client->use_tls = use_tls;
    client->socket_fd = -1;
    client->connected = 0;
    
    /* Initialize TLS context if needed */
    if (use_tls) {
        if (mbedtls_init(&client->tls_ctx, hostname) != 0) {
            free(client);
            return NULL;
        }
    }
    
    return client;
}

/* Destroy HTTP client */
void http_client_destroy(struct http_client *client) {
    if (!client) return;
    
    if (client->use_tls) {
        mbedtls_tls_free(&client->tls_ctx);
    }
    
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    
    free(client);
}

/* Connect to server */
static int http_connect(struct http_client *client) {
    if (client->connected) return 0;
    
    /* Resolve hostname */
    struct hostent *host = gethostbyname(client->hostname);
    if (!host) {
        return HTTP_ERR_RESOLVE;
    }
    
    /* Create socket */
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        return HTTP_ERR_CONNECT;
    }
    
    /* Set timeout */
    if (set_timeout(client->socket_fd, HTTP_TIMEOUT_MS) < 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
        return HTTP_ERR_CONNECT;
    }
    
    /* Connect */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client->port);
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    
    if (connect(client->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
        return HTTP_ERR_CONNECT;
    }
    
    /* TLS handshake if needed */
    if (client->use_tls) {
        client->tls_ctx.socket_fd = client->socket_fd;
        if (mbedtls_connect_socket(&client->tls_ctx, client->socket_fd) != 0 ||
            mbedtls_handshake(&client->tls_ctx) != 0) {
            close(client->socket_fd);
            client->socket_fd = -1;
            return HTTP_ERR_TLS;
        }
    }
    
    client->connected = 1;
    return 0;
}

/* Send data */
static int http_send(struct http_client *client, const char *data, size_t len) {
    size_t sent = 0;
    
    while (sent < len) {
        ssize_t n = client->use_tls ? 
            mbedtls_send(&client->tls_ctx, data + sent, len - sent) :
            send(client->socket_fd, data + sent, len - sent, 0);
        
        if (n < 0) {
            if (errno == EINTR) continue;
            return HTTP_ERR_SEND;
        }
        
        if (n == 0) {
            return HTTP_ERR_SEND;  /* Connection closed */
        }
        
        sent += n;
    }
    
    return 0;
}

/* Receive data */
static int http_recv(struct http_client *client, char *buf, size_t max_len, size_t *out_len) {
    size_t total = 0;
    
    while (total < max_len - 1) {
        ssize_t n = client->use_tls ?
            mbedtls_recv(&client->tls_ctx, buf + total, max_len - total - 1) :
            recv(client->socket_fd, buf + total, max_len - total - 1, 0);
        
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  /* Timeout */
            return HTTP_ERR_RECV;
        }
        
        if (n == 0) {
            break;  /* Connection closed */
        }
        
        total += n;
    }
    
    buf[total] = '\0';
    *out_len = total;
    return 0;
}

/* Build HTTP request */
static int build_request(char *buf, size_t max_len,
                         const char *method, const char *path,
                         const char *hostname,
                         const struct http_header *headers, int num_headers,
                         const char *body, size_t body_len) {
    int n = snprintf(buf, max_len,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: MikroClaw/0.1.0\r\n"
        "Accept: application/json\r\n",
        method, path, hostname);
    
    if (body && body_len > 0) {
        n += snprintf(buf + n, max_len - n,
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n",
            body_len);
    }
    
    /* Add custom headers */
    for (int i = 0; i < num_headers && n < (int)max_len - 1; i++) {
        n += snprintf(buf + n, max_len - n, "%s: %s\r\n",
                      headers[i].name, headers[i].value);
    }
    
    n += snprintf(buf + n, max_len - n, "\r\n");
    
    if (body && body_len > 0 && n + (int)body_len < (int)max_len) {
        memcpy(buf + n, body, body_len);
        n += body_len;
    }
    
    return n;
}

/* Parse HTTP response */
static int parse_response(const char *data, size_t len, struct http_response *response) {
    http_response_clear(response);
    
    /* Parse status line */
    int status;
    if (sscanf(data, "HTTP/%*s %d", &status) != 1) {
        return HTTP_ERR_PARSE;
    }
    response->status_code = status;
    
    /* Find body start */
    const char *body_start = strstr(data, "\r\n\r\n");
    if (!body_start) {
        /* Try single newlines */
        body_start = strstr(data, "\n\n");
        if (!body_start) {
            return HTTP_ERR_PARSE;
        }
        body_start += 2;
    } else {
        body_start += 4;
    }
    
    /* Parse headers */
    const char *header_start = strstr(data, "\r\n");
    if (header_start) {
        header_start += 2;
        
        while (header_start < body_start && response->num_headers < HTTP_MAX_HEADERS) {
            const char *line_end = strstr(header_start, "\r\n");
            if (!line_end || line_end >= body_start) break;
            
            /* Parse header line */
            char name[HTTP_MAX_HEADER_NAME];
            char value[HTTP_MAX_HEADER_VALUE];
            if (sscanf(header_start, "%63[^:]: %511[^\r]", name, value) == 2) {
                strncpy(response->headers[response->num_headers].name, name, 
                        HTTP_MAX_HEADER_NAME - 1);
                strncpy(response->headers[response->num_headers].value, value,
                        HTTP_MAX_HEADER_VALUE - 1);
                response->num_headers++;
            }
            
            header_start = line_end + 2;
        }
    }
    
    /* Extract body */
    size_t body_len = len - (body_start - data);
    if (body_len > 0) {
        if (body_len > HTTP_MAX_RESPONSE_SIZE - 1) {
            body_len = HTTP_MAX_RESPONSE_SIZE - 1;
        }
        memcpy(response->body, body_start, body_len);
        response->body[body_len] = '\0';
        response->body_len = body_len;
    }
    
    return 0;
}

/* HTTP GET */
int http_get(struct http_client *client, const char *path,
             const struct http_header *headers, int num_headers,
             struct http_response *response) {
    int ret;
    
    ret = http_connect(client);
    if (ret != 0) return ret;
    
    char request[4096];
    int req_len = build_request(request, sizeof(request),
                                "GET", path, client->hostname,
                                headers, num_headers, NULL, 0);
    
    ret = http_send(client, request, req_len);
    if (ret != 0) return ret;
    
    char raw_response[HTTP_MAX_RESPONSE_SIZE + 1024];
    size_t resp_len;
    ret = http_recv(client, raw_response, sizeof(raw_response), &resp_len);
    if (ret != 0) return ret;
    
    return parse_response(raw_response, resp_len, response);
}

/* HTTP POST */
int http_post(struct http_client *client, const char *path,
              const struct http_header *headers, int num_headers,
              const char *body, size_t body_len,
              struct http_response *response) {
    int ret;
    
    ret = http_connect(client);
    if (ret != 0) return ret;
    
    char request[4096 + 8192];  /* Allow for larger POST bodies */
    int req_len = build_request(request, sizeof(request),
                                "POST", path, client->hostname,
                                headers, num_headers, body, body_len);
    
    ret = http_send(client, request, req_len);
    if (ret != 0) return ret;
    
    char raw_response[HTTP_MAX_RESPONSE_SIZE + 1024];
    size_t resp_len;
    ret = http_recv(client, raw_response, sizeof(raw_response), &resp_len);
    if (ret != 0) return ret;
    
    return parse_response(raw_response, resp_len, response);
}

/* Clear response */
void http_response_clear(struct http_response *response) {
    memset(response, 0, sizeof(*response));
}

/* Get header value */
const char *http_response_get_header(const struct http_response *response, const char *name) {
    for (int i = 0; i < response->num_headers; i++) {
        if (strcasecmp(response->headers[i].name, name) == 0) {
            return response->headers[i].value;
        }
    }
    return NULL;
}
