#!/usr/bin/env python3
# pyright: basic

import argparse
import base64
import shutil
import subprocess
import ipaddress
import json
import os
import re
import stat
import sys
import tempfile
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import NoReturn
from typing import Optional
from typing import Callable
from typing import cast
import getpass
import socket
import ssl


def ui_menu(title: str, *options: str) -> int:
    print(file=sys.stderr)
    print(title, file=sys.stderr)
    print(file=sys.stderr)
    for i, opt in enumerate(options, 1):
        print(f"  [{i}] {opt}", file=sys.stderr)
    print(file=sys.stderr)
    while True:
        try:
            print(f"Choice [1-{len(options)}]: ", end="", file=sys.stderr, flush=True)
            choice = input()
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
    print(full_prompt, end="", file=sys.stderr, flush=True)
    value = input().strip()
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
    banner = f"""
╔═══════════════════════════════════════════╗
║               MikroClaw                   ║
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
"""
    print(banner, file=sys.stderr)


def ui_clear() -> None:
    print("\033[2J\033[H", end="", file=sys.stderr)


def ui_init() -> None:
    return None


class InstallerError(Exception):
    def __init__(self, message: str):
        self.message = message
        super().__init__(message)


class ConnectionError(InstallerError):
    pass


class AuthError(InstallerError):
    pass


class TimeoutError(InstallerError):
    pass


CONNECT_TIMEOUT = 10
DETECT_RETRIES = 2
DEPLOY_RETRIES = 2


def tcp_probe(host: str, port: int, timeout: int = 2) -> bool:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((host, port))
        sock.close()
        return result == 0
    except OSError:
        return False


def http_request(
    url: str,
    method: str = "GET",
    headers: Optional[dict] = None,
    data: Optional[bytes] = None,
    timeout: int = CONNECT_TIMEOUT,
) -> tuple:
    if method not in ("GET", "POST", "PUT", "DELETE", "PATCH"):
        raise ValueError(f"Unsupported HTTP method: {method}")
    req = urllib.request.Request(url, method=method)
    if headers:
        for key, value in headers.items():
            req.add_header(key, value)
    if data:
        req.data = data
    try:
        ctx = ssl._create_unverified_context()
        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as response:
            return (response.status, response.read())
    except urllib.error.HTTPError as exc:
        return (exc.code, exc.read())
    except urllib.error.URLError as exc:
        if isinstance(exc.reason, socket.timeout):
            raise TimeoutError(f"Request to {url} timed out") from exc
        raise ConnectionError(f"Failed to connect to {url}: {exc.reason}") from exc
    except socket.timeout:
        raise TimeoutError(f"Request to {url} timed out")


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

ADDITIONAL_VALID_PROVIDERS = {
    "xai",
    "perplexity",
    "kimi",
    "minimax",
    "zai",
    "synthetic",
}

VALID_PROVIDERS = tuple(PROVIDER_DEFAULTS.keys()) + tuple(
    sorted(ADDITIONAL_VALID_PROVIDERS)
)


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
    verbose: int


_VERBOSE = 0


def set_verbose(level: int) -> None:
    global _VERBOSE
    _VERBOSE = max(0, level)


def debug(message: str, level: int = 1) -> None:
    if _VERBOSE < level:
        return
    stamp = time.strftime("%H:%M:%S")
    print(f"[{stamp}] DEBUG: {message}", file=sys.stderr)


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
    if platform.startswith("http://") or platform.startswith("https://"):
        url = platform
    else:
        url = BINARY_URL_TEMPLATE.format(platform=platform)
    with urllib.request.urlopen(url, timeout=TRANSFER_TIMEOUT) as response:
        data = response.read()
    with open(output_path, "wb") as handle:
        handle.write(data)
    mode = os.stat(output_path).st_mode
    os.chmod(output_path, mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def _download_to_file(source: str, output_path: str) -> None:
    download_binary(source, output_path)


def _ssh_command_base(ip: str, user: str, port: int) -> list[str]:
    return [
        "ssh",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "UserKnownHostsFile=/dev/null",
        "-o",
        "ConnectTimeout=7",
        "-p",
        str(port),
        f"{user}@{ip}",
    ]


def _ssh_try_auth(ip: str, user: str, password: str, port: int) -> bool:
    base_cmd = _ssh_command_base(ip, user, port)
    if shutil.which("sshpass"):
        sshpass_cmd = ["sshpass", "-p", password] + base_cmd + ["true"]
        try:
            result = subprocess.run(
                sshpass_cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=7,
            )
            if result.returncode == 0:
                return True
        except (subprocess.TimeoutExpired, OSError):
            pass

    key_cmd = base_cmd + ["-o", "BatchMode=yes", "true"]
    try:
        result = subprocess.run(
            key_cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=7,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, OSError):
        return False


def _scp_upload(
    ip: str,
    user: str,
    password: str,
    port: int,
    local_path: str,
    remote_path: str,
) -> bool:
    scp_cmd = [
        "scp",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "UserKnownHostsFile=/dev/null",
        "-o",
        "ConnectTimeout=7",
        "-P",
        str(port),
        local_path,
        f"{user}@{ip}:{remote_path}",
    ]

    if shutil.which("sshpass"):
        command = ["sshpass", "-p", password] + scp_cmd
        try:
            result = subprocess.run(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=TRANSFER_TIMEOUT,
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, OSError):
            return False

    key_scp_cmd = [
        "scp",
        "-o",
        "BatchMode=yes",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "UserKnownHostsFile=/dev/null",
        "-o",
        "ConnectTimeout=7",
        "-P",
        str(port),
        local_path,
        f"{user}@{ip}:{remote_path}",
    ]
    try:
        result = subprocess.run(
            key_scp_cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=TRANSFER_TIMEOUT,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, OSError):
        return False


def method_ssh_test(ip: str, user: str, password: str, port: int = 22) -> bool:
    if not tcp_probe(ip, port):
        return False
    return _ssh_try_auth(ip, user, password, port)


def method_ssh_detect(ip: str, user: str, password: str) -> Optional[tuple[str, int]]:
    for port in (22, 2222, 8022):
        if method_ssh_test(ip, user, password, port):
            return ("ssh", port)
    return None


def method_ssh_deploy(
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    temp_dir = tempfile.mkdtemp()
    try:
        binary_path = os.path.join(temp_dir, "mikroclaw")
        config_path = os.path.join(temp_dir, "mikroclaw.env.json")

        _download_to_file(binary_url, binary_path)

        with open(config_path, "w", encoding="utf-8") as handle:
            handle.write(config_json)

        if not _scp_upload(ip, user, password, port, binary_path, "/disk1/mikroclaw"):
            return False
        if not _scp_upload(
            ip,
            user,
            password,
            port,
            config_path,
            "/disk1/mikroclaw.env.json",
        ):
            return False
        return True
    except (OSError, ValueError, TimeoutError, ConnectionError):
        return False
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def _api_encode_length(length: int) -> bytes:
    if length < 0:
        raise ValueError("length must be non-negative")
    if length < 0x80:
        return bytes([length])
    if length < 0x4000:
        encoded = length | 0x8000
        return encoded.to_bytes(2, byteorder="big")
    if length < 0x200000:
        encoded = length | 0xC00000
        return encoded.to_bytes(3, byteorder="big")
    raise ValueError("length too large for v1 API encoding")


def _api_send_sentence(sock: socket.socket, words: list[str]) -> None:
    for word in words:
        payload = word.encode("utf-8")
        sock.sendall(_api_encode_length(len(payload)) + payload)
    sock.sendall(b"\x00")


def method_api_test(ip: str, port: int = 8728) -> bool:
    return tcp_probe(ip, port, timeout=2)


def _api_connect(ip: str, port: int) -> socket.socket:
    sock = socket.create_connection((ip, port), timeout=CONNECT_TIMEOUT)
    if port == 8729:
        wrap_socket = cast(
            Optional[Callable[[socket.socket], socket.socket]],
            getattr(ssl, "wrap_socket", None),
        )
        if wrap_socket is not None:
            return wrap_socket(sock)
        ctx = ssl.create_default_context()
        return ctx.wrap_socket(sock, server_hostname=ip)
    return sock


def _api_try_login(ip: str, port: int, user: str, password: str) -> bool:
    try:
        with _api_connect(ip, port) as sock:
            _api_send_sentence(
                sock, ["/login", f"=name={user}", f"=password={password}"]
            )
            return True
    except OSError:
        return False


def method_api_detect(ip: str, user: str, password: str) -> Optional[tuple[str, int]]:
    _ = user
    _ = password
    for port in (8729, 8728):
        if method_api_test(ip, port):
            return ("api", port)
    return None


def method_api_deploy(
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    _ = user
    _ = password
    _ = config_json
    sock: Optional[socket.socket] = None
    try:
        sock = _api_connect(ip, port)
        _api_send_sentence(sock, ["/tool/fetch", f"=url={binary_url}"])
        return True
    except OSError:
        return False
    finally:
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass


def _rest_base_url(ip: str, port: int) -> str:
    scheme = "https" if port == 443 else "http"
    return f"{scheme}://{ip}:{port}"


def _rest_headers(user: str, password: str) -> dict[str, str]:
    raw_token = f"{user}:{password}".encode("utf-8")
    token = base64.b64encode(raw_token).decode("ascii")
    return {
        "Authorization": f"Basic {token}",
        "Accept": "application/json",
        "Content-Type": "application/json",
    }


def method_rest_test(ip: str, user: str, password: str, port: int = 443) -> bool:
    url = f"{_rest_base_url(ip, port)}/rest/system/resource"
    headers = _rest_headers(user, password)
    try:
        status, _ = http_request(
            url, method="GET", headers=headers, timeout=CONNECT_TIMEOUT
        )
    except (ConnectionError, TimeoutError, AuthError):
        return False
    return status == 200


def method_rest_detect(ip: str, user: str, password: str) -> Optional[tuple[str, int]]:
    for port in (443, 80):
        if method_rest_test(ip, user, password, port):
            return ("rest", port)
    return None


def detect_all_methods(ip: str, user: str, password: str) -> list[tuple[str, int]]:
    """Try all methods, return list of available methods.

    Detection order: SSH -> REST -> Binary API
    """
    available = []

    # SSH detection (ports 22, 2222, 8022)
    ssh_result = method_ssh_detect(ip, user, password)
    if ssh_result:
        available.append(ssh_result)

    # REST detection (ports 443, 80)
    rest_result = method_rest_detect(ip, user, password)
    if rest_result:
        available.append(rest_result)

    # API detection (ports 8729, 8728)
    api_result = method_api_detect(ip, user, password)
    if api_result:
        available.append(api_result)

    return available


def select_method_menu(available_methods: list[tuple[str, int]]) -> tuple[str, int]:
    """Show TUI menu, return chosen (method, port).

    If only 1 method: auto-select it.
    """
    if not available_methods:
        raise ValueError("No available methods to select from")

    if len(available_methods) == 1:
        return available_methods[0]

    # Build menu labels
    labels = []
    for method, port in available_methods:
        if method == "rest":
            labels.append(f"REST API + HTTPS fetch (https:{port}) [recommended]")
        elif method == "ssh":
            labels.append(f"SSH/SCP (ssh:{port})")
        elif method == "api":
            labels.append(f"MikroTik Binary API (api:{port})")
        else:
            labels.append(f"{method} (port {port})")

    choice = ui_menu("Select deployment method:", *labels)
    if choice < 1 or choice > len(available_methods):
        return available_methods[0]
    return available_methods[choice - 1]


def deploy_with_method(
    method: str,
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    """Route to correct deploy function based on method.

    Returns True/False from deploy function.
    """
    if method == "ssh":
        return method_ssh_deploy(ip, user, password, port, binary_url, config_json)
    elif method == "rest":
        return method_rest_deploy(ip, user, password, port, binary_url, config_json)
    elif method == "api":
        return method_api_deploy(ip, user, password, port, binary_url, config_json)
    else:
        raise ValueError(f"Unknown deployment method: {method}")


def method_rest_deploy(
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    if not method_rest_test(ip, user, password, port):
        return False

    base_url = _rest_base_url(ip, port)
    headers = _rest_headers(user, password)
    try:
        fetch_payload = {
            "url": binary_url,
            "dst-path": "disk1/mikroclaw",
            "mode": "https",
        }
        fetch_status, _ = http_request(
            f"{base_url}/rest/tool/fetch",
            method="POST",
            headers=headers,
            data=json.dumps(fetch_payload).encode("utf-8"),
            timeout=CONNECT_TIMEOUT,
        )
        if fetch_status != 200:
            return False

        file_payload = {
            "name": "disk1/mikroclaw.env.json",
            "type": "file",
            "content": config_json,
        }
        put_status, _ = http_request(
            f"{base_url}/rest/file",
            method="PUT",
            headers=headers,
            data=json.dumps(file_payload).encode("utf-8"),
            timeout=CONNECT_TIMEOUT,
        )

        if put_status != 200:
            script_text = (
                f":local c {json.dumps(config_json)};"
                ' /file remove [find where name="disk1/mikroclaw.env.json"];'
                ' /file add name="disk1/mikroclaw.env.json" contents=$c'
            )
            script_payload = {"script": script_text}
            script_status, _ = http_request(
                f"{base_url}/rest/system/script/run",
                method="POST",
                headers=headers,
                data=json.dumps(script_payload).encode("utf-8"),
                timeout=CONNECT_TIMEOUT,
            )
            if script_status != 200:
                return False

        list_status, list_body = http_request(
            f"{base_url}/rest/file",
            method="GET",
            headers=headers,
            timeout=CONNECT_TIMEOUT,
        )
        if list_status != 200:
            return False

        files = json.loads(list_body.decode("utf-8"))
        return any(item.get("name") == "disk1/mikroclaw.env.json" for item in files)
    except (OSError, ValueError, TimeoutError, ConnectionError, urllib.error.URLError):
        return False


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
    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Increase verbosity (repeat for more detail)",
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
        verbose=args.verbose,
    )


def _validate_args(cfg: InstallerConfig) -> Optional[str]:
    if cfg.target is None:
        return None
    if cfg.target not in ("routeros", "linux", "docker"):
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


def install_routeros_cli(cfg: InstallerConfig) -> int:
    set_verbose(cfg.verbose)
    if not cfg.ip:
        print("Error: --ip required for routeros target", file=sys.stderr)
        return 1

    config_json = config_create(
        cfg.bot_token,
        cfg.api_key,
        cfg.provider,
        cfg.base_url,
        cfg.model,
    )

    if cfg.method:
        method = cfg.method
        if method == "ssh":
            port = cfg.ssh_port or 22
        elif method == "rest":
            port = 443
        else:
            port = cfg.api_port or 8729
    else:
        available: list[tuple[str, int]] = []
        for _ in range(DETECT_RETRIES):
            available = detect_all_methods(cfg.ip, cfg.user, cfg.password)
            if available:
                break
        if not available:
            print(
                "Error [connect]: Failed to detect any deployment method",
                file=sys.stderr,
            )
            return 1
        method, port = available[0]

    ok, category, detail = _check_method_preflight(
        method,
        cfg.ip,
        cfg.user,
        cfg.password,
        port,
    )
    if not ok:
        print(f"Error [{category}]: {detail}", file=sys.stderr)
        return 1

    for _ in range(DEPLOY_RETRIES):
        if deploy_with_method(
            method,
            cfg.ip,
            cfg.user,
            cfg.password,
            port,
            "mikrotik-arm64",
            config_json,
        ):
            ui_msg("✅ Deployment complete")
            return 0

    print(
        f"Error [connect]: Deployment failed for method '{method}' on {cfg.ip}:{port}",
        file=sys.stderr,
    )
    return 1


def _check_method_preflight(
    method: str,
    ip: str,
    user: str,
    password: str,
    port: int,
) -> tuple[bool, str, str]:
    if method == "ssh":
        if not tcp_probe(ip, port):
            return (False, "connect", f"Cannot reach SSH service at {ip}:{port}")
        if not _ssh_try_auth(ip, user, password, port):
            return (False, "auth", f"SSH authentication failed for {user}@{ip}:{port}")
        return (True, "", "")

    if method == "rest":
        url = f"{_rest_base_url(ip, port)}/rest/system/resource"
        headers = _rest_headers(user, password)
        try:
            status, _ = http_request(
                url, method="GET", headers=headers, timeout=CONNECT_TIMEOUT
            )
        except TimeoutError:
            return (
                False,
                "timeout",
                f"Timed out connecting to REST API at {ip}:{port}",
            )
        except ConnectionError:
            return (False, "connect", f"Cannot reach REST API at {ip}:{port}")

        if status in (401, 403):
            return (False, "auth", f"REST authentication failed for {user}@{ip}:{port}")
        if status != 200:
            return (
                False,
                "connect",
                f"REST endpoint returned HTTP {status} at {ip}:{port}",
            )
        return (True, "", "")

    try:
        with _api_connect(ip, port) as sock:
            _api_send_sentence(
                sock, ["/login", f"=name={user}", f"=password={password}"]
            )
        return (True, "", "")
    except socket.timeout:
        return (False, "timeout", f"Timed out connecting to API service at {ip}:{port}")
    except OSError:
        return (False, "connect", f"Cannot reach API service at {ip}:{port}")


def install_routeros_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("RouterOS Installation")

    ip = ui_input("Router IP")
    user = ui_input("Router username", "admin")
    password = ui_input_secret("Router password")

    while True:
        ui_progress("Detecting available deployment methods")
        methods = detect_all_methods(ip, user, password)
        if methods:
            ui_done()
            break
        ui_error("No deployment methods detected")
        retry = ui_menu("Detection failed. What next?", "Retry detection", "Back")
        if retry != 1:
            return 1

    method, port = select_method_menu(methods)

    bot_token = ui_input("Telegram bot token")
    api_key = ui_input("LLM API key")
    provider = ui_input("LLM provider", "openrouter")
    if not is_valid_provider(provider):
        ui_error(f"Unsupported provider: {provider}")
        return 1
    base_url = ui_input("LLM base URL override", "")
    model = ui_input("LLM model override", "")

    config_json = config_create(bot_token, api_key, provider, base_url, model)

    while True:
        ui_progress(f"Deploying with {method.upper()} on port {port}")
        deployed = deploy_with_method(
            method,
            ip,
            user,
            password,
            port,
            "mikrotik-arm64",
            config_json,
        )
        if deployed:
            ui_done()
            ui_msg("✅ Installation Complete!")
            return 0
        ui_error("Deployment failed")
        retry = ui_menu("Deployment failed. What next?", "Retry deployment", "Back")
        if retry != 1:
            return 1


def install_linux() -> int:
    temp_dir = tempfile.mkdtemp(prefix="mikroclaw-linux-")
    try:
        ui_banner()
        ui_msg("Installing to Linux host...")
        binary_path = os.path.join(temp_dir, "mikroclaw")
        download_binary("linux-x64", binary_path)

        install_dir = "/usr/local/bin"
        install_path = os.path.join(install_dir, "mikroclaw")
        if os.access(install_dir, os.W_OK):
            shutil.copy(binary_path, install_path)
            ui_msg(f"Installed binary to {install_path}")
        else:
            ui_msg(
                f"Cannot write to {install_dir}. Install manually from {binary_path}"
            )
        return 0
    except Exception as exc:
        ui_error(f"Failed to install Linux target: {exc}")
        return 1
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def install_linux_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("Linux Installation")
    code = install_linux()
    if code == 0:
        ui_msg("✅ Installation Complete!")
        ui_msg("Run: mikroclaw --help")
    return code


def install_docker_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("Docker Installation")
    ui_msg("Docker support coming soon!")
    return 1


def main_interactive() -> int:
    ui_init()
    ui_clear()
    ui_banner()
    choice = ui_menu(
        "Choose installation target:",
        "RouterOS",
        "Linux",
        "Docker",
        "Exit",
    )
    if choice == 1:
        return install_routeros_interactive()
    if choice == 2:
        return install_linux_interactive()
    if choice == 3:
        return install_docker_interactive()
    return 0


def main(argv=None) -> int:
    try:
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

        if cfg.target is None:
            return main_interactive()
        if cfg.target == "routeros":
            return install_routeros_cli(cfg)
        if cfg.target == "linux":
            return install_linux()
        print("Error: Docker target not yet implemented", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted by user", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
