# MikroClaw

MikroClaw deploys AI-powered network monitoring and management to MikroTik routers.

## Features

- Install on RouterOS, Linux, or Docker
- Multi-method RouterOS deployment (SSH, REST API, Binary API)
- Auto-detection of available connection methods
- LLM integration for intelligent network management

## Installation

```bash
# Interactive mode
./mikroclaw-install.sh

# RouterOS with auto-detection
./mikroclaw-install.sh --target routeros --ip 192.168.88.1 --user admin --pass secret
```

## RouterOS Deployment Methods

MikroClaw supports three deployment methods for RouterOS. The installer auto-detects available methods, or you can specify one explicitly.

### Method Comparison

| Method | Port | Protocol | Requirements |
|--------|------|----------|--------------|
| REST API + /tool fetch | 443 (or 80) | HTTPS | Router internet access, REST API enabled |
| SSH/SCP | 22 (configurable) | SSH | SSH enabled, sshpass installed |
| MikroTik Binary API | 8728 (plain) or 8729 (SSL) | Binary | API service enabled |

### Auto-Detection

When `--method` is not specified, the installer tries methods in this order:

1. REST API (port 443, then 80)
2. SSH (ports 22, 2222, 8022)
3. Binary API (port 8729 SSL, then 8728 plain)

The first successful connection is used for deployment.

### CLI Examples

Specify a method explicitly:

```bash
# REST API (recommended, most reliable)
./mikroclaw-install.sh --target routeros --ip 192.168.88.1 --method rest

# SSH with custom port
./mikroclaw-install.sh --target routeros --ip 192.168.88.1 --method ssh --ssh-port 2222

# Binary API with custom port
./mikroclaw-install.sh --target routeros --ip 192.168.88.1 --method api --api-port 8729
```

## Configuration

Set required environment variables or use flags:

- `--bot-token`: Telegram bot token (RouterOS target)
- `--api-key`: LLM API key (RouterOS target)
- `--provider`: LLM provider (openrouter, groq, etc.)
- `--model`: Override default LLM model

## Supported Targets

- **routeros**: MikroTik RouterOS devices
- **linux**: Linux servers

## License

MIT
