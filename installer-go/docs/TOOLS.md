# Function Tools Registry

Last updated: 2026-02-25

Source of truth: `src/functions.c` (`function_register_with_schema(...)`).

## Registered Tools (14)

### `parse_url`

- Description: Parse URL host/path
- Schema:

```json
{"type":"object","properties":{"url":{"type":"string"}},"required":["url"]}
```

### `health_check`

- Description: Return process health
- Schema:

```json
{"type":"object","properties":{}}
```

### `memory_store`

- Description: Store key/value memory
- Schema:

```json
{"type":"object","properties":{"key":{"type":"string"},"value":{"type":"string"}},"required":["key","value"]}
```

### `memory_recall`

- Description: Recall key memory
- Schema:

```json
{"type":"object","properties":{"key":{"type":"string"}},"required":["key"]}
```

### `memory_forget`

- Description: Forget key memory
- Schema:

```json
{"type":"object","properties":{"key":{"type":"string"}},"required":["key"]}
```

### `web_search`

- Description: Search web documents
- Schema:

```json
{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}
```

### `web_scrape`

- Description: Scrape URL via cloud services
- Schema:

```json
{"type":"object","properties":{"url":{"type":"string"}},"required":["url"]}
```

### `skill_list`

- Description: List skills directory entries
- Schema:

```json
{"type":"object","properties":{}}
```

### `skill_invoke`

- Description: Invoke executable skill from skills directory
- Schema:

```json
{"type":"object","properties":{"skill":{"type":"string"},"params":{"type":"string"}},"required":["skill"]}
```

### `routeros_execute`

- Description: Execute RouterOS command from args
- Runtime behavior: Initializes RouterOS client from `ROUTER_HOST`/`ROUTER_USER`/`ROUTER_PASS` and calls `POST /rest/execute`
- Schema:

```json
{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}
```

### `shell_exec`

- Description: Execute allowed shell command
- Schema:

```json
{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}
```

### `file_read`

- Description: Read file in workspace
- Schema:

```json
{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}
```

### `file_write`

- Description: Write file in workspace
- Schema:

```json
{"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]}
```

### `composio_call`

- Description: Call Composio-compatible endpoint
- Schema:

```json
{"type":"object","properties":{"tool":{"type":"string"},"input":{"type":"string"}},"required":["tool","input"]}
```

## Runtime Notes

- `shell_exec` is constrained by `ALLOWED_SHELL_CMDS`.
- `file_read`/`file_write` enforce workspace checks and forbidden paths (`FORBIDDEN_PATHS`) with symlink escape protection.
- `web_scrape` behavior is controlled by `WEBSCRAPE_SERVICES`, `SCRAPINGBEE_API_KEY`, and optional `WEBSCRAPE_MOCK_RESPONSE`.
- `composio_call` requires `COMPOSIO_URL` and `COMPOSIO_API_KEY`.
