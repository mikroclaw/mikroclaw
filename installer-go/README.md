# MikroClaw

Ultra-lightweight AI agent runtime for MikroTik RouterOS container deployments.

**Website**: [www.mikroclaw.com](https://www.mikroclaw.com)

- Tiny C runtime designed for static builds
- memU cloud-first memory (no local conversational persistence)
- RouterOS-native automation via REST API
- Internal-network / VPN-first gateway model

## Current Release Focus

This repository is currently prepared for release `2025.02.25:BETA2` with:

- Provider registry (17 named providers)
- LLM streaming parser support
- Sender allowlists for Telegram/Discord/Slack
- Async subagent task API (`/tasks`)
- Gateway pairing + rate limiting + lockout
- Tool schema registry + security checks
- Encrypted secret values (`ENCRYPTED:v1:...` format)
- Installer refresh: full 17-provider selection, CLI provider/options, and runtime-aligned config output

## Why MikroClaw?

Traditional AI agents are heavy (Python runtimes, GB of dependencies) and require cloud connectivity. MikroClaw inverts this:

- **<50KB binary** vs GB-sized runtimes
- **Runs on RouterOS containers** (2MB RAM, no external dependencies)  
- **VPN-first**: Your AI agent lives on your network, not the public cloud
- **Zero local persistence**: Conversational memory via memU cloud—no data on disk

## Quick Start

```bash
# 1. Configure environment
export BOT_TOKEN="your_telegram_bot_token"
export OPENROUTER_KEY="your_openrouter_key"
export ROUTER_HOST="192.168.88.1"
export ROUTER_USER="admin"
export ROUTER_PASS="your_password"

# 2. Build
make clean && make

# 3. Run agent mode
./mikroclaw agent

# 4. Test via Gateway API
curl http://localhost:18789/health
```

## Build

```bash
make clean && make
./scripts/verify-release.sh
```

Check runtime version:

```bash
./mikroclaw --version
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

- `docs/ARCHITECTURE.md` - System architecture and data flow
- `docs/ENV_VARS.md` - Complete environment variable reference
- `docs/API.md` - Gateway API reference
- `docs/TOOLS.md` - Function tools registry
- `CHANGELOG.md` - Release history
- `ROADMAP.md` - Planned features and milestones
- `SECURITY.md` - Security policy and reporting
- `CODE_OF_CONDUCT.md` - Community guidelines
