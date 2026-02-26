"""Test suite for mikroclaw-install.py"""

import argparse
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

# Load module with hyphenated name using importlib
spec = importlib.util.spec_from_file_location(
    "mikroclaw_install", Path(__file__).parent.parent / "mikroclaw-install.py"
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class TestConfigCreate:
    """Tests for config_create function."""

    def test_config_create_produces_valid_json(self):
        """Verify JSON output has expected fields."""
        result = mod.config_create(
            bot_token="test_token_123",
            api_key="test_api_key_456",
            provider="openrouter",
        )

        # Parse the JSON
        config = json.loads(result)

        # Verify all expected fields are present
        assert "BOT_TOKEN" in config
        assert "LLM_API_KEY" in config
        assert "LLM_PROVIDER" in config
        assert "LLM_BASE_URL" in config
        assert "MODEL" in config
        assert "AUTH_TYPE" in config
        assert "allowlists" in config
        assert "memory" in config

        # Verify values
        assert config["BOT_TOKEN"] == "test_token_123"
        assert config["LLM_API_KEY"] == "test_api_key_456"
        assert config["LLM_PROVIDER"] == "openrouter"
        assert config["AUTH_TYPE"] == "bearer"


class TestIsValidIpv4:
    """Tests for is_valid_ipv4 function."""

    def test_is_valid_ipv4_with_valid_ips(self):
        """Test valid IP addresses."""
        valid_ips = [
            "192.168.1.1",
            "10.0.0.1",
            "255.255.255.255",
            "0.0.0.0",
            "172.16.0.1",
            "127.0.0.1",
        ]

        for ip in valid_ips:
            assert mod.is_valid_ipv4(ip) is True, f"Expected {ip} to be valid"

    def test_is_valid_ipv4_with_invalid_ips(self):
        """Test invalid IP addresses."""
        invalid_ips = [
            "256.1.1.1",
            "192.168.1",
            "192.168.1.1.1",
            "not-an-ip",
            "",
            "192.168.1.256",
            "::1",
            "2001:db8::1",
        ]

        for ip in invalid_ips:
            assert mod.is_valid_ipv4(ip) is False, f"Expected {ip} to be invalid"


class TestIsValidProvider:
    """Tests for is_valid_provider function."""

    def test_is_valid_provider_with_all_17_providers(self):
        """Test all 17 provider names are valid."""
        providers = [
            "openrouter",
            "openai",
            "anthropic",
            "google",
            "azure",
            "groq",
            "together",
            "replicate",
            "mistral",
            "cohere",
            "ai21",
            "fireworks",
            "vertex",
            "bedrock",
            "cloudflare",
            "deepseek",
            "ollama",
        ]

        for provider in providers:
            assert mod.is_valid_provider(provider) is True, (
                f"Expected {provider} to be valid"
            )

    def test_is_valid_provider_rejects_invalid(self):
        """Test invalid provider is rejected."""
        invalid_providers = [
            "invalid_provider",
            "openai-",
            "OPENROUTER",
            "",
            "unknown",
        ]

        for provider in invalid_providers:
            assert mod.is_valid_provider(provider) is False, (
                f"Expected {provider} to be invalid"
            )


class TestApiEncodeLength:
    """Tests for _api_encode_length function."""

    def test_api_encode_length_1_byte(self):
        """Test length < 128 encodes as single byte."""
        # Test various lengths under 128
        assert mod._api_encode_length(0) == bytes([0])
        assert mod._api_encode_length(1) == bytes([1])
        assert mod._api_encode_length(127) == bytes([127])
        assert mod._api_encode_length(64) == bytes([64])

    def test_api_encode_length_2_byte(self):
        """Test length 128-16383 encodes as 2 bytes."""
        # Test boundary values
        result = mod._api_encode_length(128)
        assert len(result) == 2
        assert result[0] & 0x80 != 0  # High bit set

        result = mod._api_encode_length(16383)
        assert len(result) == 2

        result = mod._api_encode_length(500)
        assert len(result) == 2

    def test_api_encode_length_3_byte(self):
        """Test length > 16383 encodes as 3 bytes."""
        result = mod._api_encode_length(16384)
        assert len(result) == 3

        result = mod._api_encode_length(100000)
        assert len(result) == 3

        result = mod._api_encode_length(0x1FFFFF)  # Max 3-byte value
        assert len(result) == 3


class TestTcpProbe:
    """Tests for tcp_probe function."""

    def test_tcp_probe_returns_false_on_closed_port(self):
        """Test against closed port on localhost."""
        # Port 1 is typically not used and should be closed
        result = mod.tcp_probe("127.0.0.1", 1, timeout=1)
        assert result is False


class TestMethodDetection:
    """Tests for method detection functions - all should return quickly on localhost."""

    def test_method_ssh_test_returns_false_on_localhost(self):
        """SSH test should return False quickly on localhost (no hang)."""
        # Should not hang and should return False (no SSH on localhost:22 likely)
        result = mod.method_ssh_test("127.0.0.1", "admin", "", port=22222)
        assert result is False

    def test_method_rest_test_returns_false_on_localhost(self):
        """REST test should return False quickly on localhost (no hang)."""
        # Should not hang and should return False (no REST API on localhost)
        result = mod.method_rest_test("127.0.0.1", "admin", "", port=65443)
        assert result is False

    def test_method_api_test_returns_false_on_localhost(self):
        """API test should return False quickly on localhost (no hang)."""
        # Should not hang and should return False (no API on localhost)
        result = mod.method_api_test("127.0.0.1", port=58728)
        assert result is False

    def test_detect_all_methods_returns_empty_on_localhost(self):
        """Detect all methods should return empty list on localhost (no hang)."""
        # Should not hang and should return empty list
        result = mod.detect_all_methods("127.0.0.1", "admin", "")
        assert result == []


class TestCLI:
    """Tests for CLI behavior."""

    def test_cli_help_output_contains_expected_options(self):
        """Verify help text contains expected options."""
        installer_path = Path(__file__).parent.parent / "mikroclaw-install.py"

        result = subprocess.run(
            [sys.executable, str(installer_path), "--help"],
            capture_output=True,
            text=True,
        )

        assert result.returncode == 0
        help_text = result.stdout

        # Check for expected options
        assert "--target" in help_text
        assert "--ip" in help_text
        assert "--user" in help_text
        assert "--pass" in help_text
        assert "--method" in help_text
        assert "--provider" in help_text
        assert "--bot-token" in help_text
        assert "--api-key" in help_text
        assert "--verbose" in help_text

    def test_invalid_provider_exits_with_error(self):
        """Invalid provider should exit with code 1."""
        installer_path = Path(__file__).parent.parent / "mikroclaw-install.py"

        result = subprocess.run(
            [
                sys.executable,
                str(installer_path),
                "--target",
                "routeros",
                "--ip",
                "192.168.1.1",
                "--provider",
                "invalid_provider_xyz",
            ],
            capture_output=True,
            text=True,
        )

        assert result.returncode == 1
        assert "Unsupported provider" in result.stderr or "Error" in result.stderr
