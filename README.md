# MikroClaw

Ultra-lightweight AI agent runtime for MikroTik RouterOS container deployments.

- Tiny C runtime designed for static builds
- memU cloud-first memory (no local conversational persistence)
- RouterOS-native automation via REST API
- Internal-network / VPN-first gateway model

## Current Release Focus

This repository is configured for a publish-ready `v0.3.0` track with:

- Provider registry (13 named providers)
- LLM streaming parser support
- Sender allowlists for Telegram/Discord/Slack
- Async subagent task API (`/tasks`)
- Gateway pairing + rate limiting + lockout
- Tool schema registry + security checks
- Encrypted secret values (`ENCRYPTED:v1:...` format)

## Build

```bash
make clean && make
./scripts/verify-release.sh
```

Artifacts:

- `mikroclaw` (dynamic)
- `mikroclaw-static-musl` (fully static)
- `mikrotik-docker` (`make mikrotik-docker`)

## Commands

```bash
./mikroclaw agent
./mikroclaw gateway [--port 0]
./mikroclaw daemon
./mikroclaw status
./mikroclaw doctor
./mikroclaw channel
./mikroclaw config --dump
./mikroclaw integrations [list|info <name>]
./mikroclaw identity [--rotate]
./mikroclaw encrypt KEY=VALUE
```

## Gateway API

- `GET /health`
- `GET /health/heartbeat`
- `POST /pair`
- `POST /tasks`
- `GET /tasks`
- `GET /tasks/:id`
- `DELETE /tasks/:id`

When `PAIRING_REQUIRED=1`, authenticated bearer token flow is enforced for non-health routes.

## Environment Variables

See `docs/ENV_VARS.md` for full reference. Key groups:

- LLM/provider: `LLM_PROVIDER`, `LLM_BASE_URL`, `LLM_API_KEY`, `RELIABLE_PROVIDERS`, `LLM_STREAMING`
- memU: `MEMU_API_KEY`, `MEMU_BASE_URL`, `MEMU_DEVICE_ID`
- RouterOS: `ROUTER_HOST`, `ROUTER_USER`, `ROUTER_PASS`, `ROUTEROS_FIREWALL`, `ROUTEROS_ALLOW_SUBNETS`
- Gateway: `GATEWAY_PORT`, `GATEWAY_BIND`, `PAIRING_REQUIRED`
- Channels: `BOT_TOKEN`, `DISCORD_WEBHOOK_URL`, `SLACK_WEBHOOK_URL`, `TELEGRAM_ALLOWLIST`, `DISCORD_ALLOWLIST`, `SLACK_ALLOWLIST`
- Security/tools: `ALLOWED_SHELL_CMDS`, `FORBIDDEN_PATHS`, `MEMU_ENCRYPTION_KEY`, `WEBSCRAPE_SERVICES`, `SCRAPINGBEE_API_KEY`

## Memory Model

- Runtime memory persistence is memU-backed (`memory_store`, `memory_recall`, `memory_forget`)
- Boot config may be rehydrated from memU
- Static RouterOS container builds use memU stubs if cloud memory is disabled

## Integrations Registry

`integrations list` currently includes:

- Providers: openrouter, openai, anthropic, ollama
- Channels: telegram, discord, slack
- Platform: routeros
- Cloud data: memu, zai, jina, firecrawl

## Security Posture

- Pairing-token auth (`/pair`)
- Rate limiting + auth lockout
- Sender allowlists (empty list denies all)
- Workspace-only file path checks
- Forbidden path defaults + optional custom list
- Symlink escape prevention
- Encrypted secrets helper (`encrypt` command)

See `SECURITY.md`.

## Docs

- `docs/ARCHITECTURE.md`
- `docs/ENV_VARS.md`
- `CHANGELOG.md`
- `SECURITY.md`
