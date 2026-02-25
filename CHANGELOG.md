# Changelog

All notable changes to MikroClaw will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.3.0] - 2026-02-24

### Added
- Provider registry with 13 named providers and auth style mapping (bearer + x-api-key)
- LLM stream parser module and `llm_chat_stream`/`llm_chat_reliable` support
- Sender allowlists for Telegram/Discord/Slack inbound messages
- `memory_forget` and `web_scrape` tools in function registry
- Config validation module and `config --dump` command (redacted output)
- Encrypted secret support (`ENCRYPTED:v1:`) with `encrypt KEY=VALUE` command
- RouterOS helper APIs for script, scheduler, and firewall operations
- Gateway heartbeat endpoint (`GET /health/heartbeat`)
- Integrations command and identity command (`identity [--rotate]`)
- New tests: provider registry, llm stream, allowlist, config validation, crypto, identity, gateway port

### Changed
- Gateway now supports bind address configuration and port `0` randomization
- Gateway startup can auto-apply RouterOS firewall rules and scheduler heartbeat job
- Function path validation now blocks forbidden paths and symlink escapes
- Verify-release suite expanded from 30 to 42 checks

### Removed
- No reintroduction of removed local/cloud legacy memory persistence paths (memU-first remains)

### Security
- Forbidden path expansion + symlink escape checks for file tools
- Sender allowlist enforcement on inbound channel traffic
- Encrypted secret workflow integrated into runtime and tests

---

## [0.2.0] - 2026-02-21

### Added
- **OpenAI-Compatible Provider System** - Support any OpenAI-compatible endpoint
  - Configurable URL, auth type (Bearer/API Key/OAuth2), model
  - Providers: OpenRouter, OpenAI, LocalAI, Ollama
- **Multi-Turn Conversations** - 32-turn conversation history in RAM
- **Function Calling** - 8 built-in functions:
  - `routeros_execute` - Execute RouterOS commands
  - `web_search` - Search via Perplexity/Exa/Z.ai
- `memory_recall` - Recall from cloud memory
- `memory_store` - Store to cloud memory
  - `skill_invoke` - Execute Lua skills
  - `skill_list` - List available skills
  - `parse_url` - Parse URLs
  - `health_check` - System metrics
- **TUI Installer** - Human-friendly installation
  - whiptail → dialog → ASCII fallback
  - Interactive and command-line modes
  - JSON configuration generation
  - RouterOS and Linux install paths
- **Secure Deployment Tools** - `deploy/build.sh` and `deploy/install.sh`
  - AES-256-CBC encrypted secrets
  - Device-bound encryption (router serial)
  - In-memory secret handling (never saved to disk)

### Changed
- **README.md** - Complete rewrite with quick start, features, usage
- **.gitignore** - Comprehensive ignore rules for GitHub best practices

### Security
- Secrets never persisted to disk
- Encrypted transport (TLS 1.3)
- Device-bound deployment packages

---

## [0.1.0] - 2026-02-20

### Added
- **Core Framework** - Event loop, configuration, error handling
- **HTTP Client** - HTTPS via mbedTLS, synchronous requests
- **JSON Parser** - jsmn-based, minimal footprint
- **LLM Integration** - OpenRouter API client
- **RouterOS API** - REST API client for command execution
- **Telegram Channel** - Bot API polling
- **Gateway API** - OpenClaw-compatible HTTP server
- **Local Storage** - 2.88MB filesystem storage
- **Legacy cloud storage skeleton** - Partial legacy implementation (removed in later versions)
- **Cron Integration** - RouterOS scheduler interface
- **Build System** - Makefile with feature flags
- **Test Suite** - TLS, JSON escaping, legacy storage tests

### Technical
- Binary size: ~30KB
- Memory usage: ~2MB runtime
- Static linking with musl
- mbedTLS for TLS (not integrated yet)

---

## [0.0.1] - 2026-02-16

### Added
- Initial scaffold
- Project structure
- Documentation skeleton

---

## Future Roadmap

### [0.4.0] - Planned
- Lua skill runtime
- Skill marketplace (cloud-based)
- Discord/Slack channels
- Package managers (Homebrew, APT)

### [1.0.0] - Planned
- Production hardened
- Full test coverage
- Security audit passed
- Enterprise features

---

## Contributing

When adding changes:
1. Add to `[Unreleased]` section
2. Use categories: Added, Changed, Deprecated, Removed, Fixed, Security
3. Reference issues/PRs where applicable
4. Keep chronological order (newest first)

Example:
```markdown
### Added
- New feature description (#123)
```
