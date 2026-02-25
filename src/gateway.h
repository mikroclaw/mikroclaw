/*
 * MikroClaw - Gateway Server Header
 */

#ifndef GATEWAY_H
#define GATEWAY_H
#include <stddef.h>
#include <stdint.h>

struct gateway_ctx;

struct gateway_config {
    uint16_t port;
    const char *bind_addr;
};

struct gateway_ctx *gateway_init(const struct gateway_config *config);
void gateway_destroy(struct gateway_ctx *ctx);
int gateway_poll(struct gateway_ctx *ctx, char *request, size_t max_request,
                 int *client_fd, int timeout_ms,
                 char *client_ip, size_t client_ip_len);
int gateway_respond(int client_fd, const char *response);
int gateway_port(const struct gateway_ctx *ctx);

#endif /* GATEWAY_H */
