#!/usr/bin/env python3
# pyright: basic

import importlib.util
import os
import subprocess

_spec = importlib.util.spec_from_file_location(
    "mikroclaw_install",
    os.path.join(os.path.dirname(__file__), "..", "mikroclaw-install.py"),
)
assert _spec is not None
assert _spec.loader is not None
mikroclaw_install = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mikroclaw_install)


def test_detect_all_methods_returns_empty_list_when_no_methods_available(monkeypatch):
    """Test that detect_all_methods returns [] when no methods are detected."""
    monkeypatch.setattr(
        mikroclaw_install, "method_ssh_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_api_detect", lambda _ip, _user, _password: None
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    assert result == []


def test_detect_all_methods_detects_ssh_first(monkeypatch):
    """Test that detect_all_methods detects SSH when available."""
    monkeypatch.setattr(
        mikroclaw_install,
        "method_ssh_detect",
        lambda _ip, _user, _password: ("ssh", 22),
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_api_detect", lambda _ip, _user, _password: None
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    assert result == [("ssh", 22)]


def test_detect_all_methods_detects_rest_api(monkeypatch):
    """Test that detect_all_methods detects REST API when available."""
    monkeypatch.setattr(
        mikroclaw_install, "method_ssh_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install,
        "method_rest_detect",
        lambda _ip, _user, _password: ("rest", 443),
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_api_detect", lambda _ip, _user, _password: None
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    assert result == [("rest", 443)]


def test_detect_all_methods_detects_binary_api(monkeypatch):
    """Test that detect_all_methods detects Binary API when available."""
    monkeypatch.setattr(
        mikroclaw_install, "method_ssh_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_detect", lambda _ip, _user, _password: None
    )
    monkeypatch.setattr(
        mikroclaw_install,
        "method_api_detect",
        lambda _ip, _user, _password: ("api", 8728),
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    assert result == [("api", 8728)]


def test_detect_all_methods_returns_methods_in_parity_order(monkeypatch):
    """Test that detect_all_methods returns methods in SSH -> REST -> API order."""
    monkeypatch.setattr(
        mikroclaw_install,
        "method_ssh_detect",
        lambda _ip, _user, _password: ("ssh", 22),
    )
    monkeypatch.setattr(
        mikroclaw_install,
        "method_rest_detect",
        lambda _ip, _user, _password: ("rest", 443),
    )
    monkeypatch.setattr(
        mikroclaw_install,
        "method_api_detect",
        lambda _ip, _user, _password: ("api", 8728),
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    # Order should be SSH -> REST -> API (shell parity)
    assert result == [("ssh", 22), ("rest", 443), ("api", 8728)]


def test_detect_all_methods_detects_multiple_methods(monkeypatch):
    """Test that detect_all_methods detects all available methods."""
    monkeypatch.setattr(
        mikroclaw_install,
        "method_ssh_detect",
        lambda _ip, _user, _password: ("ssh", 2222),
    )
    monkeypatch.setattr(
        mikroclaw_install,
        "method_rest_detect",
        lambda _ip, _user, _password: ("rest", 80),
    )
    monkeypatch.setattr(
        mikroclaw_install, "method_api_detect", lambda _ip, _user, _password: None
    )

    result = mikroclaw_install.detect_all_methods("192.0.2.1", "admin", "pw")
    assert ("ssh", 2222) in result
    assert ("rest", 80) in result
    assert len(result) == 2


def test_select_method_menu_returns_single_method_directly(monkeypatch):
    """Test that select_method_menu auto-selects when only one method is available."""
    available = [("ssh", 22)]

    # Should not call ui_menu when there's only one option
    ui_menu_calls = []
    monkeypatch.setattr(
        mikroclaw_install,
        "ui_menu",
        lambda title, *options: ui_menu_calls.append((title, options)) or 1,
    )

    result = mikroclaw_install.select_method_menu(available)
    assert result == ("ssh", 22)
    assert len(ui_menu_calls) == 0  # No menu shown for single option


def test_select_method_menu_shows_menu_for_multiple_methods(monkeypatch):
    """Test that select_method_menu presents menu when multiple methods are available."""
    available = [("ssh", 22), ("rest", 443), ("api", 8728)]

    # Simulate user selecting option 2 (REST)
    monkeypatch.setattr(mikroclaw_install, "ui_menu", lambda title, *options: 2)

    result = mikroclaw_install.select_method_menu(available)
    assert result == ("rest", 443)


def test_select_method_menu_returns_first_on_invalid_selection(monkeypatch):
    """Test that select_method_menu returns first method on invalid selection."""
    available = [("ssh", 22), ("rest", 443)]

    # Simulate invalid selection (0)
    monkeypatch.setattr(mikroclaw_install, "ui_menu", lambda title, *options: 0)

    result = mikroclaw_install.select_method_menu(available)
    assert result == ("ssh", 22)


def test_select_method_menu_returns_first_on_out_of_range_selection(monkeypatch):
    """Test that select_method_menu returns first method when selection is out of range."""
    available = [("ssh", 22), ("rest", 443)]

    # Simulate out of range selection (5 when only 2 options)
    monkeypatch.setattr(mikroclaw_install, "ui_menu", lambda title, *options: 5)

    result = mikroclaw_install.select_method_menu(available)
    assert result == ("ssh", 22)


def test_select_method_menu_handles_api_ssl_port(monkeypatch):
    """Test that select_method_menu handles api with SSL port (8729)."""
    available = [("api", 8729)]

    result = mikroclaw_install.select_method_menu(available)
    assert result == ("api", 8729)


def test_deploy_with_method_routes_to_ssh(monkeypatch):
    """Test that deploy_with_method routes to SSH deploy function."""
    ssh_calls = []

    def mock_ssh_deploy(ip, user, password, port, binary_url, config_json):
        ssh_calls.append((ip, user, password, port, binary_url, config_json))
        return True

    monkeypatch.setattr(mikroclaw_install, "method_ssh_deploy", mock_ssh_deploy)
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_deploy", lambda *_a, **_k: False
    )
    monkeypatch.setattr(mikroclaw_install, "method_api_deploy", lambda *_a, **_k: False)

    result = mikroclaw_install.deploy_with_method(
        "ssh",
        "192.0.2.1",
        "admin",
        "pw",
        22,
        "https://example.com/binary",
        '{"key": "val"}',
    )

    assert result is True
    assert len(ssh_calls) == 1
    assert ssh_calls[0][0] == "192.0.2.1"
    assert ssh_calls[0][3] == 22


def test_deploy_with_method_routes_to_rest(monkeypatch):
    """Test that deploy_with_method routes to REST deploy function."""
    rest_calls = []

    def mock_rest_deploy(ip, user, password, port, binary_url, config_json):
        rest_calls.append((ip, user, password, port, binary_url, config_json))
        return True

    monkeypatch.setattr(mikroclaw_install, "method_ssh_deploy", lambda *_a, **_k: False)
    monkeypatch.setattr(mikroclaw_install, "method_rest_deploy", mock_rest_deploy)
    monkeypatch.setattr(mikroclaw_install, "method_api_deploy", lambda *_a, **_k: False)

    result = mikroclaw_install.deploy_with_method(
        "rest",
        "192.0.2.1",
        "admin",
        "pw",
        443,
        "https://example.com/binary",
        '{"key": "val"}',
    )

    assert result is True
    assert len(rest_calls) == 1
    assert rest_calls[0][3] == 443


def test_deploy_with_method_routes_to_api(monkeypatch):
    """Test that deploy_with_method routes to API deploy function."""
    api_calls = []

    def mock_api_deploy(ip, user, password, port, binary_url, config_json):
        api_calls.append((ip, user, password, port, binary_url, config_json))
        return True

    monkeypatch.setattr(mikroclaw_install, "method_ssh_deploy", lambda *_a, **_k: False)
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_deploy", lambda *_a, **_k: False
    )
    monkeypatch.setattr(mikroclaw_install, "method_api_deploy", mock_api_deploy)

    result = mikroclaw_install.deploy_with_method(
        "api",
        "192.0.2.1",
        "admin",
        "pw",
        8728,
        "https://example.com/binary",
        '{"key": "val"}',
    )

    assert result is True
    assert len(api_calls) == 1
    assert api_calls[0][3] == 8728


def test_deploy_with_method_propagates_failure(monkeypatch):
    """Test that deploy_with_method returns False when underlying deploy fails."""
    monkeypatch.setattr(mikroclaw_install, "method_ssh_deploy", lambda *_a, **_k: False)
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_deploy", lambda *_a, **_k: False
    )
    monkeypatch.setattr(mikroclaw_install, "method_api_deploy", lambda *_a, **_k: False)

    result = mikroclaw_install.deploy_with_method(
        "ssh",
        "192.0.2.1",
        "admin",
        "pw",
        22,
        "https://example.com/binary",
        '{"key": "val"}',
    )

    assert result is False


def test_deploy_with_method_raises_on_unknown_method(monkeypatch):
    """Test that deploy_with_method raises ValueError for unknown method."""
    try:
        mikroclaw_install.deploy_with_method(
            "unknown",
            "192.0.2.1",
            "admin",
            "pw",
            1234,
            "https://example.com/binary",
            '{"key": "val"}',
        )
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "unknown" in str(e).lower() or "unsupported" in str(e).lower()


def test_detect_all_methods_against_localhost_returns_empty():
    """Integration test: detect_all_methods against localhost should return empty."""
    # This test runs against actual localhost (no mocking)
    # Should return empty list since we're unlikely to have RouterOS on localhost
    result = mikroclaw_install.detect_all_methods("127.0.0.1", "admin", "badpass")
    assert isinstance(result, list)
    assert len(result) == 0
