#include "gateway_auth.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#define MAX_TOKENS 16

struct token_entry {
    char token[96];
    time_t expires_at;
    int in_use;
};

struct gateway_auth_ctx {
    char pairing_code[7];
    int token_ttl_seconds;
    struct token_entry entries[MAX_TOKENS];
};

static int random_fill_bytes(unsigned char *out, size_t len) {
    int rc;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mikroclaw-gateway-auth";

    if (!out || len == 0) {
        return -1;
    }

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    rc = mbedtls_ctr_drbg_seed(&ctr_drbg,
                               mbedtls_entropy_func,
                               &entropy,
                               (const unsigned char *)pers,
                               strlen(pers));
    if (rc == 0) {
        rc = mbedtls_ctr_drbg_random(&ctr_drbg, out, len);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        if (rc == 0) {
            return 0;
        }
    }

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    {
        int fd = open("/dev/urandom", O_RDONLY);
        size_t offset = 0;

        if (fd < 0) {
            return -1;
        }

        while (offset < len) {
            ssize_t got = read(fd, out + offset, len - offset);
            if (got < 0) {
                if (errno == EINTR) {
                    continue;
                }
                close(fd);
                return -1;
            }
            if (got == 0) {
                close(fd);
                return -1;
            }
            offset += (size_t)got;
        }

        close(fd);
    }

    return 0;
}

static int random_string(char *out, size_t len) {
    static const char *alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t alpha_len = strlen(alphabet);
    size_t i;
    unsigned char r[1];

    if (!out || len == 0) {
        return -1;
    }

    for (i = 0; i + 1 < len; i++) {
        if (random_fill_bytes(r, sizeof(r)) != 0) {
            return -1;
        }
        out[i] = alphabet[r[0] % alpha_len];
    }
    out[len - 1] = '\0';
    return 0;
}

struct gateway_auth_ctx *gateway_auth_init(int token_ttl_seconds) {
    struct gateway_auth_ctx *ctx = calloc(1, sizeof(*ctx));
    unsigned char bytes[6];
    size_t i;

    if (!ctx) {
        return NULL;
    }

    if (random_fill_bytes(bytes, sizeof(bytes)) != 0) {
        gateway_auth_destroy(ctx);
        return NULL;
    }

    for (i = 0; i < 6; i++) {
        ctx->pairing_code[i] = (char)('0' + (bytes[i] % 10));
    }
    ctx->pairing_code[6] = '\0';
    ctx->token_ttl_seconds = token_ttl_seconds > 0 ? token_ttl_seconds : 300;
    return ctx;
}

void gateway_auth_destroy(struct gateway_auth_ctx *ctx) {
    if (!ctx) {
        return;
    }
    free(ctx);
}

const char *gateway_auth_pairing_code(struct gateway_auth_ctx *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->pairing_code;
}

int gateway_auth_exchange_pairing_code(struct gateway_auth_ctx *ctx,
                                       const char *code,
                                       char *token_out,
                                       size_t token_out_len) {
    int i;
    time_t now;

    if (!ctx || !code || !token_out || token_out_len == 0) {
        return -1;
    }
    if (strcmp(ctx->pairing_code, code) != 0) {
        return -1;
    }

    now = time(NULL);
    for (i = 0; i < MAX_TOKENS; i++) {
        if (!ctx->entries[i].in_use || ctx->entries[i].expires_at <= now) {
            ctx->entries[i].in_use = 1;
            if (random_string(ctx->entries[i].token, sizeof(ctx->entries[i].token)) != 0) {
                return -1;
            }
            ctx->entries[i].expires_at = now + ctx->token_ttl_seconds;
            snprintf(token_out, token_out_len, "%s", ctx->entries[i].token);
            return 0;
        }
    }
    return -1;
}

int gateway_auth_validate_token(struct gateway_auth_ctx *ctx, const char *token) {
    int i;
    time_t now;

    if (!ctx || !token || token[0] == '\0') {
        return 0;
    }
    now = time(NULL);
    for (i = 0; i < MAX_TOKENS; i++) {
        if (!ctx->entries[i].in_use) {
            continue;
        }
        if (ctx->entries[i].expires_at <= now) {
            ctx->entries[i].in_use = 0;
            continue;
        }
        if (strcmp(ctx->entries[i].token, token) == 0) {
            return 1;
        }
    }
    return 0;
}

int gateway_auth_extract_header(const char *request,
                                const char *name,
                                char *out,
                                size_t out_len) {
    char pat[128];
    const char *p;
    const char *line_end;
    size_t n;

    if (!request || !name || !out || out_len == 0) {
        return -1;
    }

    snprintf(pat, sizeof(pat), "\n%s:", name);
    p = strstr(request, pat);
    if (!p && strncmp(request, name, strlen(name)) == 0 && request[strlen(name)] == ':') {
        p = request - 1;
    }
    if (!p) {
        return -1;
    }

    p += strlen(pat);
    while (*p == ' ') {
        p++;
    }
    line_end = strstr(p, "\r\n");
    if (!line_end) {
        line_end = strchr(p, '\n');
    }
    if (!line_end) {
        line_end = p + strlen(p);
    }

    n = (size_t)(line_end - p);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

int gateway_auth_extract_bearer(const char *request, char *token_out, size_t token_out_len) {
    char auth[256];
    const char *prefix = "Bearer ";

    if (gateway_auth_extract_header(request, "Authorization", auth, sizeof(auth)) != 0) {
        return -1;
    }
    if (strncmp(auth, prefix, strlen(prefix)) != 0) {
        return -1;
    }

    snprintf(token_out, token_out_len, "%s", auth + strlen(prefix));
    return 0;
}
