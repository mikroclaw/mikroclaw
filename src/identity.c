#include "identity.h"

#include "memu_client.h"

#include <errno.h>
#include <fcntl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int random_fill_bytes(unsigned char *out, size_t len) {
    int rc;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mikroclaw-identity";

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

static void random_id(char *out, size_t out_len) {
    if (out_len < 37) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return;
    }

    unsigned char bytes[16];
    if (random_fill_bytes(bytes, sizeof(bytes)) != 0) {
        return;
    }

    snprintf(out, out_len,
             "%02x%02x%02x%02x%02x%02x%02x%02x-"
             "%02x%02x-"
             "%02x%02x-"
             "%02x%02x-"
             "%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11],
             bytes[12], bytes[13],
             bytes[14], bytes[15], bytes[0], bytes[1], bytes[2], bytes[3]);
}

int identity_get(char *out, size_t out_len) {
    const char *env;

    if (!out || out_len == 0) {
        return -1;
    }

    env = getenv("AGENT_ID");
    if (env && env[0] != '\0') {
        snprintf(out, out_len, "%s", env);
        return 0;
    }

    if (memu_retrieve("agent_id", "keyword", out, out_len) == 0 && out[0] != '\0') {
        return 0;
    }

    random_id(out, out_len);
    (void)memu_memorize(out, "identity", "agent");
    return 0;
}

int identity_rotate(char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return -1;
    }
    random_id(out, out_len);
    (void)memu_memorize(out, "identity", "agent");
    return 0;
}
