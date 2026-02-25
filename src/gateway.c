/*
 * MikroClaw - Gateway Server Implementation
 */

#include "gateway.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

struct gateway_ctx {
    int listen_fd;
    int port;
    char bind_addr[64];
};

struct gateway_ctx *gateway_init(const struct gateway_config *config) {
    if (!config) return NULL;

    struct gateway_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->port = config->port;
    snprintf(ctx->bind_addr, sizeof(ctx->bind_addr), "%s",
             (config->bind_addr && config->bind_addr[0] != '\0') ? config->bind_addr : "0.0.0.0");
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->listen_fd < 0) {
        free(ctx);
        return NULL;
    }

    /* Allow reuse */
    int opt = 1;
    setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ctx->bind_addr, &addr.sin_addr) != 1) {
        close(ctx->listen_fd);
        free(ctx);
        return NULL;
    }
    addr.sin_port = htons(config->port);
    
    if (bind(ctx->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ctx->listen_fd);
        free(ctx);
        return NULL;
    }
    
    /* Listen */
    if (listen(ctx->listen_fd, 5) < 0) {
        close(ctx->listen_fd);
        free(ctx);
        return NULL;
    }
    
    /* Non-blocking */
    fcntl(ctx->listen_fd, F_SETFL, O_NONBLOCK);

    if (config->port == 0) {
        struct sockaddr_in bound;
        socklen_t bound_len = sizeof(bound);
        memset(&bound, 0, sizeof(bound));
        if (getsockname(ctx->listen_fd, (struct sockaddr *)&bound, &bound_len) == 0) {
            ctx->port = ntohs(bound.sin_port);
        }
    }
    
    return ctx;
}

void gateway_destroy(struct gateway_ctx *ctx) {
    if (!ctx) return;
    close(ctx->listen_fd);
    free(ctx);
}

int gateway_poll(struct gateway_ctx *ctx, char *request, size_t max_request,
                 int *client_fd, int timeout_ms,
                 char *client_ip, size_t client_ip_len) {
    (void)timeout_ms;
    if (!ctx || !request || !client_fd) return -1;
    if (client_ip && client_ip_len > 0) {
        client_ip[0] = '\0';
    }
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    *client_fd = accept(ctx->listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (*client_fd < 0) {
        return 0; /* No connection */
    }

    if (client_ip && client_ip_len > 0) {
        const char *ip = inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, (socklen_t)client_ip_len);
        if (!ip) {
            snprintf(client_ip, client_ip_len, "unknown");
        }
    }
    
    /* Read request */
    ssize_t n = recv(*client_fd, request, max_request - 1, 0);
    if (n > 0) {
        request[n] = '\0';
        return 1; /* Got request */
    }
    
    close(*client_fd);
    return 0;
}

int gateway_respond(int client_fd, const char *response) {
    if (client_fd < 0 || !response) return -1;
    
    send(client_fd, response, strlen(response), 0);
    close(client_fd);
    return 0;
}

int gateway_port(const struct gateway_ctx *ctx) {
    if (!ctx) {
        return -1;
    }
    return ctx->port;
}
