/*
 * MikroClaw - mbedTLS Integration Header
 */

#ifndef MBEDTLS_INTEGRATION_H
#define MBEDTLS_INTEGRATION_H

#include <stddef.h>
#include <stdbool.h>

struct mbedtls_ctx {
    void *ssl;
    void *conf;
    void *ctr_drbg;
    void *entropy;
    void *cacert;
    int socket_fd;
    bool initialized;
};

/* Initialize mbedTLS context */
int mbedtls_init(struct mbedtls_ctx *ctx, const char *hostname);

/* Connect socket to mbedTLS */
int mbedtls_connect_socket(struct mbedtls_ctx *ctx, int socket_fd);

/* Perform TLS handshake */
int mbedtls_handshake(struct mbedtls_ctx *ctx);

/* Send data over TLS */
int mbedtls_send(struct mbedtls_ctx *ctx, const void *buf, size_t len);

/* Receive data over TLS */
int mbedtls_recv(struct mbedtls_ctx *ctx, void *buf, size_t len);

/* Close TLS connection */
void mbedtls_tls_close(struct mbedtls_ctx *ctx);

/* Free mbedTLS context */
void mbedtls_tls_free(struct mbedtls_ctx *ctx);

#endif /* MBEDTLS_INTEGRATION_H */
