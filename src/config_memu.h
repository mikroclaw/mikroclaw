#ifndef MIKROCLAW_CONFIG_MEMU_H
#define MIKROCLAW_CONFIG_MEMU_H

struct memu_boot_config {
    char telegram_bot_token[256];
    char llm_api_key[256];
    char routeros_host[256];
    char routeros_user[128];
    char routeros_pass[128];
    char model[128];
    char discord_webhook_url[512];
    char slack_webhook_url[512];
};

int config_memu_load(const char *device_id, struct memu_boot_config *cfg);

#endif
