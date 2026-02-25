#include "crypto.h"

#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int random_fill_bytes(unsigned char *out, size_t len) {
    int rc;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mikroclaw-crypto";

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
    }

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    if (rc == 0) {
        return 0;
    }

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

static int derive_key(const char *key_text, unsigned char key[32]) {
    if (!key_text || key_text[0] == '\0') {
        return -1;
    }
    mbedtls_sha256((const unsigned char *)key_text, strlen(key_text), key, 0);
    return 0;
}

static int nonce_fill(unsigned char nonce[12]) {
    if (!nonce) {
        return -1;
    }

    return random_fill_bytes(nonce, 12);
}

int crypto_encrypt_env_value(const char *key_env, const char *plaintext, char *out, size_t out_len) {
    const char *key_text;
    unsigned char key[32];
    unsigned char nonce[12];
    unsigned char tag[16];
    unsigned char cipher[512];
    unsigned char b64_nonce[64];
    unsigned char b64_cipher[1024];
    unsigned char b64_tag[64];
    size_t nonce_len = 0;
    size_t cipher_len = 0;
    size_t tag_len = 0;
    mbedtls_gcm_context gcm;

    if (!key_env || !plaintext || !out || out_len == 0) {
        return -1;
    }
    if (strlen(plaintext) > sizeof(cipher)) {
        return -1;
    }

    key_text = getenv(key_env);
    if (derive_key(key_text, key) != 0) {
        return -1;
    }

    if (nonce_fill(nonce) != 0) {
        return -1;
    }

    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }
    if (mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                  strlen(plaintext),
                                  nonce, sizeof(nonce),
                                  NULL, 0,
                                  (const unsigned char *)plaintext,
                                  cipher,
                                  sizeof(tag), tag) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }
    mbedtls_gcm_free(&gcm);

    if (mbedtls_base64_encode(b64_nonce, sizeof(b64_nonce), &nonce_len,
                              nonce, sizeof(nonce)) != 0) {
        return -1;
    }
    if (mbedtls_base64_encode(b64_cipher, sizeof(b64_cipher), &cipher_len,
                              cipher, strlen(plaintext)) != 0) {
        return -1;
    }
    if (mbedtls_base64_encode(b64_tag, sizeof(b64_tag), &tag_len,
                              tag, sizeof(tag)) != 0) {
        return -1;
    }

    if (snprintf(out, out_len, "ENCRYPTED:v1:%s:%s:%s", b64_nonce, b64_cipher, b64_tag) >= (int)out_len) {
        return -1;
    }

    return 0;
}

int crypto_decrypt_env_value(const char *key_env, const char *input, char *out, size_t out_len) {
    const char *key_text;
    unsigned char key[32];
    unsigned char nonce[32];
    unsigned char cipher[512];
    unsigned char tag[32];
    unsigned char plain[512];
    size_t nonce_len = 0;
    size_t cipher_len = 0;
    size_t tag_len = 0;
    mbedtls_gcm_context gcm;
    char copy[2048];
    char *p;
    char *prefix;
    char *version;
    char *n;
    char *c;
    char *t;

    if (!key_env || !input || !out || out_len == 0) {
        return -1;
    }
    if (strncmp(input, "ENCRYPTED:v1:", 13) != 0) {
        snprintf(out, out_len, "%s", input);
        return 0;
    }

    key_text = getenv(key_env);
    if (derive_key(key_text, key) != 0) {
        return -1;
    }

    snprintf(copy, sizeof(copy), "%s", input);
    p = copy;
    prefix = strtok(p, ":");
    version = strtok(NULL, ":");
    n = strtok(NULL, ":");
    c = strtok(NULL, ":");
    t = strtok(NULL, ":");
    if (!prefix || !version || !n || !c || !t || strcmp(prefix, "ENCRYPTED") != 0 || strcmp(version, "v1") != 0) {
        return -1;
    }

    if (mbedtls_base64_decode(nonce, sizeof(nonce), &nonce_len,
                              (const unsigned char *)n, strlen(n)) != 0) {
        return -1;
    }
    if (mbedtls_base64_decode(cipher, sizeof(cipher), &cipher_len,
                              (const unsigned char *)c, strlen(c)) != 0) {
        return -1;
    }
    if (mbedtls_base64_decode(tag, sizeof(tag), &tag_len,
                              (const unsigned char *)t, strlen(t)) != 0) {
        return -1;
    }

    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }
    if (mbedtls_gcm_auth_decrypt(&gcm,
                                 cipher_len,
                                 nonce, nonce_len,
                                 NULL, 0,
                                 tag, tag_len,
                                 cipher,
                                 plain) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }
    mbedtls_gcm_free(&gcm);

    if (cipher_len >= out_len) {
        return -1;
    }
    memcpy(out, plain, cipher_len);
    out[cipher_len] = '\0';
    return 0;
}
