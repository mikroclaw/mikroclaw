/*
 * MikroClaw - mbedTLS Integration Implementation
 */

#include "mbedtls_integration.h"
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/x509_crt.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int mbedtls_init(struct mbedtls_ctx *ctx, const char *hostname) {
    if (!ctx || !hostname || hostname[0] == '\0') {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->socket_fd = -1;
    
    /* Allocate structures */
    ctx->ssl = calloc(1, sizeof(mbedtls_ssl_context));
    ctx->conf = calloc(1, sizeof(mbedtls_ssl_config));
    ctx->ctr_drbg = calloc(1, sizeof(mbedtls_ctr_drbg_context));
    ctx->entropy = calloc(1, sizeof(mbedtls_entropy_context));
    ctx->cacert = calloc(1, sizeof(mbedtls_x509_crt));
    
    if (!ctx->ssl || !ctx->conf || !ctx->ctr_drbg || !ctx->entropy || !ctx->cacert) {
        goto fail;
    }
    
    /* Initialize RNG */
    mbedtls_entropy_init(ctx->entropy);
    mbedtls_ctr_drbg_init(ctx->ctr_drbg);
    mbedtls_x509_crt_init(ctx->cacert);
    
    const char *pers = "mikroclaw_tls_client";
    if (mbedtls_ctr_drbg_seed(ctx->ctr_drbg, mbedtls_entropy_func, ctx->entropy,
                              (const unsigned char *)pers, strlen(pers)) != 0) {
        goto fail;
    }
    
    /* Initialize SSL config */
    mbedtls_ssl_config_init(ctx->conf);
    if (mbedtls_ssl_config_defaults(ctx->conf,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        goto fail;
    }

    if (mbedtls_x509_crt_parse_file(ctx->cacert, "/etc/ssl/certs/ca-certificates.crt") != 0 &&
        mbedtls_x509_crt_parse_file(ctx->cacert, "/etc/ssl/cert.pem") != 0) {
        goto fail;
    }
    
    mbedtls_ssl_conf_rng(ctx->conf, mbedtls_ctr_drbg_random, ctx->ctr_drbg);
    mbedtls_ssl_conf_authmode(ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(ctx->conf, ctx->cacert, NULL);
    
    /* Initialize SSL context */
    mbedtls_ssl_init(ctx->ssl);
    if (mbedtls_ssl_setup(ctx->ssl, ctx->conf) != 0) {
        goto fail;
    }

    if (mbedtls_ssl_set_hostname(ctx->ssl, hostname) != 0) {
        goto fail;
    }
    
    ctx->initialized = 1;
    return 0;

fail:
    mbedtls_tls_free(ctx);
    return -1;
}

int mbedtls_connect_socket(struct mbedtls_ctx *ctx, int socket_fd) {
    if (!ctx->initialized) return -1;
    
    ctx->socket_fd = socket_fd;
    
    /* Set up BIO callbacks */
    mbedtls_ssl_set_bio(ctx->ssl, &ctx->socket_fd,
                        mbedtls_net_send, mbedtls_net_recv, NULL);
    
    return 0;
}

int mbedtls_handshake(struct mbedtls_ctx *ctx) {
    if (!ctx->initialized) return -1;
    if (ctx->socket_fd < 0) return -1;
    
    int ret;
    while ((ret = mbedtls_ssl_handshake(ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            return -1;
        }
    }
    return 0;
}

int mbedtls_send(struct mbedtls_ctx *ctx, const void *buf, size_t len) {
    if (!ctx->initialized) return -1;
    return mbedtls_ssl_write(ctx->ssl, buf, len);
}

int mbedtls_recv(struct mbedtls_ctx *ctx, void *buf, size_t len) {
    if (!ctx->initialized) return -1;
    return mbedtls_ssl_read(ctx->ssl, buf, len);
}

void mbedtls_tls_close(struct mbedtls_ctx *ctx) {
    if (!ctx->initialized) return;
    mbedtls_ssl_close_notify(ctx->ssl);
}

void mbedtls_tls_free(struct mbedtls_ctx *ctx) {
    if (!ctx->initialized) return;
    
    mbedtls_ssl_free(ctx->ssl);
    mbedtls_ssl_config_free(ctx->conf);
    mbedtls_ctr_drbg_free(ctx->ctr_drbg);
    mbedtls_entropy_free(ctx->entropy);
    mbedtls_x509_crt_free(ctx->cacert);
    
    free(ctx->ssl);
    free(ctx->conf);
    free(ctx->ctr_drbg);
    free(ctx->entropy);
    free(ctx->cacert);
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->socket_fd = -1;
}
