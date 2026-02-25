#include <stdio.h>
#include <string.h>

#include <mbedtls/ssl.h>

#include "../vendor/mbedtls_integration.h"

static int test_verify_required(void) {
    struct mbedtls_ctx ctx;
    int ret = mbedtls_init(&ctx, "api.telegram.org");
    if (ret != 0) {
        printf("FAIL: mbedtls_init returned %d\n", ret);
        return 1;
    }

    mbedtls_ssl_config *conf = (mbedtls_ssl_config *) ctx.conf;
    if (conf->MBEDTLS_PRIVATE(authmode) != MBEDTLS_SSL_VERIFY_REQUIRED) {
        printf("FAIL: authmode=%d expected=%d\n",
               conf->MBEDTLS_PRIVATE(authmode), MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_tls_free(&ctx);
        return 1;
    }

    mbedtls_tls_free(&ctx);
    printf("PASS: authmode is VERIFY_REQUIRED\n");
    return 0;
}

static int test_hostname_set(void) {
    struct mbedtls_ctx ctx;
    int ret = mbedtls_init(&ctx, "api.telegram.org");
    if (ret != 0) {
        printf("FAIL: mbedtls_init returned %d\n", ret);
        return 1;
    }

    mbedtls_ssl_context *ssl = (mbedtls_ssl_context *) ctx.ssl;
    const char *hostname = mbedtls_ssl_get_hostname(ssl);
    if (hostname == NULL || strcmp(hostname, "api.telegram.org") != 0) {
        printf("FAIL: hostname not set correctly\n");
        mbedtls_tls_free(&ctx);
        return 1;
    }

    mbedtls_tls_free(&ctx);
    printf("PASS: hostname set\n");
    return 0;
}

int main(void) {
    int failures = 0;

    failures += test_verify_required();
    failures += test_hostname_set();

    if (failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }

    printf("FAILURES: %d\n", failures);
    return 1;
}
