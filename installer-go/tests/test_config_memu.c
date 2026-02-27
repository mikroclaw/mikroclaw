#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/config_memu.h"

int main(void) {
    struct memu_boot_config cfg;

    setenv("MEMU_BOOT_CONFIG_JSON",
           "{\"telegram_bot_token\":\"tg-token\",\"llm_api_key\":\"llm-key\",\"routeros_host\":\"10.0.0.1\",\"routeros_user\":\"admin\",\"routeros_pass\":\"secret\",\"model\":\"gpt-4o-mini\"}",
           1);

    memset(&cfg, 0, sizeof(cfg));
    assert(config_memu_load("device-1", &cfg) == 0);
    assert(strcmp(cfg.telegram_bot_token, "tg-token") == 0);
    assert(strcmp(cfg.llm_api_key, "llm-key") == 0);
    assert(strcmp(cfg.routeros_host, "10.0.0.1") == 0);
    assert(strcmp(cfg.routeros_user, "admin") == 0);
    assert(strcmp(cfg.routeros_pass, "secret") == 0);
    assert(strcmp(cfg.model, "gpt-4o-mini") == 0);

    unsetenv("MEMU_BOOT_CONFIG_JSON");
    printf("ALL PASS: config_memu\n");
    return 0;
}
