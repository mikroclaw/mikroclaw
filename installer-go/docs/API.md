# Gateway API

Last updated: 2026-02-25

This reference is generated from `src/mikroclaw.c` and related auth/rate-limit modules.

## Base

- Default bind/port: `GATEWAY_BIND` / `GATEWAY_PORT` (default port `18789`)
- Content type: JSON responses for API routes

## Auth and Rate Limit

- Pairing endpoint (`POST /pair`) exchanges an `X-Pairing-Code` header for a bearer token.
- When `PAIRING_REQUIRED=1`, all routes except `/health`, `/health/heartbeat`, and `/pair` require `Authorization: Bearer <token>`.
- Request rate limiting can return `429` with `{"error":"rate limit exceeded"}`.
- Auth lockout can return `429` with `{"error":"auth locked","retry_after":<seconds>}`.

## Endpoints

### `GET /health`

- Returns component health:
  - `status`
  - `components.llm`
  - `components.gateway`
  - `components.routeros`
  - `components.memu`

Example response:

```json
{"status":"ok","components":{"llm":true,"gateway":true,"routeros":true,"memu":true}}
```

### `GET /health/heartbeat`

- Lightweight heartbeat probe.

Example response:

```json
{"heartbeat":"ok"}
```

### `POST /pair`

- Header required: `X-Pairing-Code: <code>`
- Success: returns short-lived bearer token.
- Failure: `403 {"error":"invalid pairing code"}`

Example response:

```json
{"paired":true,"token":"<bearer-token>"}
```

### `POST /tasks`

- Requires subagent runtime enabled.
- Request body must include `type`.
- Success: `{"task_id":"...","status":"queued"}`
- Failure:
  - `400 {"error":"missing type"}`
  - `503 {"error":"queue full"}`

Example body:

```json
{"type":"analyze","input":"..."}
```

### `GET /tasks`

- Requires subagent runtime enabled.
- Returns task list JSON from subagent queue.
- Failure: `500 {"error":"list failed"}`

### `GET /tasks/:id`

- Requires subagent runtime enabled.
- Returns task status/result JSON.
- Failure: `404 {"error":"task not found"}`

### `DELETE /tasks/:id`

- Requires subagent runtime enabled.
- Success: `{"status":"cancelled"}`
- Failure: `404 {"error":"task not found"}`

## Minimal cURL Examples

```bash
curl -s "http://127.0.0.1:18789/health"
curl -s "http://127.0.0.1:18789/health/heartbeat"
curl -s -X POST "http://127.0.0.1:18789/pair" -H "X-Pairing-Code: <code>"
curl -s -X POST "http://127.0.0.1:18789/tasks" -H "Authorization: Bearer <token>" -H "Content-Type: application/json" -d '{"type":"analyze"}'
curl -s "http://127.0.0.1:18789/tasks" -H "Authorization: Bearer <token>"
curl -s "http://127.0.0.1:18789/tasks/<id>" -H "Authorization: Bearer <token>"
curl -s -X DELETE "http://127.0.0.1:18789/tasks/<id>" -H "Authorization: Bearer <token>"
```
