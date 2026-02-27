# MikroClaw Installer - Security Improvements

## Summary of Changes

### 1. Environment Variable Substitution
The config loader now supports `${ENV_VAR}` and `${ENV_VAR:-default}` syntax:

```json
{
  "routeros": {
    "password": "${MIKROCLAW_ROUTEROS_PASSWORD}",
    "host": "${MIKROCLAW_ROUTEROS_HOST:-192.168.88.1}"
  }
}
```

### 2. Port 7778 Validation
Port 7778 is the RouterOS API SSL port. The validator now ensures:
- If port is 7778, `use_tls` MUST be `true`
- Warning if password is less than 12 characters

### 3. Files Added/Updated

| File | Purpose |
|------|---------|
| `pkg/config/config.go` | Added `ExpandEnv()` function, updated `LoadConfig()` |
| `pkg/config/config_test.go` | New tests for env expansion and validation |
| `.env.example` | Template for environment variables |
| `mikroclaw-config.json` | Updated to use env vars |
| `.gitignore` | Added `mikroclaw-config.json` to prevent commits |
| `SECURITY_PLAN.md` | Full security planning document |

## How to Use Secure Configuration

### Step 1: Create your .env file
```bash
cd /home/agent/mikroclaw/installer-go
cp .env.example .env
# Edit .env with your actual secrets
```

### Step 2: Source the environment
```bash
export $(grep -v '^#' .env | xargs)
```

Or load directly when running:
```bash
set -a && source .env && set +a && ./bin/mikroclaw-install -config mikroclaw-config.json
```

### Step 3: Verify config loads correctly
```bash
./bin/mikroclaw-install -config mikroclaw-config.json -check
```

## Current mikroclaw-config.json Structure

```json
{
  "routeros": {
    "host": "${MIKROCLAW_ROUTEROS_HOST:-192.168.100.1}",
    "port": ${MIKROCLAW_ROUTEROS_PORT:-7778},
    "username": "${MIKROCLAW_ROUTEROS_USERNAME:-jefferson}",
    "password": "${MIKROCLAW_ROUTEROS_PASSWORD}",
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
    "llm_provider": "cerebras",
    "api_key": "${MIKROCLAW_CEREBRAS_API_KEY}",
    "memu_key": "${MIKROCLAW_MEMU_KEY}",
    "telegram_token": "${MIKROCLAW_TELEGRAM_TOKEN}"
  }
}
```

## Immediate Actions Required

1. **Rotate exposed credentials** - The old config had plaintext passwords
2. **Create .env file** with your actual secrets
3. **Never commit** `mikroclaw-config.json` or `.env`
4. **Set proper permissions**: `chmod 600 .env mikroclaw-config.json`

## Future Encryption Options

For even more security, consider:

### Option A: Age Encryption
```bash
# Encrypt config
age -r age1... -o config.json.age mikroclaw-config.json

# Decrypt at runtime
age -d config.json.age | ./bin/mikroclaw-install -config -
```

### Option B: System Keyring
Use OS-native keyring for credential storage (future enhancement).

## Testing

All tests pass:
```
ok      github.com/openclaw/mikroclaw-installer/pkg/config    0.003s
ok      github.com/openclaw/mikroclaw-installer/pkg/preflight 0.003s
```
