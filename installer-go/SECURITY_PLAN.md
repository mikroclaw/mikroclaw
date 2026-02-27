# MikroClaw Installer Security Plan

## Current Configuration Analysis

### Issues Found

1. **CRITICAL: Plaintext Credentials**
   - RouterOS password visible in JSON
   - Cerebras API key exposed
   - MemU key exposed
   - Telegram bot token visible

2. **Port Configuration**
   - Port 7778 set correctly for RouterOS API SSL
   - `use_tls: true` is correct for SSL port

3. **Missing Fields**
   - No mounts configured for persistent storage
   - No environment variables defined
   - No interface binding specified

## Proposed Solutions

### Option 1: Environment Variable Injection (Recommended)

Store secrets in environment, inject at runtime:

```bash
# .env file (add to .gitignore!)
MIKROCLAW_ROUTEROS_PASSWORD=DragonsHere2026!
MIKROCLAW_CEREBRAS_API_KEY=csk-...
MIKROCLAW_MEMU_KEY=mu_...
MIKROCLAW_TELEGRAM_TOKEN=8084...
```

Config becomes:
```json
{
  "routeros": {
    "host": "192.168.100.1",
    "port": 7778,
    "username": "jefferson",
    "password": "${MIKROCLAW_ROUTEROS_PASSWORD}",
    "use_tls": true
  },
  "mikroclaw": {
    "api_key": "${MIKROCLAW_CEREBRAS_API_KEY}",
    "memu_key": "${MIKROCLAW_MEMU_KEY}",
    "telegram_token": "${MIKROCLAW_TELEGRAM_TOKEN}"
  }
}
```

### Option 2: Age/SOPS Encryption

Encrypt config with age or Mozilla SOPS:

```bash
# Encrypt with age
age -r age1... -o mikroclaw-config.json.age mikroclaw-config.json

# Decrypt at runtime
age -d mikroclaw-config.json.age | mikroclaw-install -config -
```

### Option 3: System Keyring Integration

Use OS keyring for secrets:

```go
// Use 99designs/keyring or similar
password, _ := keyring.Get("mikroclaw", "routeros_password")
```

## Implementation Plan

### Phase 1: Immediate Fixes
1. Update config.go to support env var substitution
2. Add `mikroclaw-install encrypt` command
3. Create `.env.example` template
4. Update .gitignore to exclude `.env` and `*.json`

### Phase 2: Keyring Integration
1. Add keyring support for Linux (dbus/secret-service)
2. Add macOS Keychain support
3. Add Windows Credential Manager support

### Phase 3: Config Validation
1. Validate port 7778 requires `use_tls: true`
2. Warn if password length < 12 chars
3. Check API key format validity

## Updated Configuration Structure

```json
{
  "routeros": {
    "host": "192.168.100.1",
    "port": 7778,
    "username": "jefferson",
    "password_source": "env:MIKROCLAW_ROUTEROS_PASSWORD",
    "use_tls": true
  },
  "container": {
    "name": "mikroclaw",
    "image": "ghcr.io/openclaw/mikroclaw:latest",
    "auto_start": true,
    "mounts": {
      "disk1/mikroclaw-data": "/app/data"
    }
  },
  "mikroclaw": {
    "api_key_source": "env:MIKROCLAW_API_KEY",
    "memu_key_source": "env:MIKROCLAW_MEMU_KEY",
    "telegram_token_source": "env:MIKROCLAW_TELEGRAM_TOKEN"
  }
}
```

## Security Checklist

- [ ] Move all secrets to environment variables
- [ ] Add `mikroclaw-config.json` to `.gitignore`
- [ ] Encrypt backup of original config
- [ ] Validate TLS is used with port 7778
- [ ] Use strong password (12+ chars, mixed case, symbols)
- [ ] Rotate exposed API keys immediately
- [ ] Enable Telegram allowlist (already done: "*")
- [ ] Configure container mounts for persistence
