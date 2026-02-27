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

    assert(provider_registry_get("kimi", &cfg) == 0);
    assert(strcmp(cfg.name, "kimi") == 0);
    assert(strcmp(cfg.base_url, "https://api.moonshot.cn/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_BEARER);
    assert(strcmp(cfg.api_key_env_var, "MOONSHOT_API_KEY") == 0);

    assert(provider_registry_get("minimax", &cfg) == 0);
    assert(strcmp(cfg.name, "minimax") == 0);
    assert(strcmp(cfg.base_url, "https://api.minimax.chat/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_API_KEY);
    assert(strcmp(cfg.api_key_env_var, "MINIMAX_API_KEY") == 0);

    assert(provider_registry_get("zai", &cfg) == 0);
    assert(strcmp(cfg.name, "zai") == 0);
    assert(strcmp(cfg.base_url, "https://api.z.ai/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_BEARER);
    assert(strcmp(cfg.api_key_env_var, "ZAI_API_KEY") == 0);

    assert(provider_registry_get("synthetic", &cfg) == 0);
    assert(strcmp(cfg.name, "synthetic") == 0);
    assert(strcmp(cfg.base_url, "https://api.synthetic.new/v1") == 0);
    assert(cfg.auth_style == PROVIDER_AUTH_BEARER);
    assert(strcmp(cfg.api_key_env_var, "SYNTHETIC_API_KEY") == 0);
    assert(provider_registry_get("does-not-exist", &cfg) != 0);

    printf("ALL PASS: provider registry\n");
    return 0;
}
