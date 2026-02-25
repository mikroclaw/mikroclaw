#ifndef MIKROCLAW_GATEWAY_AUTH_H
#define MIKROCLAW_GATEWAY_AUTH_H

#include <stddef.h>

struct gateway_auth_ctx;

struct gateway_auth_ctx *gateway_auth_init(int token_ttl_seconds);
void gateway_auth_destroy(struct gateway_auth_ctx *ctx);
const char *gateway_auth_pairing_code(struct gateway_auth_ctx *ctx);
int gateway_auth_exchange_pairing_code(struct gateway_auth_ctx *ctx,
                                       const char *code,
                                       char *token_out,
                                       size_t token_out_len);
int gateway_auth_validate_token(struct gateway_auth_ctx *ctx, const char *token);
int gateway_auth_extract_header(const char *request,
                                const char *name,
                                char *out,
                                size_t out_len);
int gateway_auth_extract_bearer(const char *request, char *token_out, size_t token_out_len);

#endif
