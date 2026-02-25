#include "provider_registry.h"

#include <stdio.h>
#include <string.h>

struct provider_entry {
    const char *name;
    const char *base_url;
    enum provider_auth_style auth_style;
    const char *api_key_env_var;
};

static const struct provider_entry g_providers[] = {
    {"openrouter", "https://openrouter.ai/api/v1", PROVIDER_AUTH_BEARER, "OPENROUTER_KEY"},
    {"openai", "https://api.openai.com/v1", PROVIDER_AUTH_BEARER, "OPENAI_API_KEY"},
    {"anthropic", "https://api.anthropic.com/v1", PROVIDER_AUTH_API_KEY, "ANTHROPIC_API_KEY"},
    {"ollama", "http://127.0.0.1:11434/v1", PROVIDER_AUTH_BEARER, "OLLAMA_API_KEY"},
    {"groq", "https://api.groq.com/openai", PROVIDER_AUTH_BEARER, "GROQ_API_KEY"},
    {"mistral", "https://api.mistral.ai", PROVIDER_AUTH_BEARER, "MISTRAL_API_KEY"},
    {"xai", "https://api.x.ai/v1", PROVIDER_AUTH_BEARER, "XAI_API_KEY"},
    {"deepseek", "https://api.deepseek.com", PROVIDER_AUTH_BEARER, "DEEPSEEK_API_KEY"},
    {"together", "https://api.together.xyz", PROVIDER_AUTH_BEARER, "TOGETHER_API_KEY"},
    {"fireworks", "https://api.fireworks.ai/inference", PROVIDER_AUTH_BEARER, "FIREWORKS_API_KEY"},
    {"perplexity", "https://api.perplexity.ai", PROVIDER_AUTH_BEARER, "PERPLEXITY_API_KEY"},
    {"cohere", "https://api.cohere.com/compatibility", PROVIDER_AUTH_API_KEY, "COHERE_API_KEY"},
    {"bedrock", "https://bedrock-runtime.us-east-1.amazonaws.com", PROVIDER_AUTH_API_KEY, "BEDROCK_API_KEY"}
};

int provider_registry_get(const char *name, struct provider_config *out) {
    size_t i;

    if (!name || !out) {
        return -1;
    }

    for (i = 0; i < (sizeof(g_providers) / sizeof(g_providers[0])); i++) {
        if (strcmp(g_providers[i].name, name) == 0) {
            snprintf(out->name, sizeof(out->name), "%s", g_providers[i].name);
            snprintf(out->base_url, sizeof(out->base_url), "%s", g_providers[i].base_url);
            out->auth_style = g_providers[i].auth_style;
            snprintf(out->api_key_env_var, sizeof(out->api_key_env_var), "%s",
                     g_providers[i].api_key_env_var);
            return 0;
        }
    }

    return -1;
}
