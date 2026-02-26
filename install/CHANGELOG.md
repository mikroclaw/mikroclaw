# Changelog

All notable changes to MikroClaw will be documented in this file.

## [Unreleased]

### Added

- **Multi-Method RouterOS Deployment** - RouterOS installation now supports three deployment methods with auto-detection:
  - REST API + /tool fetch (ports 443/80)
  - SSH/SCP (port 22, configurable)
  - MikroTik Binary API (ports 8728/8729)
  - Auto-detection tries methods in order: REST API → SSH → Binary API
  - New `--method` flag to specify deployment method explicitly
  - Interactive method selection menu when multiple methods available

## [2025.02.25:BETA2] - 2025-02-25

### Added

- Initial beta release
- RouterOS target support
- Linux target support
- LLM provider integration (OpenRouter, Groq)
