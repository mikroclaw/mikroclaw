/*
 * MikroClaw - Main entry point
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "mikroclaw.h"
#include "mikroclaw_config.h"
#include "routeros.h"
#include "llm.h"
#include "provider_registry.h"
#include "config_validate.h"
#include "crypto.h"
#include "identity.h"
#include "log.h"
#include "channels/telegram.h"
#include "gateway.h"
#include "functions.h"
#ifdef USE_MEMU_CLOUD
#include "memu_client.h"
#include "config_memu.h"
#endif
#include "cli.h"

static volatile int running = 1;

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static const char *getenv_or(const char *key, const char *default_val) {
    const char *val = getenv(key);
    return val ? val : default_val;
}

static void print_usage(const char *prog) {
    printf("MikroClaw %s - AI agent for MikroTik RouterOS\n", MIKROCLAW_VERSION);
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --version      Show version and exit\n");
    printf("  --test         Run self-test and exit\n");
    printf("  --help         Show this help\n\n");
    printf("Commands:\n");
    printf("  agent          Run full agent loop (default)\n");
    printf("  gateway        Run gateway-focused loop\n");
    printf("  daemon         Run daemon mode\n");
    printf("  status         Print status and exit\n");
    printf("  doctor         Run diagnostics and exit\n");
    printf("  channel        Print channel status and exit\n\n");
    printf("  config --dump  Print validated config (secrets redacted)\n");
    printf("  integrations [list|info <name>]\n");
    printf("  identity [--rotate]\n");
    printf("  encrypt KEY=VALUE  Encrypt secret using MEMU_ENCRYPTION_KEY\n\n");
    printf("Environment variables:\n");
    printf("  Required:\n");
    printf("    BOT_TOKEN         Telegram bot token\n");
    printf("    OPENROUTER_KEY    OpenRouter API key\n");
    printf("    ROUTER_HOST       RouterOS REST API host (e.g., 172.17.0.1)\n");
    printf("    ROUTER_USER       RouterOS API username\n");
    printf("    ROUTER_PASS       RouterOS API password\n");
    printf("  Optional:\n");
    printf("    CHAT_ID           Default Telegram chat ID\n");
    printf("    DISCORD_WEBHOOK_URL Discord webhook URL\n");
    printf("    SLACK_WEBHOOK_URL Slack webhook URL\n");
    printf("    GATEWAY_PORT      Gateway port (default: 18789)\n");
    printf("    MODEL             LLM model (default: google/gemini-flash)\n");
}

int main(int argc, char **argv) {
    enum cli_mode cli_mode = CLI_MODE_AGENT;
    int gateway_port_override = -1;
    int identity_rotate_flag = 0;
    int firewall_added = 0;
    int scheduler_added = 0;

    log_set_level_from_env();
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("%s\n", MIKROCLAW_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--test") == 0) {
            printf("Self-test not yet implemented\n");
            return 0;
        }
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            gateway_port_override = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--rotate") == 0) {
            identity_rotate_flag = 1;
            continue;
        }
    }

    if (argc >= 3 && strcmp(argv[1], "encrypt") == 0) {
        const char *kv = argv[2];
        const char *eq = strchr(kv, '=');
        char encrypted[2048];
        if (!eq || eq == kv || eq[1] == '\0') {
            fprintf(stderr, "Usage: %s encrypt KEY=VALUE\n", argv[0]);
            return 1;
        }
        if (crypto_encrypt_env_value("MEMU_ENCRYPTION_KEY", eq + 1, encrypted, sizeof(encrypted)) != 0) {
            fprintf(stderr, "Error: encryption failed (set MEMU_ENCRYPTION_KEY)\n");
            return 1;
        }
        printf("%.*s=%s\n", (int)(eq - kv), kv, encrypted);
        return 0;
    }

    cli_parse_mode(argc, argv, &cli_mode);
    
    /* Setup signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    int memu_loaded = 0;
    const char *memu_bot_token = NULL;
    const char *memu_llm_key = NULL;
    const char *memu_router_host = NULL;
    const char *memu_router_user = NULL;
    const char *memu_router_pass = NULL;
    const char *memu_model = NULL;
#ifdef USE_MEMU_CLOUD
    struct memu_boot_config memu_cfg;
    const char *memu_api_key = getenv("MEMU_API_KEY");
    const char *memu_base_url = getenv_or("MEMU_BASE_URL", "https://api.memu.so");
    const char *memu_device_id = getenv_or("MEMU_DEVICE_ID", "mikroclaw-default");

    if (memu_api_key && memu_api_key[0] != '\0' &&
        memu_client_configure(memu_api_key, memu_base_url) == 0 &&
        config_memu_load(memu_device_id, &memu_cfg) == 0) {
        memu_loaded = 1;
        memu_bot_token = memu_cfg.telegram_bot_token[0] ? memu_cfg.telegram_bot_token : NULL;
        memu_llm_key = memu_cfg.llm_api_key[0] ? memu_cfg.llm_api_key : NULL;
        memu_router_host = memu_cfg.routeros_host[0] ? memu_cfg.routeros_host : NULL;
        memu_router_user = memu_cfg.routeros_user[0] ? memu_cfg.routeros_user : NULL;
        memu_router_pass = memu_cfg.routeros_pass[0] ? memu_cfg.routeros_pass : NULL;
        memu_model = memu_cfg.model[0] ? memu_cfg.model : NULL;
        if (memu_cfg.discord_webhook_url[0] != '\0') {
            setenv("DISCORD_WEBHOOK_URL", memu_cfg.discord_webhook_url, 1);
        }
        if (memu_cfg.slack_webhook_url[0] != '\0') {
            setenv("SLACK_WEBHOOK_URL", memu_cfg.slack_webhook_url, 1);
        }
        printf("Loaded device config from memU for %s\n", memu_device_id);
    }
#endif

    char openrouter_key_plain[512];
    char llm_key_plain[512];
    const char *bot_token = (memu_loaded && memu_bot_token) ?
        memu_bot_token : getenv("BOT_TOKEN");
    const char *openrouter_key = (memu_loaded && memu_llm_key) ?
        memu_llm_key : getenv("OPENROUTER_KEY");
    const char *router_host = (memu_loaded && memu_router_host) ?
        memu_router_host : getenv("ROUTER_HOST");
    const char *router_user = (memu_loaded && memu_router_user) ?
        memu_router_user : getenv("ROUTER_USER");
    const char *router_pass = (memu_loaded && memu_router_pass) ?
        memu_router_pass : getenv("ROUTER_PASS");
    const char *model = (memu_loaded && memu_model) ?
        memu_model : getenv_or("MODEL", "google/gemini-flash");

    if (openrouter_key && strncmp(openrouter_key, "ENCRYPTED:", 10) == 0) {
        if (crypto_decrypt_env_value("MEMU_ENCRYPTION_KEY", openrouter_key,
                                     openrouter_key_plain, sizeof(openrouter_key_plain)) == 0) {
            openrouter_key = openrouter_key_plain;
        }
    }

    if (cli_mode == CLI_MODE_STATUS) {
        int in_docker = (access("/.dockerenv", F_OK) == 0) ? 1 : 0;
        printf("{\"status\":\"ok\",\"mode\":\"%s\",\"router\":\"%s\",\"model\":\"%s\",\"container\":%s}\n",
               cli_mode_name(cli_mode),
               router_host ? router_host : "unset",
               model ? model : "unset",
               in_docker ? "true" : "false");
        return 0;
    }

    if (cli_mode == CLI_MODE_DOCTOR) {
        printf("doctor: env=%s router=%s model=%s memu=%s\n",
               (bot_token && openrouter_key && router_host && router_user && router_pass) ? "ok" : "missing",
               router_host ? router_host : "unset",
               model ? model : "unset",
               getenv("MEMU_API_KEY") ? "configured" : "not-configured");
        return 0;
    }

    if (cli_mode == CLI_MODE_CHANNEL) {
        printf("channel: telegram=%s discord=%s slack=%s\n",
               bot_token ? "configured" : "missing",
               getenv("DISCORD_WEBHOOK_URL") ? "configured" : "missing",
               getenv("SLACK_WEBHOOK_URL") ? "configured" : "missing");
        return 0;
    }

    if (cli_mode == CLI_MODE_CONFIG) {
        char dump[4096];
        if (config_dump_redacted(dump, sizeof(dump)) != 0) {
            fprintf(stderr, "Error: unable to render config dump\n");
            return 1;
        }
        printf("%s", dump);
        return 0;
    }

    if (cli_mode == CLI_MODE_INTEGRATIONS) {
        if (argc >= 4 && strcmp(argv[2], "info") == 0) {
            if (strcmp(argv[3], "openrouter") == 0) {
                printf("openrouter: env=OPENROUTER_KEY url=https://openrouter.ai/api/v1\n");
            } else if (strcmp(argv[3], "memu") == 0) {
                printf("memu: env=MEMU_API_KEY,MEMU_BASE_URL url=https://api.memu.so\n");
            } else if (strcmp(argv[3], "zai") == 0) {
                printf("zai: env=WEBSCRAPE_SERVICES endpoint=api.z.ai/web/scrape\n");
            } else {
                printf("%s: unknown integration\n", argv[3]);
            }
        } else {
            printf("openrouter\nopenai\nanthropic\nollama\nmemu\ntelegram\ndiscord\nslack\nrouteros\nzai\njina\nfirecrawl\n");
        }
        return 0;
    }

    if (cli_mode == CLI_MODE_IDENTITY) {
        char id[128];
        int rc = identity_rotate_flag ? identity_rotate(id, sizeof(id)) : identity_get(id, sizeof(id));
        if (rc != 0) {
            fprintf(stderr, "identity operation failed\n");
            return 1;
        }
        printf("%s\n", id);
        return 0;
    }

    {
        char config_error[256];
        if (!memu_loaded && config_validate_required(config_error, sizeof(config_error)) != 0) {
            fprintf(stderr, "Error: %s\n", config_error);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!bot_token || !openrouter_key || !router_host || !router_user || !router_pass) {
        fprintf(stderr, "Error: Missing required runtime values (memU or env)\n");
        print_usage(argv[0]);
        return 1;
    }
    
    printf("MikroClaw %s starting...\n", MIKROCLAW_VERSION);
    log_emit(LOG_LEVEL_INFO, "main", "startup");
    printf("Mode: %s\n", cli_mode_name(cli_mode));
    printf("Router: %s@%s\n", router_user, router_host);
    printf("Model: %s\n", model);
    
    /* Initialize context */
    struct mikroclaw_ctx ctx = {0};
    ctx.openrouter_key = openrouter_key;
    ctx.model = model;
    channel_supervisor_init(&ctx.supervisor);

    functions_init();
    
    /* Initialize RouterOS connection */
    ctx.ros = routeros_init(router_host, 443, router_user, router_pass);
    if (!ctx.ros) {
        fprintf(stderr, "Failed to connect to RouterOS\n");
        functions_destroy();
        return 1;
    }
    
    /* Test RouterOS connection */
    char test_resp[1024];
    int ret = routeros_get(ctx.ros, "/system/resource", test_resp, sizeof(test_resp));
    if (ret != MC_OK) {
        fprintf(stderr, "RouterOS connection test failed: %d\n", ret);
        routeros_destroy(ctx.ros);
        return 1;
    }
    printf("RouterOS connected successfully\n");
    
    /* Initialize LLM client */
    struct llm_config llm_cfg;
    struct provider_config provider_cfg;
    const char *provider_name = getenv_or("LLM_PROVIDER", "openrouter");
    const char *llm_key = NULL;
    if (provider_registry_get(provider_name, &provider_cfg) == 0) {
        const char *provider_key = getenv(provider_cfg.api_key_env_var);
        strncpy(llm_cfg.base_url, provider_cfg.base_url, sizeof(llm_cfg.base_url) - 1);
        llm_cfg.auth_style = provider_cfg.auth_style;
        llm_key = provider_key ? provider_key : getenv("LLM_API_KEY");
    } else {
        const char *base_url = getenv("LLM_BASE_URL");
        strncpy(llm_cfg.base_url,
                (base_url && base_url[0] != '\0') ? base_url : "https://api.openai.com/v1",
                sizeof(llm_cfg.base_url) - 1);
        llm_cfg.auth_style = PROVIDER_AUTH_BEARER;
        llm_key = getenv("LLM_API_KEY");
    }
    if (!llm_key || llm_key[0] == '\0') {
        llm_key = openrouter_key;
    }
    if (llm_key && strncmp(llm_key, "ENCRYPTED:", 10) == 0) {
        if (crypto_decrypt_env_value("MEMU_ENCRYPTION_KEY", llm_key,
                                     llm_key_plain, sizeof(llm_key_plain)) == 0) {
            llm_key = llm_key_plain;
        }
    }
    llm_cfg.base_url[sizeof(llm_cfg.base_url) - 1] = '\0';
    strncpy(llm_cfg.model, model, sizeof(llm_cfg.model) - 1);
    llm_cfg.model[sizeof(llm_cfg.model) - 1] = '\0';
    llm_cfg.temperature = 0.7f;
    llm_cfg.max_tokens = 2048;
    llm_cfg.timeout_ms = 30000;
    strncpy(llm_cfg.api_key, llm_key ? llm_key : "", sizeof(llm_cfg.api_key) - 1);
    llm_cfg.api_key[sizeof(llm_cfg.api_key) - 1] = '\0';
    ctx.llm = llm_init(&llm_cfg);
    if (!ctx.llm) {
        fprintf(stderr, "Failed to initialize LLM client\n");
        routeros_destroy(ctx.ros);
        functions_destroy();
        return 1;
    }
    printf("LLM client ready\n");
    
    /* Initialize channels */
#ifdef CHANNEL_TELEGRAM
    struct telegram_config tg_config;
    strncpy(tg_config.bot_token, bot_token, sizeof(tg_config.bot_token) - 1);

    ctx.telegram = telegram_init(&tg_config);
    if (!ctx.telegram) {
        fprintf(stderr, "Failed to initialize Telegram channel\n");
        llm_destroy(ctx.llm);
        routeros_destroy(ctx.ros);
        functions_destroy();
        return 1;
    }
    printf("Telegram channel ready\n");
#endif

#ifdef CHANNEL_DISCORD
    {
        const char *discord_webhook = getenv("DISCORD_WEBHOOK_URL");
        if (discord_webhook && discord_webhook[0] != '\0') {
            struct discord_config dc_config;
            memset(&dc_config, 0, sizeof(dc_config));
            strncpy(dc_config.webhook_url, discord_webhook, sizeof(dc_config.webhook_url) - 1);
            ctx.discord = discord_init(&dc_config);
            if (!ctx.discord) {
                fprintf(stderr, "Failed to initialize Discord channel\n");
                llm_destroy(ctx.llm);
#ifdef CHANNEL_TELEGRAM
                telegram_destroy(ctx.telegram);
#endif
                routeros_destroy(ctx.ros);
                functions_destroy();
                return 1;
            }
            printf("Discord channel ready\n");
        }
    }
#endif

#ifdef CHANNEL_SLACK
    {
        const char *slack_webhook = getenv("SLACK_WEBHOOK_URL");
        if (slack_webhook && slack_webhook[0] != '\0') {
            struct slack_config sk_config;
            memset(&sk_config, 0, sizeof(sk_config));
            strncpy(sk_config.webhook_url, slack_webhook, sizeof(sk_config.webhook_url) - 1);
            ctx.slack = slack_init(&sk_config);
            if (!ctx.slack) {
                fprintf(stderr, "Failed to initialize Slack channel\n");
                llm_destroy(ctx.llm);
#ifdef CHANNEL_TELEGRAM
                telegram_destroy(ctx.telegram);
#endif
#ifdef CHANNEL_DISCORD
                discord_destroy(ctx.discord);
#endif
                routeros_destroy(ctx.ros);
                functions_destroy();
                return 1;
            }
            printf("Slack channel ready\n");
        }
    }
#endif
    
    /* Initialize gateway */
#ifdef ENABLE_GATEWAY
    const char *gateway_port_str = getenv_or("GATEWAY_PORT", "18789");
    const char *gateway_bind = getenv_or("GATEWAY_BIND", "0.0.0.0");
    int gateway_port_value = (gateway_port_override >= 0) ? gateway_port_override : atoi(gateway_port_str);
    struct gateway_config gw_config = {
        .port = (uint16_t)gateway_port_value,
        .bind_addr = gateway_bind
    };
    ctx.gateway = gateway_init(&gw_config);
    if (ctx.gateway) {
        int bound_port = gateway_port(ctx.gateway);
        printf("Gateway listening on %s:%d\n", gateway_bind, bound_port);
        ctx.gateway_auth = gateway_auth_init(300);
        ctx.rate_limit = rate_limit_init(10, 60, 60);
        ctx.subagent = subagent_init(4, 100);
        if (ctx.gateway_auth) {
            printf("Pairing code: %s\n", gateway_auth_pairing_code(ctx.gateway_auth));
        }

        if (getenv("ROUTEROS_FIREWALL") && strcmp(getenv("ROUTEROS_FIREWALL"), "1") == 0) {
            const char *subnets = getenv_or("ROUTEROS_ALLOW_SUBNETS", "10.0.0.0/8,172.16.0.0/12,192.168.0.0/16");
            if (ctx.ros && routeros_firewall_allow_subnets(ctx.ros, "mikroclaw-auto", subnets, bound_port) == 0) {
                firewall_added = 1;
            }
        }

        if (getenv("HEARTBEAT_ROUTEROS") && strcmp(getenv("HEARTBEAT_ROUTEROS"), "1") == 0) {
            char sched_resp[1024];
            char on_event[512];
            const char *interval = getenv_or("HEARTBEAT_INTERVAL", "5m");
            snprintf(on_event, sizeof(on_event),
                     "/tool fetch url=\"http://%s:%d/health/heartbeat\" keep-result=no",
                     gateway_bind, bound_port);
            if (ctx.ros && routeros_scheduler_add(ctx.ros, "mikroclaw-heartbeat", interval,
                                                  on_event, sched_resp, sizeof(sched_resp)) == 0) {
                scheduler_added = 1;
            }
        }
    }
#endif
    
    /* Main loop */
    printf("Entering main loop...\n");
    if (cli_mode == CLI_MODE_DAEMON) {
        while (running) {
            pid_t child = fork();
            int status = 0;
            if (child == 0) {
                while (running) {
                    mikroclaw_run(&ctx);
                    usleep(100000);
                }
                _exit(0);
            }
            if (child < 0) {
                break;
            }
            waitpid(child, &status, 0);
            if (!running) {
                break;
            }
            sleep(1);
        }
    } else {
        while (running) {
            mikroclaw_run(&ctx);
            usleep(100000);  /* 100ms poll interval */
        }
    }
    
    printf("Shutting down...\n");
    log_emit(LOG_LEVEL_INFO, "main", "shutdown");
    
    /* Cleanup */
        llm_destroy(ctx.llm);
#ifdef CHANNEL_TELEGRAM
        telegram_destroy(ctx.telegram);
#endif
#ifdef CHANNEL_DISCORD
        discord_destroy(ctx.discord);
#endif
#ifdef CHANNEL_SLACK
        slack_destroy(ctx.slack);
#endif
#ifdef ENABLE_GATEWAY
        if (ctx.ros && firewall_added) {
            char tmp[256];
            (void)routeros_firewall_remove_comment(ctx.ros, "mikroclaw-auto");
            (void)tmp;
        }
        if (ctx.ros && scheduler_added) {
            char sched_out[512];
            (void)routeros_scheduler_remove(ctx.ros, "mikroclaw-heartbeat", sched_out, sizeof(sched_out));
        }
        gateway_destroy(ctx.gateway);
        gateway_auth_destroy(ctx.gateway_auth);
        rate_limit_destroy(ctx.rate_limit);
        subagent_destroy(ctx.subagent);
#endif
    functions_destroy();
    routeros_destroy(ctx.ros);
    
    return 0;
}
