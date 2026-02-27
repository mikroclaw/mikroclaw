# MikroClaw Installer - Security Features

## Required Encryption

**All configuration files containing credentials MUST be encrypted.**

### How It Works

1. **Interactive Mode** (`-i` flag):
   - Prompts for all settings including passwords and API keys
   - **Requires encryption password** when saving config
   - Saves to `mikroclaw-config.json` (encrypted with Argon2id + ChaCha20-Poly1305)
   - File permissions: `0600` (owner read/write only)

2. **Loading Config**:
   - Auto-detects encrypted files
   - Prompts for decryption password
   - Falls back to unencrypted only if explicitly allowed

### Usage

#### Create Encrypted Config (Interactive)
```bash
./bin/mikroclaw-install -i
# ... answer prompts ...
# When saving, you'll be prompted for an encryption password
```

#### Use Encrypted Config
```bash
./bin/mikroclaw-install -config mikroclaw-config.json
# You'll be prompted for the decryption password
```

### Encryption Details

- **Algorithm**: ChaCha20-Poly1305 (AEAD)
- **Key Derivation**: Argon2id (3 iterations, 64MB memory, 4 threads)
- **Salt**: 16 bytes random
- **Nonce**: 12 bytes random per encryption
- **Format**: `base64(salt):base64(nonce+ciphertext)`

### File Permissions

The installer enforces restrictive permissions:
- Config files: `0600` (owner read/write only)
- No group or other access

### Migration from Unencrypted

If you have an old unencrypted config:
```bash
# Load it once (will warn about plaintext)
./bin/mikroclaw-install -config old-config.json

# Re-save it encrypted through interactive mode
./bin/mikroclaw-install -i
# Enter same settings, provide encryption password

# Securely delete old config
shred -u old-config.json
```

### Security Checklist

- [ ] Config files are encrypted
- [ ] File permissions are 0600
- [ ] Encryption password is strong (16+ characters)
- [ ] Different passwords for different environments
- [ ] Old unencrypted configs securely deleted
- [ ] No credentials in shell history (use interactive mode)

### API Keys in Config

The following sensitive values are encrypted in the config:
- RouterOS password
- LLM API keys (Cerebras, OpenRouter, etc.)
- MemU memory key
- Telegram bot token
- Discord webhook URL
- Registry authentication tokens

### Port 7778 Security

Port 7778 is the RouterOS API SSL port:
- **TLS is mandatory** - config validation will fail if `use_tls: false`
- Uses certificate verification (can be disabled for self-signed)
- Strongly recommended over port 8728 (HTTP API)
