#ifndef MIKROCLAW_CRYPTO_H
#define MIKROCLAW_CRYPTO_H

#include <stddef.h>

int crypto_encrypt_env_value(const char *key_env, const char *plaintext, char *out, size_t out_len);
int crypto_decrypt_env_value(const char *key_env, const char *input, char *out, size_t out_len);

#endif
