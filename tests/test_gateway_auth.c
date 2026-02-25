#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/gateway_auth.h"

int main(void) {
    struct gateway_auth_ctx *auth;
    char token[128];
    const char *code;
    char bearer[128];
    const char *request =
        "POST /webhook HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Authorization: Bearer abc123\r\n\r\n"
        "{}";

    auth = gateway_auth_init(300);
    assert(auth != NULL);

    code = gateway_auth_pairing_code(auth);
    assert(code != NULL);
    assert(strlen(code) == 6);

    memset(token, 0, sizeof(token));
    assert(gateway_auth_exchange_pairing_code(auth, code, token, sizeof(token)) == 0);
    assert(token[0] != '\0');
    assert(gateway_auth_validate_token(auth, token) == 1);

    memset(token, 0, sizeof(token));
    assert(gateway_auth_exchange_pairing_code(auth, "000000", token, sizeof(token)) != 0);

    memset(bearer, 0, sizeof(bearer));
    assert(gateway_auth_extract_bearer(request, bearer, sizeof(bearer)) == 0);
    assert(strcmp(bearer, "abc123") == 0);

    gateway_auth_destroy(auth);
    printf("ALL PASS: gateway auth pairing\n");
    return 0;
}
