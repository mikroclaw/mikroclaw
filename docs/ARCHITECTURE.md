# MikroClaw Architecture

Last updated: 2026-02-25

## Runtime Flow

```text
Inbound Channel/Gateway
    -> Allowlist and auth checks
    -> LLM prompt execution (provider registry + optional reliable fallback)
    -> Optional RouterOS command execution
    -> Tool/function execution
    -> Response routing (channel or gateway)
    -> Optional memU persistence
]

## Data Flow

```
Inbound Request (Telegram/Discord/Slack/Direct)
        |
        v
  [Allowlist Check] ----(rejected)----> X
        |
        v
  [Gateway Auth] ----(unauthorized)----> 403
        |
        v
  [Rate Limit] ----(throttled)----> 429
        |
        v
[LLM Provider] (with reliable fallback chain)
        |
        v
[Tool Router] -----> [RouterOS API]
        |                [shell_exec]
        |                [file_read/write]
        |                [web_search/scrape]
        |                [memory_*]
        v
  [memU Cloud] <----(persistence)----
        |
        v
   Response Routing (channel or gateway)
```

## Core Runtime
```

## Core Runtime

- `src/main.c`: bootstrap, CLI mode routing, config validation, startup/shutdown hooks
- `src/mikroclaw.c`: orchestration loop, gateway endpoints, request routing, task APIs
- `src/cli.c`: command parsing (`agent`, `gateway`, `daemon`, `status`, `doctor`, `config`, `integrations`, `identity`)
- `src/log.c`: runtime log levels and format controls (`LOG_LEVEL`, `LOG_FORMAT`)

## LLM + Provider Layer

- `src/llm.c`: chat transport and reliable provider fallback chain
- `src/llm_stream.c`: streamed response chunk parsing helpers
- `src/provider_registry.c`: 13 named providers with auth metadata

## Tooling + Execution Layer

- `src/functions.c`: function/tool registry, schema exposure, execution guards
- `src/task_handlers.c`: maps task type to handler implementation
- `src/task_queue.c`: async queue primitives for task dispatch
- `src/worker_pool.c`: worker execution pool for queued task processing
- `src/subagent.c`: task submission/list/get/cancel APIs backing `/tasks`
- `src/tasks/investigate.c`: RouterOS-targeted investigation task (collects live data + LLM diagnosis)
- `src/tasks/analyze.c`: RouterOS scoped analysis task (collects domain data + LLM findings/recommendations)
- `src/tasks/summarize.c`: summarize task handler
- `src/tasks/skill_invoke.c`: skill invocation task handler

## Gateway + Security Layer

- `src/gateway.c`: socket listener, request polling, response writeback
- `src/gateway_auth.c`: pairing code exchange and bearer token validation
- `src/rate_limit.c`: per-IP throttling and auth lockout tracking

Gateway routes implemented in `src/mikroclaw.c`:

- `GET /health`
- `GET /health/heartbeat`
- `POST /pair`
- `POST /tasks`
- `GET /tasks`
- `GET /tasks/:id`
- `DELETE /tasks/:id`

## Channel Layer

- `src/channels/telegram.c`: polling, send, allowlist checks
- `src/channels/discord.c`: webhook send/inbound parse, allowlist checks
- `src/channels/slack.c`: webhook send/inbound parse, allowlist checks
- `src/channels/allowlist.c`: shared allowlist parser/checks
- `src/channel_supervisor.c`: channel lifecycle supervision

## RouterOS + Scheduler Layer

- `src/routeros.c`: RouterOS REST API operations and command execution
- `src/cron.c`: scheduler helper functions (heartbeat job support)

## Memory + Storage Layer

- `src/memu_client.c`: memU cloud integration
- `src/memu_client_stub.c`: stub implementation when cloud memory disabled
- `src/config_memu.c`: boot-time config rehydration from memU
- `src/storage_local.c`: local filesystem storage helpers

## Validation + Identity + Crypto

- `src/config_validate.c`: environment/config validation rules
- `src/identity.c`: agent identity resolution and rotation support
- `src/crypto.c`: encrypt/decrypt support for `ENCRYPTED:v1:` values
- `src/base64.c`: base64 helpers
- `src/json.c`: JSON helper utilities
- `src/http.c`: lightweight HTTP request helpers
- `src/http_client.c`: libcurl-backed HTTP client used by cloud/webhook integrations

## Build Targets

- `make`: dynamic binary (`mikroclaw`)
- `make static-musl`: fully static binary (`mikroclaw-static-musl`)
- `make mikrotik-docker`: RouterOS container binary (`mikrotik-docker`)
- `./scripts/verify-release.sh`: release verification suite
