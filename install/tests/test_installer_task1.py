# pyright: basic
import json
import subprocess
import sys
from pathlib import Path


INSTALL_DIR = Path(__file__).resolve().parents[1]
SCRIPT_PATH = INSTALL_DIR / "mikroclaw-install.py"


def run_installer(*args):
    return subprocess.run(
        [sys.executable, str(SCRIPT_PATH), *args],
        capture_output=True,
        text=True,
    )


def run_module_expr(expr):
    code = (
        "import importlib.util, json;"
        f"p=r'{SCRIPT_PATH}';"
        "spec=importlib.util.spec_from_file_location('mikroclaw_install', p);"
        "mod=importlib.util.module_from_spec(spec);"
        "spec.loader.exec_module(mod);"
        f"{expr}"
    )
    return subprocess.run([sys.executable, "-c", code], capture_output=True, text=True)


def test_help_semantics_include_legacy_flags_and_new_ports():
    result = run_installer("--help")

    assert result.returncode == 0
    help_text = result.stdout + result.stderr
    assert "Usage:" in help_text or "usage:" in help_text
    assert "--target" in help_text
    assert "--ip" in help_text
    assert "--user" in help_text
    assert "--pass" in help_text
    assert "--method" in help_text
    assert "--provider" in help_text
    assert "--bot-token" in help_text
    assert "--api-key" in help_text
    assert "--base-url" in help_text
    assert "--model" in help_text
    assert "--ssh-port" in help_text
    assert "--api-port" in help_text
    assert "openrouter" in help_text


def test_invalid_provider_exits_1_with_clear_error():
    result = run_installer(
        "--target",
        "routeros",
        "--ip",
        "1.2.3.4",
        "--provider",
        "invalid-provider",
    )

    assert result.returncode == 1
    assert "Unsupported provider" in (result.stdout + result.stderr)


def test_invalid_ssh_port_values_exit_1():
    for value in ("0", "65536", "bad"):
        result = run_installer(
            "--target",
            "routeros",
            "--ip",
            "1.2.3.4",
            "--method",
            "ssh",
            "--ssh-port",
            value,
        )
        assert result.returncode == 1, (
            f"value={value} rc={result.returncode} stderr={result.stderr} stdout={result.stdout}"
        )


def test_invalid_api_port_values_exit_1():
    for value in ("0", "65536", "bad"):
        result = run_installer(
            "--target",
            "routeros",
            "--ip",
            "1.2.3.4",
            "--method",
            "api",
            "--api-port",
            value,
        )
        assert result.returncode == 1, (
            f"value={value} rc={result.returncode} stderr={result.stderr} stdout={result.stdout}"
        )


def test_config_create_provider_defaults_for_openrouter_and_auth_type_variants():
    result = run_module_expr(
        "print(mod.config_create('bot-token', 'api-key', 'openrouter'))"
    )
    assert result.returncode == 0, result.stderr
    config = json.loads(result.stdout)

    assert config["LLM_PROVIDER"] == "openrouter"
    assert config["LLM_BASE_URL"] == "https://openrouter.ai/api/v1"
    assert config["MODEL"] == "google/gemini-flash"
    assert config["AUTH_TYPE"] == "bearer"

    anthropic = run_module_expr(
        "print(mod.config_create('bot-token', 'api-key', 'anthropic'))"
    )
    assert anthropic.returncode == 0, anthropic.stderr
    anthropic_config = json.loads(anthropic.stdout)
    assert anthropic_config["AUTH_TYPE"] == "api_key"
    assert anthropic_config["MODEL"] == "claude-3-5-sonnet-latest"


def test_provider_validator_supports_current_provider_set():
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

    for provider in providers:
        result = run_module_expr(f"print(mod.is_valid_provider('{provider}'))")
        assert result.returncode == 0, result.stderr
        assert result.stdout.strip() == "True"

    invalid = run_module_expr("print(mod.is_valid_provider('not-a-provider'))")
    assert invalid.returncode == 0, invalid.stderr
    assert invalid.stdout.strip() == "False"
