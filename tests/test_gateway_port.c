#include <assert.h>
#include <stdio.h>

#include "../src/gateway.h"

int main(void) {
    struct gateway_config cfg = {0};
    struct gateway_ctx *gw;

    cfg.port = 0;
    cfg.bind_addr = "127.0.0.1";
    gw = gateway_init(&cfg);
    assert(gw != NULL);
    assert(gateway_port(gw) > 0);
    gateway_destroy(gw);

    printf("ALL PASS: gateway port\n");
    return 0;
}
