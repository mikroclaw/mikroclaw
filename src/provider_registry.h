#ifndef MIKROCLAW_PROVIDER_REGISTRY_H
#define MIKROCLAW_PROVIDER_REGISTRY_H

enum provider_auth_style {
    PROVIDER_AUTH_BEARER = 0,
    PROVIDER_AUTH_API_KEY = 1
};

struct provider_config {
    char name[32];
    char base_url[128];
    enum provider_auth_style auth_style;
    char api_key_env_var[64];
};

int provider_registry_get(const char *name, struct provider_config *out);

#endif
