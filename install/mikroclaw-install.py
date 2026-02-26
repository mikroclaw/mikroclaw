#!/usr/bin/env python3
# pyright: basic

import argparse
import base64
import getpass
import ipaddress
import json
import os
import re
import shlex
import shutil
import socket
import ssl
import stat
import subprocess
import sys
import tempfile
import urllib.request
import urllib.error
from dataclasses import dataclass
from typing import Any, Optional


INSTALLER_VERSION = "2025.02.25:BETA2"
BINARY_URL_TEMPLATE = "https://github.com/mikroclaw/mikroclaw/releases/latest/download/mikroclaw-{platform}"
CONNECT_TIMEOUT = 10
TRANSFER_TIMEOUT = 120


# =============================================================================
# Error Classes (Task 3)
# =============================================================================


class InstallerError(Exception):
    """Base error with user-friendly message."""

    pass


class ConnectionError(InstallerError):
    """Network connection failures."""

    pass


class AuthError(InstallerError):
    """Authentication failures."""

    pass


class TimeoutError(InstallerError):
    """Timeout failures."""

    pass


# =============================================================================
# Network Helpers (Task 3)
# =============================================================================


def tcp_probe(host: str, port: int, timeout: int = 2) -> bool:
    """Test TCP connectivity using socket.

    Args:
        host: Target host IP address
        port: Target port number
        timeout: Connection timeout in seconds

    Returns:
        True if connection succeeds, False otherwise
    """
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((host, port))
        return result == 0
    except (socket.error, OSError):
        return False
    finally:
        if sock is not None:
            sock.close()


def http_request(
    url: str,
    method: str = "GET",
    headers: Optional[dict] = None,
    data: Optional[bytes] = None,
    timeout: int = 10,
) -> tuple:
    """Make HTTP request with timeout and error classification.

    Args:
        url: Full URL to request
        method: HTTP method (GET, POST, PUT)
        headers: Optional dict of headers
        data: Optional request body bytes
        timeout: Request timeout in seconds

    Returns:
        Tuple of (status_code, response_body)

    Raises:
        ValueError: If method is not supported
        TimeoutError: If connection times out
        ConnectionError: If connection is refused or fails
        AuthError: If authentication fails (401)
    """
    if method not in ("GET", "POST", "PUT"):
        raise ValueError(f"Unsupported HTTP method: {method}")

    req_headers = headers.copy() if headers else {}

    try:
        req = urllib.request.Request(url, data=data, headers=req_headers, method=method)

        # Create SSL context that doesn't verify certs (for self-signed RouterOS certs)
        ssl_context = ssl._create_unverified_context()

        with urllib.request.urlopen(
            req, timeout=timeout, context=ssl_context
        ) as response:
            body = response.read()
            return (response.status, body)

    except urllib.error.HTTPError as e:
        if e.code == 401:
            raise AuthError("Authentication failed. Verify router username/password.")
        return (e.code, b"")
    except urllib.error.URLError as e:
        reason = str(e.reason).lower()
        if "timeout" in reason or "timed out" in reason:
            raise TimeoutError(f"Request timed out after {timeout} seconds")
        elif "refused" in reason or "connection refused" in reason:
            raise ConnectionError("Connection refused. Service may not be enabled.")
        elif "no route" in reason or "unreachable" in reason:
            raise ConnectionError("Network unreachable. Verify router IP and routing.")
        else:
            raise ConnectionError(f"Connection failed: {e.reason}")
    except socket.timeout:
        raise TimeoutError(f"Request timed out after {timeout} seconds")
    except socket.error as e:
        raise ConnectionError(f"Connection failed: {e}")


def _ssh_command_base(ip: str, user: str, port: int) -> list:
    return [
        "ssh",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "ConnectTimeout=5",
        "-p",
        str(port),
        f"{user}@{ip}",
        ':put "OK"',
    ]


def _ssh_try_auth(ip: str, user: str, password: str, port: int) -> bool:
    safe_password = shlex.quote(password)
    if safe_password and shutil.which("sshpass"):
        cmd = ["sshpass", "-p", password, *_ssh_command_base(ip, user, port)]
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=7,
        )
        if result.returncode == 0 and "OK" in result.stdout:
            return True

    key_cmd = [
        "ssh",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "ConnectTimeout=5",
        "-o",
        "BatchMode=yes",
        "-p",
        str(port),
        f"{user}@{ip}",
        ':put "OK"',
    ]
    key_result = subprocess.run(
        key_cmd,
        capture_output=True,
        text=True,
        timeout=7,
    )
    return key_result.returncode == 0 and "OK" in key_result.stdout


def method_ssh_test(ip: str, user: str, password: str, port: int = 22) -> bool:
    if not tcp_probe(ip, port, timeout=2):
        return False

    try:
        return _ssh_try_auth(ip, user, password, port)
    except subprocess.TimeoutExpired:
        return False
    except OSError:
        return False


def method_ssh_detect(ip: str, user: str, password: str):
    for port in (22, 2222, 8022):
        if method_ssh_test(ip, user, password, port):
            return ("ssh", port)
    return None


def _rest_auth_header(user: str, password: str) -> str:
    token = f"{user}:{password}".encode("utf-8")
    encoded = base64.b64encode(token).decode("ascii")
    return f"Basic {encoded}"


def _rest_base_url(ip: str, port: int) -> str:
    protocol = "http" if port == 80 else "https"
    return f"{protocol}://{ip}:{port}"


def method_rest_test(ip: str, user: str, password: str, port: int = 443) -> bool:
    url = f"{_rest_base_url(ip, port)}/rest/system/resource"
    headers = {
        "Authorization": _rest_auth_header(user, password),
        "Accept": "application/json",
    }
    try:
        status, _ = http_request(
            url,
            method="GET",
            headers=headers,
            timeout=CONNECT_TIMEOUT,
        )
    except (AuthError, ConnectionError, TimeoutError, OSError):
        return False
    return status == 200


def method_rest_detect(ip: str, user: str, password: str):
    for port in (443, 80):
        if method_rest_test(ip, user, password, port):
            return ("rest", port)
    return None


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
    auth_header = _rest_auth_header(user, password)
    common_headers = {
        "Authorization": auth_header,
        "Accept": "application/json",
    }

    try:
        fetch_body = json.dumps(
            {
                "url": binary_url,
                "dst-path": "disk1/mikroclaw",
                "mode": "https",
            }
        ).encode("utf-8")
        fetch_headers = dict(common_headers)
        fetch_headers["Content-Type"] = "application/json"
        fetch_code, _ = http_request(
            f"{base_url}/rest/tool/fetch",
            method="POST",
            headers=fetch_headers,
            data=fetch_body,
            timeout=CONNECT_TIMEOUT,
        )
        if fetch_code != 200:
            return False

        file_body = json.dumps(
            {
                "name": "disk1/mikroclaw.env.json",
                "content": config_json,
            }
        ).encode("utf-8")
        file_headers = dict(common_headers)
        file_headers["Content-Type"] = "application/json"
        file_code, _ = http_request(
            f"{base_url}/rest/file",
            method="PUT",
            headers=file_headers,
            data=file_body,
            timeout=CONNECT_TIMEOUT,
        )

        if file_code != 200:
            script_cmd = (
                "/file print file=mikroclaw.env.json; "
                f"/file set mikroclaw.env.json content={config_json}"
            )
            script_body = json.dumps({"script": script_cmd}).encode("utf-8")
            script_headers = dict(common_headers)
            script_headers["Content-Type"] = "application/json"
            script_code, _ = http_request(
                f"{base_url}/rest/system/script/run",
                method="POST",
                headers=script_headers,
                data=script_body,
                timeout=CONNECT_TIMEOUT,
            )
            if script_code != 200:
                return False

        list_code, list_body = http_request(
            f"{base_url}/rest/file",
            method="GET",
            headers=common_headers,
            timeout=CONNECT_TIMEOUT,
        )
        if list_code != 200:
            return False

        files = json.loads(list_body.decode("utf-8"))
        for item in files:
            if (
                isinstance(item, dict)
                and item.get("name") == "disk1/mikroclaw.env.json"
            ):
                return True
        return False
    except (
        AuthError,
        ConnectionError,
        TimeoutError,
        OSError,
        ValueError,
        json.JSONDecodeError,
    ):
        return False


def _api_encode_length(length: int) -> bytes:
    if length < 0:
        raise ValueError("length must be non-negative")
    if length < 0x80:
        return bytes([length])
    if length < 0x4000:
        value = 0x8000 | length
        return bytes([(value >> 8) & 0xFF, value & 0xFF])
    if length < 0x200000:
        value = 0xC00000 | length
        return bytes([(value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF])
    raise ValueError("length out of supported range")


def _api_send_sentence(sock_obj, words) -> None:
    payload = bytearray()
    for word in words:
        encoded = word.encode("utf-8")
        payload.extend(_api_encode_length(len(encoded)))
        payload.extend(encoded)
    payload.append(0)
    sock_obj.sendall(bytes(payload))


def method_api_test(ip: str, port: int = 8728) -> bool:
    return tcp_probe(ip, port, timeout=2)


def method_api_detect(ip: str, user: str, password: str):
    _ = (user, password)
    for port in (8729, 8728):
        if method_api_test(ip, port):
            return ("api", port)
    return None


def _api_wrap_socket_for_ssl(sock_obj, ip: str):
    wrap_socket = getattr(ssl, "wrap_socket", None)
    if callable(wrap_socket):
        return wrap_socket(sock_obj)
    context = ssl._create_unverified_context()
    return context.wrap_socket(sock_obj, server_hostname=ip)


def method_api_deploy(
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    sock_obj: Any = None
    try:
        sock_obj = socket.create_connection((ip, port), timeout=CONNECT_TIMEOUT)
        if port == 8729:
            sock_obj = _api_wrap_socket_for_ssl(sock_obj, ip)

        _api_send_sentence(
            sock_obj, ["!login", f"=name={user}", f"=password={password}"]
        )

        fetch_script = (
            f'/tool fetch url="{binary_url}" dst-path="disk1/mikroclaw" mode=https'
        )
        _api_send_sentence(sock_obj, ["!exec", f"=command={fetch_script}"])

        escaped_config = config_json.replace('"', '\\"').replace("\n", " ")
        config_script = (
            "/file print file=disk1/mikroclaw.env.json; "
            f'/file set mikroclaw.env.json content="{escaped_config}"'
        )
        _api_send_sentence(sock_obj, ["!exec", f"=command={config_script}"])
        return True
    except (OSError, socket.error, ssl.SSLError, ValueError):
        return False
    finally:
        if sock_obj is not None:
            try:
                sock_obj.close()
            except OSError:
                pass


def _download_to_file(binary_url: str, output_path: str):
    with urllib.request.urlopen(binary_url, timeout=TRANSFER_TIMEOUT) as response:
        data = response.read()
    with open(output_path, "wb") as handle:
        handle.write(data)
    current_mode = os.stat(output_path).st_mode
    os.chmod(output_path, current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def _scp_upload(
    ip: str, user: str, password: str, port: int, local_path: str, remote_path: str
) -> bool:
    base_cmd = [
        "scp",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "ConnectTimeout=10",
        "-P",
        str(port),
        local_path,
        f"{user}@{ip}:{remote_path}",
    ]

    safe_password = shlex.quote(password)
    if safe_password and shutil.which("sshpass"):
        cmd = ["sshpass", "-p", password, *base_cmd]
    else:
        cmd = [
            "scp",
            "-o",
            "StrictHostKeyChecking=no",
            "-o",
            "ConnectTimeout=10",
            "-o",
            "BatchMode=yes",
            "-P",
            str(port),
            local_path,
            f"{user}@{ip}:{remote_path}",
        ]

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=TRANSFER_TIMEOUT,
    )
    return result.returncode == 0


def method_ssh_deploy(
    ip: str,
    user: str,
    password: str,
    port: int,
    binary_url: str,
    config_json: str,
) -> bool:
    if not method_ssh_test(ip, user, password, port):
        return False

    tmpdir = tempfile.mkdtemp()
    try:
        binary_path = os.path.join(tmpdir, "mikroclaw")
        config_path = os.path.join(tmpdir, "mikroclaw.env.json")

        _download_to_file(binary_url, binary_path)

        with open(config_path, "w", encoding="utf-8") as config_file:
            config_file.write(config_json)

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
    except (OSError, urllib.error.URLError):
        return False
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def detect_all_methods(ip: str, user: str, password: str) -> list:
    """Detect all available connection methods in shell parity order.



    Detection order: SSH -> REST -> Binary API

    Returns list of (method, port) tuples.

    """

    available = []

    # Try SSH first (ports 22, 2222, 8022)

    ssh_result = method_ssh_detect(ip, user, password)

    if ssh_result:
        available.append(ssh_result)

    # Try REST API (ports 443, 80)

    rest_result = method_rest_detect(ip, user, password)

    if rest_result:
        available.append(rest_result)

    # Try Binary API (ports 8729, 8728)

    api_result = method_api_detect(ip, user, password)

    if api_result:
        available.append(api_result)

    return available


def select_method_menu(available_methods: list) -> tuple:
    """Present TUI menu with detected methods, return chosen (method, port).



    If only one method detected, auto-select (skip menu).

    Returns tuple of (method_name, port_number).

    """

    if not available_methods:
        raise ValueError("No available methods to select from")

    # Auto-select if only one method available

    if len(available_methods) == 1:
        return available_methods[0]

    # Build menu labels matching shell parity

    menu_options = []

    for method, port in available_methods:
        if method == "ssh":
            menu_options.append(f"SSH/SCP (ssh:{port})")

        elif method == "rest":
            if port == 443:
                menu_options.append(
                    f"REST API + HTTPS fetch (https:{port}) [recommended]"
                )

            else:
                menu_options.append(f"REST API + HTTP fetch (http:{port})")

        elif method == "api":
            if port == 8729:
                menu_options.append(f"MikroTik Binary API SSL (api-ssl:{port})")

            else:
                menu_options.append(f"MikroTik Binary API (api:{port})")

        else:
            menu_options.append(f"{method} (port {port})")

    # Show menu and get selection

    selection = ui_menu("Select Connection Method", *menu_options)

    # Validate selection

    if selection < 1 or selection > len(available_methods):
        # Invalid selection - return first available (shell parity behavior)

        return available_methods[0]

    return available_methods[selection - 1]


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



    Args:

        method: Deployment method ('ssh', 'rest', or 'api')

        ip: Router IP address

        user: Router username

        password: Router password

        port: Port number for the method

        binary_url: URL to download the binary from

        config_json: JSON configuration string



    Returns:

        True if deployment succeeded, False otherwise



    Raises:

        ValueError: If method is not supported

    """

    method_lower = method.lower()

    if method_lower == "ssh":
        return method_ssh_deploy(ip, user, password, port, binary_url, config_json)

    elif method_lower == "rest":
        return method_rest_deploy(ip, user, password, port, binary_url, config_json)

    elif method_lower == "api":
        return method_api_deploy(ip, user, password, port, binary_url, config_json)

    else:
        raise ValueError(f"Unknown deployment method: {method}")


PROVIDER_DEFAULTS = {
    "openrouter": ("https://openrouter.ai/api/v1", "google/gemini-flash", "bearer"),
    "openai": ("https://api.openai.com/v1", "gpt-4o", "bearer"),
    "ollama": ("http://127.0.0.1:11434/v1", "llama3", "bearer"),
    "anthropic": (
        "https://api.anthropic.com/v1",
        "claude-3-5-sonnet-latest",
        "api_key",
    ),
    "groq": ("https://api.groq.com/openai", "llama3-70b-8192", "bearer"),
    "mistral": ("https://api.mistral.ai", "mistral-large-latest", "bearer"),
    "xai": ("https://api.x.ai/v1", "grok-beta", "bearer"),
    "deepseek": ("https://api.deepseek.com", "deepseek-chat", "bearer"),
    "together": ("https://api.together.xyz", "meta-llama/Llama-3-70b", "bearer"),
    "fireworks": (
        "https://api.fireworks.ai/inference",
        "accounts/fireworks/models/llama-v3-70b",
        "bearer",
    ),
    "perplexity": ("https://api.perplexity.ai", "sonar-medium-online", "bearer"),
    "cohere": ("https://api.cohere.com/compatibility", "command-r", "api_key"),
    "bedrock": (
        "https://bedrock-runtime.us-east-1.amazonaws.com",
        "anthropic.claude-3-sonnet",
        "api_key",
    ),
    "kimi": ("https://api.moonshot.cn/v1", "moonshot-v1-8k", "bearer"),
    "minimax": ("https://api.minimax.chat/v1", "abab6.5-chat", "api_key"),
    "zai": ("https://api.z.ai/v1", "zai-latest", "bearer"),
    "synthetic": ("https://api.synthetic.new/v1", "synthetic-large", "bearer"),
    "localai": ("http://127.0.0.1:11434/v1", "llama3", "bearer"),
}

VALID_PROVIDERS = [
    "openrouter",
    "openai",
    "anthropic",
    "ollama",
    "groq",
    "mistral",
    "xai",
    "deepseek",
    "together",
    "fireworks",
    "perplexity",
    "cohere",
    "bedrock",
    "kimi",
    "minimax",
    "zai",
    "synthetic",
]


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
    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(1, f"Error: {message}\n")


def port_value(raw):
    try:
        value = int(raw)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "must be an integer in range 1..65535"
        ) from exc
    if value < 1 or value > 65535:
        raise argparse.ArgumentTypeError("must be an integer in range 1..65535")
    return value


def is_valid_ipv4(value):
    try:
        parsed = ipaddress.ip_address(value)
    except ValueError:
        return False
    return parsed.version == 4


def is_valid_user(value):
    return bool(value) and re.fullmatch(r"[A-Za-z0-9._-]+", value) is not None


def is_valid_provider(value):
    return value in VALID_PROVIDERS


def config_create(bot_token, api_key, provider="openrouter", base_url="", model=""):
    provider_key = provider
    if provider_key not in PROVIDER_DEFAULTS:
        provider_key = "openrouter"

    default_url, default_model, auth_type = PROVIDER_DEFAULTS[provider_key]
    config = {
        "BOT_TOKEN": bot_token,
        "LLM_API_KEY": api_key,
        "LLM_PROVIDER": provider,
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
    return json.dumps(config, indent=2)


def download_binary(platform, output_path):
    url = BINARY_URL_TEMPLATE.format(platform=platform)
    with urllib.request.urlopen(url, timeout=TRANSFER_TIMEOUT) as response:
        data = response.read()
    with open(output_path, "wb") as handle:
        handle.write(data)
    current_mode = os.stat(output_path).st_mode
    os.chmod(output_path, current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def build_parser():
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


def parse_args(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
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


def _normalize_failure_category(message: str) -> str:
    lowered = (message or "").lower()
    if "auth" in lowered or "unauthorized" in lowered or "permission" in lowered:
        return "auth"
    if "timeout" in lowered or "timed out" in lowered:
        return "timeout"
    return "connect"


def install_routeros_cli(args: InstallerConfig) -> int:
    if not args.ip:
        print("Error: --ip required for routeros target", file=sys.stderr)
        return 1

    if args.method and args.method not in ("ssh", "rest", "api"):
        print(
            f"Error: Unknown method '{args.method}'. Use: ssh, rest, or api",
            file=sys.stderr,
        )
        return 1

    config_json = config_create(
        args.bot_token,
        args.api_key,
        args.provider,
        args.base_url,
        args.model,
    )
    binary_url = BINARY_URL_TEMPLATE.format(platform="linux-x64")

    method_name = ""
    port = 0
    if args.method == "ssh":
        method_name = "ssh"
        port = args.ssh_port or 22
    elif args.method == "rest":
        detected = method_rest_detect(args.ip, args.user, args.password)
        if not detected:
            print(
                "Error [connect]: REST API not available on target",
                file=sys.stderr,
            )
            return 1
        method_name, port = detected
    elif args.method == "api":
        if args.api_port:
            method_name = "api"
            port = args.api_port
        else:
            detected = method_api_detect(args.ip, args.user, args.password)
            if not detected:
                print(
                    "Error [connect]: Binary API not available on target",
                    file=sys.stderr,
                )
                return 1
            method_name, port = detected
    else:
        available = detect_all_methods(args.ip, args.user, args.password)
        if not available:
            print(
                "Error [connect]: No connection methods detected on target",
                file=sys.stderr,
            )
            return 1
        method_name, port = available[0]

    if deploy_with_method(
        method_name,
        args.ip,
        args.user,
        args.password,
        port,
        binary_url,
        config_json,
    ):
        print("✅ Installation Complete!")
        return 0

    category = _normalize_failure_category(f"{method_name} deploy failed")
    print(
        f"Error [{category}]: deployment failed via {method_name}:{port}",
        file=sys.stderr,
    )
    return 1


def install_linux() -> int:
    ui_banner()
    ui_msg("Installing MikroClaw to Linux")
    ui_msg("")

    tmpdir = tempfile.mkdtemp()
    try:
        binary = os.path.join(tmpdir, "mikroclaw")
        ui_progress("Downloading MikroClaw for linux-x64")
        download_binary("linux-x64", binary)
        ui_done()

        if os.access("/usr/local/bin", os.W_OK):
            shutil.copy(binary, "/usr/local/bin/mikroclaw")
            ui_msg("✓ Installed to /usr/local/bin/mikroclaw")
        else:
            ui_msg("⚠ Cannot write to /usr/local/bin")
            ui_msg(f"  Copy manually: sudo cp {binary} /usr/local/bin/")
        return 0
    except Exception as exc:
        ui_error(f"Failed to install Linux target: {exc}")
        return 1
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def install_linux_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("Linux Installation")
    ui_msg("")

    rc = install_linux()
    if rc == 0:
        ui_msg("")
        ui_msg("✅ Installation Complete!")
        ui_msg("Run: mikroclaw --help")
    return rc


def install_docker_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("Docker Installation")
    ui_msg("")
    ui_msg("Docker support coming soon!")
    return 1


def install_routeros_interactive() -> int:
    ui_clear()
    ui_banner()
    ui_msg("RouterOS Installation")
    ui_msg("")

    router_ip = ui_input("Router IP address")
    router_user = ui_input("Router username", "admin")
    router_pass = ui_input_secret("Router password")

    while True:
        methods = detect_all_methods(router_ip, router_user, router_pass)
        if methods:
            break
        ui_error(f"No connection methods detected on {router_ip}")
        choice = ui_menu(
            "Connection Failed",
            "Retry detection",
            "Re-enter router credentials",
            "Cancel installation",
        )
        if choice == 1:
            continue
        if choice == 2:
            router_ip = ui_input("Router IP address", router_ip)
            router_user = ui_input("Router username", router_user)
            router_pass = ui_input_secret("Router password")
            continue
        return 1

    selected_method, selected_port = select_method_menu(methods)

    ui_msg("")
    bot_token = ui_input("Telegram Bot Token")
    provider_choice = ui_menu(
        "Select LLM Provider",
        "OpenRouter (recommended)",
        "OpenAI",
        "Anthropic",
        "Ollama/LocalAI",
        "Groq",
        "Mistral",
        "XAI (Grok)",
        "DeepSeek",
        "Together",
        "Fireworks",
        "Perplexity",
        "Cohere",
        "Bedrock (AWS)",
        "Kimi (Moonshot)",
        "MiniMax",
        "Z.AI",
        "Synthetic.New",
    )
    providers = [
        "openrouter",
        "openai",
        "anthropic",
        "ollama",
        "groq",
        "mistral",
        "xai",
        "deepseek",
        "together",
        "fireworks",
        "perplexity",
        "cohere",
        "bedrock",
        "kimi",
        "minimax",
        "zai",
        "synthetic",
    ]
    if provider_choice < 1 or provider_choice > len(providers):
        provider = "openrouter"
    else:
        provider = providers[provider_choice - 1]

    api_key = ui_input("LLM API Key")
    tg_allowlist = ui_input(
        "Telegram allowlist (comma-separated, * for all, empty deny-all)"
    )
    dc_allowlist = ui_input(
        "Discord allowlist (comma-separated, * for all, empty deny-all)"
    )
    sk_allowlist = ui_input(
        "Slack allowlist (comma-separated, * for all, empty deny-all)"
    )

    config_json = config_create(bot_token, api_key, provider)
    parsed_config = json.loads(config_json)
    parsed_config["allowlists"]["telegram"] = tg_allowlist
    parsed_config["allowlists"]["discord"] = dc_allowlist
    parsed_config["allowlists"]["slack"] = sk_allowlist
    config_json = json.dumps(parsed_config, indent=2)

    binary_url = BINARY_URL_TEMPLATE.format(platform="linux-x64")
    while True:
        if deploy_with_method(
            selected_method,
            router_ip,
            router_user,
            router_pass,
            selected_port,
            binary_url,
            config_json,
        ):
            ui_msg("")
            ui_msg("✅ Installation Complete!")
            return 0

        ui_error("Installation failed")
        choice = ui_menu(
            "Installation Failed",
            "Retry install",
            "Re-enter router credentials",
            "Select different method",
            "Cancel installation",
        )
        if choice == 1:
            continue
        if choice == 2:
            router_ip = ui_input("Router IP address", router_ip)
            router_user = ui_input("Router username", router_user)
            router_pass = ui_input_secret("Router password")
            redetected = detect_all_methods(router_ip, router_user, router_pass)
            if redetected:
                selected_method, selected_port = select_method_menu(redetected)
            continue
        if choice == 3:
            selected_method, selected_port = select_method_menu(methods)
            continue
        ui_msg("Installation cancelled")
        return 1


def main_interactive() -> int:
    ui_init()
    ui_clear()
    ui_banner()

    target = ui_menu(
        "Select Install Target",
        "RouterOS (MikroTik router)",
        "Linux (standalone binary)",
        "Docker (container)",
        "Exit",
    )
    if target == 1:
        return install_routeros_interactive()
    if target == 2:
        return install_linux_interactive()
    if target == 3:
        return install_docker_interactive()
    if target == 4:
        return 0
    ui_error("Invalid selection")
    return 1


def main(argv=None):
    try:
        args = parse_args(argv)
        if args.target is None:
            return main_interactive()

        if not is_valid_provider(args.provider):
            valid = ", ".join(VALID_PROVIDERS)
            print(f"Error: Unsupported provider '{args.provider}'", file=sys.stderr)
            print(f"Valid providers: {valid}", file=sys.stderr)
            return 1

        if not is_valid_user(args.user):
            print("Error: Invalid router username", file=sys.stderr)
            return 1

        if args.target == "routeros":
            return install_routeros_cli(args)
        if args.target == "linux":
            return install_linux()
        if args.target == "docker":
            print(
                "Error [connect]: docker target is not yet implemented in CLI mode",
                file=sys.stderr,
            )
            return 1

        print(f"Error: Unknown target '{args.target}'", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted", file=sys.stderr)
        return 130


# =============================================================================
# UI Functions (Task 2)
# =============================================================================


def ui_menu(title, *options):
    """Present numbered menu and return 1-indexed selection."""
    print("", file=sys.stderr)
    print("═══════════════════════════════════════════", file=sys.stderr)
    print(f"  {title}", file=sys.stderr)
    print("═══════════════════════════════════════════", file=sys.stderr)
    print("", file=sys.stderr)
    for i, option in enumerate(options, 1):
        print(f"   [{i}] {option}", file=sys.stderr)
    print("", file=sys.stderr)
    print("➤ Select option: ", file=sys.stderr, end="")
    try:
        selection = input()
    except EOFError:
        selection = ""
    try:
        return int(selection)
    except ValueError:
        return 0


def ui_input(prompt, default=None):
    """Text input with optional default value."""
    if default:
        print(f"❓ {prompt} [{default}]: ", file=sys.stderr, end="")
    else:
        print(f"❓ {prompt}: ", file=sys.stderr, end="")
    try:
        value = input()
    except EOFError:
        value = ""
    if not value and default:
        return default
    return value


def ui_input_secret(prompt):
    """Password input using getpass (no echo)."""
    return getpass.getpass(f"🔑 {prompt}: ")


def ui_progress(msg):
    """Print progress indicator to stderr."""
    print(f"⏳ {msg}...", file=sys.stderr, end="")


def ui_done():
    """Print completion indicator to stderr."""
    print(" ✅", file=sys.stderr)


def ui_error(msg):
    """Print error message to stderr."""
    print(f" ❌ {msg}", file=sys.stderr)


def ui_msg(msg):
    """Print message with leading spaces to stderr."""
    print(f"  {msg}", file=sys.stderr)


def ui_banner():
    """Print ASCII banner matching shell version."""
    print("╔══════════════════════════════════════════╗", file=sys.stderr)
    print("║   MikroClaw Installer 2025.02.25:BETA2  ║", file=sys.stderr)
    print("║     AI Agent for MikroTik RouterOS      ║", file=sys.stderr)
    print("╚══════════════════════════════════════════╝", file=sys.stderr)
    print("", file=sys.stderr)


def ui_init():
    return None


def ui_clear():
    """Clear terminal screen."""
    subprocess.run(
        ["sh", "-c", "clear 2>/dev/null || true"],
        capture_output=True,
        text=True,
        timeout=2,
    )


if __name__ == "__main__":
    raise SystemExit(main())
