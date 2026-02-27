/* MikroClaw TLS Test */
#include <stdio.h>
#include "../src/http.h"
int main() {
    printf("Testing HTTPS to api.telegram.org...\n");
    struct http_client *c = http_client_create("api.telegram.org", 443, 1);
    if (!c) { printf("FAIL: create\n"); return 1; }
    struct http_response r;
    int ret = http_get(c, "/bot123:test/getMe", NULL, 0, &r);
    if (ret != 0) { printf("FAIL: get returned %d\n", ret); return 1; }
    printf("Status: %d\n", r.status_code);
    if (r.status_code == 401 || r.status_code == 404) { printf("PASS: TLS works (%d expected for bad token)\n", r.status_code); }
    else { printf("FAIL: unexpected status\n"); return 1; }
    http_client_destroy(c);
    return 0;
}
