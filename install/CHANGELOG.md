# Changelog

All notable changes to MikroClaw will be documented in this file.

## [Unreleased]

### Added

- **Multi-Method RouterOS Deployment** - RouterOS installation now supports three deployment methods with auto-detection:
  - REST API + /tool fetch (ports 443/80)
  - SSH/SCP (port 22, configurable)
  - MikroTik Binary API (ports 8728/8729)
  - Auto-detection tries methods in order: SSH → REST API → Binary API
  - New `--method` flag to specify deployment method explicitly
  - New `--ssh-port` and `--api-port` flags for explicit per-method port selection
  - Interactive method selection menu when multiple methods available
- **Python Installer Migration**
  - `mikroclaw-install.sh` is now a thin POSIX bootstrap (Python 3.8+ check + passthrough)
  - Core installer flow moved to `mikroclaw-install.py`
  - Legacy shell method libraries removed from `install/lib/`

## [2025.02.25:BETA2] - 2025-02-25

### Added

- Initial beta release
- RouterOS target support
- Linux target support
- LLM provider integration (OpenRouter, Groq)
