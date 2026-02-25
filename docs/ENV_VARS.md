# Environment Variables

Last updated: 2026-02-25

## Required (typical runtime)

- `BOT_TOKEN`
- `OPENROUTER_KEY` (or provider-specific key)
- `ROUTER_HOST`
- `ROUTER_USER`
- `ROUTER_PASS`

## LLM / Provider

- `LLM_PROVIDER` (named provider key)
- `LLM_BASE_URL` (fallback custom endpoint)
- `LLM_API_KEY` (fallback key)
- `MODEL`
- `LLM_STREAMING` (`1` to parse streamed SSE payloads)
- `RELIABLE_PROVIDERS` (comma-separated provider fallback chain)

Provider-specific keys used by `src/provider_registry.c`:

- `OPENROUTER_KEY`
- `OPENAI_API_KEY`
- `ANTHROPIC_API_KEY`
- `OLLAMA_API_KEY`
- `GROQ_API_KEY`
- `MISTRAL_API_KEY`
- `XAI_API_KEY`
- `DEEPSEEK_API_KEY`
- `TOGETHER_API_KEY`
- `FIREWORKS_API_KEY`
- `PERPLEXITY_API_KEY`
- `COHERE_API_KEY`
- `BEDROCK_API_KEY`

## memU

- `MEMU_API_KEY`
- `MEMU_BASE_URL` (default `https://api.memu.so`)
- `MEMU_DEVICE_ID` (default `mikroclaw-default`)
- `MEMU_BOOT_CONFIG_JSON` (optional boot override)
- `MEMU_MOCK_RETRIEVE_TEXT` (test mock)
- `MEMU_MOCK_CATEGORIES` (test mock)

## Gateway

- `GATEWAY_PORT`
- `GATEWAY_BIND`
- `PAIRING_REQUIRED` (`1` to enforce bearer auth)

## RouterOS Network/Scheduler

- `ROUTEROS_FIREWALL` (`1` enable auto firewall rule add/remove)
- `ROUTEROS_ALLOW_SUBNETS` (CSV list)
- `HEARTBEAT_ROUTEROS` (`1` enable scheduler add/remove)
- `HEARTBEAT_INTERVAL` (scheduler interval, e.g. `5m`)

## Channels

- `DISCORD_WEBHOOK_URL`
- `SLACK_WEBHOOK_URL`
- `TELEGRAM_ALLOWLIST`
- `DISCORD_ALLOWLIST`
- `SLACK_ALLOWLIST`

## Tools / Security

- `ALLOWED_SHELL_CMDS`
- `FORBIDDEN_PATHS`
- `SKILLS_MODE` (`routeros` or local)
- `WEBSCRAPE_SERVICES` (CSV: `jina,zai,firecrawl,scrapingbee`)
- `WEBSCRAPE_MOCK_RESPONSE` (test mock override for `web_scrape`)
- `SCRAPINGBEE_API_KEY`
- `COMPOSIO_URL`
- `COMPOSIO_API_KEY`

## Logging

- `LOG_LEVEL` (`error`, `warn`, `info`, `debug`; default `info`)
- `LOG_FORMAT` (`text` or `json`)

## Identity / Secrets

- `AGENT_ID`
- `MEMU_ENCRYPTION_KEY`
