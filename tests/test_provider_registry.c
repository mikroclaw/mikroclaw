#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/provider_registry.h"

int main(void) {
    struct provider_config cfg;

    assert(provider_registry_get("openrouter", &cfg) == 0);
    assert(strcmp(cfg.name, "openrouter") == 0);
    assert(strcmp(cfg.base_url, "https://openrouter.ai/api/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_BEARER);
    assert(strcmp(cfg.api_key_env_var, "OPENROUTER_KEY") == 0);

    assert(provider_registry_get("openai", &cfg) == 0);
    assert(strcmp(cfg.base_url, "https://api.openai.com/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_BEARER);

    assert(provider_registry_get("cohere", &cfg) == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_API_KEY);

    assert(provider_registry_get("does-not-exist", &cfg) != 0);

    printf("ALL PASS: provider registry\n");
    return 0;
}
