#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/crypto.h"

int main(void) {
    char enc[1024];
    char dec[256];

    assert(crypto_encrypt_env_value("MEMU_ENCRYPTION_KEY", "secret-value", enc, sizeof(enc)) == 0);
    assert(strncmp(enc, "ENCRYPTED:", 10) == 0);
    assert(crypto_decrypt_env_value("MEMU_ENCRYPTION_KEY", enc, dec, sizeof(dec)) == 0);
    assert(strcmp(dec, "secret-value") == 0);

    printf("ALL PASS: crypto\n");
    return 0;
}
