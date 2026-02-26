#!/usr/bin/env python3
# pyright: basic

import argparse
import ipaddress
import json
import os
import re
import stat
import sys
import urllib.request
from dataclasses import dataclass
from typing import NoReturn
from typing import Optional
import getpass


def ui_menu(title: str, *options: str) -> int:
    print(file=sys.stderr)
    print(title, file=sys.stderr)
    print(file=sys.stderr)
    for i, opt in enumerate(options, 1):
        print(f"  {i}) {opt}", file=sys.stderr)
    print(file=sys.stderr)
    while True:
        try:
            choice = input(f"Choice [1-{len(options)}]: ")
            idx = int(choice)
            if 1 <= idx <= len(options):
                return idx
        except (ValueError, EOFError):
            pass

def ui_input(prompt: str, default: Optional[str] = None) -> str:
    if default is not None:
        full_prompt = f"{prompt} [{default}]: "
    else:
        full_prompt = f"{prompt}: "
    value = input(full_prompt).strip()
    if default is not None and not value:
        return default
    return value

def ui_input_secret(prompt: str) -> str:
    return getpass.getpass(f"{prompt}: ")

def ui_progress(msg: str) -> None:
    print(f"⏳ {msg}...", end="", flush=True, file=sys.stderr)

def ui_done() -> None:
    print(" ✅", file=sys.stderr)

def ui_error(msg: str) -> None:
    print(f" ❌ {msg}", file=sys.stderr)

def ui_msg(msg: str) -> None:
    print(f"  {msg}", file=sys.stderr)

def ui_banner() -> None:
    banner = f'''
╔═══════════════════════════════════════════╗
║                                           ║
║   ███╗   ███╗██╗██╗  ██╗██████╗  ██████╗  ║
║   ████╗ ████║██║██║ ██╔╝██╔══██╗██╔════╝  ║
║   ██╔████╔██║██║█████╔╝ ██████╔╝██║       ║
║   ██║╚██╔╝██║██║██╔═██╗ ██╔══██╗██║       ║
║   ██║ ╚═╝ ██║██║██║  ██╗██║  ██║╚██████╗  ║
║   ╚═╝     ╚═╝╚═╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ║
║                                           ║
║        Installer {INSTALLER_VERSION:<18}     ║
║                                           ║
╚═══════════════════════════════════════════╝
'''
    print(banner, file=sys.stderr)

def ui_clear() -> None:
    print("\033[2J\033[H", end="", file=sys.stderr)

INSTALLER_VERSION = "2025.02.25:BETA2"
BINARY_URL_TEMPLATE = "https://github.com/mikroclaw/mikroclaw/releases/latest/download/mikroclaw-{platform}"
TRANSFER_TIMEOUT = 120

PROVIDER_DEFAULTS = {
    "openrouter": ("https://openrouter.ai/api/v1", "google/gemini-flash", "bearer"),
    "openai": ("https://api.openai.com/v1", "gpt-4o-mini", "bearer"),
    "anthropic": (
        "https://api.anthropic.com/v1",
        "claude-3-5-sonnet-latest",
        "api_key",
    ),
    "google": (
        "https://generativelanguage.googleapis.com/v1beta/openai",
        "gemini-1.5-flash",
        "bearer",
    ),
    "azure": (
        "https://YOUR_RESOURCE_NAME.openai.azure.com/openai/v1",
        "gpt-4o-mini",
        "api_key",
    ),
    "groq": ("https://api.groq.com/openai/v1", "llama-3.3-70b-versatile", "bearer"),
    "together": (
        "https://api.together.xyz/v1",
        "meta-llama/Llama-3.3-70B-Instruct-Turbo",
        "bearer",
    ),
    "replicate": (
        "https://api.replicate.com/v1",
        "meta/meta-llama-3-70b-instruct",
        "bearer",
    ),
    "mistral": ("https://api.mistral.ai/v1", "mistral-small-latest", "bearer"),
    "cohere": ("https://api.cohere.ai/compatibility/v1", "command-r-plus", "api_key"),
    "ai21": ("https://api.ai21.com/studio/v1", "jamba-1.5-large", "api_key"),
    "fireworks": (
        "https://api.fireworks.ai/inference/v1",
        "accounts/fireworks/models/llama-v3p1-70b-instruct",
        "bearer",
    ),
    "vertex": (
        "https://aiplatform.googleapis.com/v1/projects/YOUR_PROJECT/locations/us-central1/publishers/google/models",
        "gemini-1.5-flash",
        "bearer",
    ),
    "bedrock": (
        "https://bedrock-runtime.us-east-1.amazonaws.com",
        "anthropic.claude-3-5-sonnet-20240620-v1:0",
        "api_key",
    ),
    "cloudflare": (
        "https://api.cloudflare.com/client/v4/accounts/YOUR_ACCOUNT_ID/ai/v1",
        "@cf/meta/llama-3.1-8b-instruct",
        "bearer",
    ),
    "deepseek": ("https://api.deepseek.com/v1", "deepseek-chat", "bearer"),
    "ollama": ("http://127.0.0.1:11434/v1", "llama3.1", "bearer"),
}

VALID_PROVIDERS = tuple(PROVIDER_DEFAULTS.keys())


@dataclass
class InstallerConfig:
    target: Optional[str]
    ip: Optional[str]
    user: str
    password: str
    method: Optional[str]
    provider: str
    bot_token: str
    api_key: str
    base_url: str
    model: str
    ssh_port: Optional[int]
    api_port: Optional[int]


class InstallerArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> NoReturn:
        self.print_usage(sys.stderr)
        self.exit(1, f"Error: {message}\n")


def port_value(raw: str) -> int:
    try:
        value = int(raw)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "must be an integer in range 1..65535"
        ) from exc
    if value < 1 or value > 65535:
        raise argparse.ArgumentTypeError("must be an integer in range 1..65535")
    return value


def is_valid_ipv4(value: str) -> bool:
    try:
        parsed = ipaddress.ip_address(value)
    except ValueError:
        return False
    return parsed.version == 4


def is_valid_user(value: str) -> bool:
    return bool(value) and re.fullmatch(r"[A-Za-z0-9._-]+", value) is not None


def is_valid_provider(value: str) -> bool:
    return value in VALID_PROVIDERS


def config_create(
    bot_token: str,
    api_key: str,
    provider: str = "openrouter",
    base_url: str = "",
    model: str = "",
) -> str:
    provider_key = provider if provider in PROVIDER_DEFAULTS else "openrouter"
    default_url, default_model, auth_type = PROVIDER_DEFAULTS[provider_key]
    payload = {
        "BOT_TOKEN": bot_token,
        "LLM_API_KEY": api_key,
        "LLM_PROVIDER": provider_key,
        "LLM_BASE_URL": base_url or default_url,
        "MODEL": model or default_model,
        "AUTH_TYPE": auth_type,
        "allowlists": {
            "telegram": "",
            "discord": "",
            "slack": "",
        },
        "memory": {
            "enabled": False,
            "provider": None,
        },
    }
    return json.dumps(payload, indent=2)


def download_binary(platform: str, output_path: str) -> None:
    url = BINARY_URL_TEMPLATE.format(platform=platform)
    with urllib.request.urlopen(url, timeout=TRANSFER_TIMEOUT) as response:
        data = response.read()
    with open(output_path, "wb") as handle:
        handle.write(data)
    mode = os.stat(output_path).st_mode
    os.chmod(output_path, mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def build_parser() -> argparse.ArgumentParser:
    parser = InstallerArgumentParser(
        prog="mikroclaw-install.py",
        usage="%(prog)s [OPTIONS]",
        description=f"MikroClaw Installer {INSTALLER_VERSION}",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("--target", help="Install target: routeros, linux")
    parser.add_argument("--ip", help="Router IP (for routeros target)")
    parser.add_argument(
        "--user", default="admin", help="Router username (default: admin)"
    )
    parser.add_argument("--pass", dest="password", default="", help="Router password")
    parser.add_argument(
        "--method",
        help="Deployment method: ssh, rest, api (auto-detect if not specified)",
    )
    parser.add_argument(
        "--provider", default="openrouter", help="LLM provider (default: openrouter)"
    )
    parser.add_argument(
        "--bot-token", default="", help="Telegram bot token (routeros target)"
    )
    parser.add_argument("--api-key", default="", help="LLM API key (routeros target)")
    parser.add_argument("--base-url", default="", help="Override LLM base URL")
    parser.add_argument("--model", default="", help="Override LLM model")
    parser.add_argument(
        "--ssh-port", type=port_value, help="Explicit SSH method port override"
    )
    parser.add_argument(
        "--api-port", type=port_value, help="Explicit Binary API method port override"
    )
    return parser


def parse_args(argv=None) -> InstallerConfig:
    args = build_parser().parse_args(argv)
    return InstallerConfig(
        target=args.target,
        ip=args.ip,
        user=args.user,
        password=args.password,
        method=args.method,
        provider=args.provider,
        bot_token=args.bot_token,
        api_key=args.api_key,
        base_url=args.base_url,
        model=args.model,
        ssh_port=args.ssh_port,
        api_port=args.api_port,
    )


def _validate_args(cfg: InstallerConfig) -> Optional[str]:
    if cfg.target is None:
        return "--target is required in CLI mode"
    if cfg.target not in ("routeros", "linux"):
        return f"Unknown target '{cfg.target}'"
    if not is_valid_provider(cfg.provider):
        return f"Unsupported provider '{cfg.provider}'"
    if not is_valid_user(cfg.user):
        return "Invalid router username"
    if cfg.method and cfg.method not in ("ssh", "rest", "api"):
        return f"Unknown method '{cfg.method}'. Use: ssh, rest, or api"
    if cfg.target == "routeros":
        if not cfg.ip:
            return "--ip required for routeros target"
        if not is_valid_ipv4(cfg.ip):
            return "Invalid router IP"
    if cfg.ssh_port is not None and cfg.method not in (None, "ssh"):
        return "--ssh-port can only be used with --method ssh"
    if cfg.api_port is not None and cfg.method not in (None, "api"):
        return "--api-port can only be used with --method api"
    return None


def main(argv=None) -> int:
    try:
        cfg = parse_args(argv)
    except SystemExit as exc:
        code = exc.code
        if isinstance(code, int):
            return code
        return 1

    error = _validate_args(cfg)
    if error:
        print(f"Error: {error}", file=sys.stderr)
        if "Unsupported provider" in error:
            print(f"Valid providers: {', '.join(VALID_PROVIDERS)}", file=sys.stderr)
        return 1

    if cfg.target == "linux":
        print("Linux installation skeleton ready.")
        return 0

    _ = config_create(cfg.bot_token, cfg.api_key, cfg.provider, cfg.base_url, cfg.model)
    print("RouterOS installation skeleton ready.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
