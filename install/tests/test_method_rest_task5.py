#!/usr/bin/env python3
# pyright: basic

import base64
import importlib.util
import json
import os


_spec = importlib.util.spec_from_file_location(
    "mikroclaw_install",
    os.path.join(os.path.dirname(__file__), "..", "mikroclaw-install.py"),
)
assert _spec is not None
assert _spec.loader is not None
mikroclaw_install = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mikroclaw_install)


def _expected_auth(user, password):
    token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
    return f"Basic {token}"


def test_method_rest_test_uses_basic_auth_and_resource_endpoint(monkeypatch):
    calls = []

    def fake_http_request(url, method="GET", headers=None, data=None, timeout=10):
        calls.append((url, method, headers or {}, data, timeout))
        return (200, b"[]")

    monkeypatch.setattr(mikroclaw_install, "http_request", fake_http_request)

    ok = mikroclaw_install.method_rest_test("10.0.0.10", "admin", "pw", 443)

    assert ok is True
    assert len(calls) == 1
    url, method, headers, data, timeout = calls[0]
    assert url == "https://10.0.0.10:443/rest/system/resource"
    assert method == "GET"
    assert headers["Authorization"] == _expected_auth("admin", "pw")
    assert headers["Accept"] == "application/json"
    assert data is None
    assert timeout == mikroclaw_install.CONNECT_TIMEOUT


def test_method_rest_test_returns_false_on_auth_or_connection_errors(monkeypatch):
    def raise_auth(*_args, **_kwargs):
        raise mikroclaw_install.AuthError("bad creds")

    monkeypatch.setattr(mikroclaw_install, "http_request", raise_auth)
    assert mikroclaw_install.method_rest_test("10.0.0.10", "admin", "bad", 443) is False


def test_method_rest_detect_tries_443_then_80(monkeypatch):
    attempted = []

    def fake_test(_ip, _user, _password, port=443):
        attempted.append(port)
        return port == 80

    monkeypatch.setattr(mikroclaw_install, "method_rest_test", fake_test)

    result = mikroclaw_install.method_rest_detect("10.0.0.11", "admin", "pw")

    assert result == ("rest", 80)
    assert attempted == [443, 80]


def test_method_rest_deploy_posts_fetch_and_puts_config(monkeypatch):
    calls = []

    monkeypatch.setattr(
        mikroclaw_install, "method_rest_test", lambda *_args, **_kwargs: True
    )

    def fake_http_request(url, method="GET", headers=None, data=None, timeout=10):
        calls.append((url, method, headers or {}, data, timeout))
        if url.endswith("/rest/tool/fetch") and method == "POST":
            return (200, b"{}")
        if url.endswith("/rest/file") and method == "PUT":
            return (200, b"{}")
        if url.endswith("/rest/file") and method == "GET":
            return (200, b'[{"name":"disk1/mikroclaw.env.json"}]')
        raise AssertionError(f"unexpected request: {method} {url}")

    monkeypatch.setattr(mikroclaw_install, "http_request", fake_http_request)

    ok = mikroclaw_install.method_rest_deploy(
        "10.0.0.12",
        "admin",
        "pw",
        443,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is True
    assert len(calls) == 3

    fetch_payload = json.loads(calls[0][3].decode("utf-8"))
    assert calls[0][0] == "https://10.0.0.12:443/rest/tool/fetch"
    assert calls[0][1] == "POST"
    assert fetch_payload == {
        "url": "https://example.invalid/mikroclaw",
        "dst-path": "disk1/mikroclaw",
        "mode": "https",
    }

    put_payload = json.loads(calls[1][3].decode("utf-8"))
    assert calls[1][0] == "https://10.0.0.12:443/rest/file"
    assert calls[1][1] == "PUT"
    assert put_payload["name"] == "disk1/mikroclaw.env.json"
    assert put_payload["content"] == '{"BOT_TOKEN":"abc"}'


def test_method_rest_deploy_falls_back_to_script_when_put_fails(monkeypatch):
    calls = []

    monkeypatch.setattr(
        mikroclaw_install, "method_rest_test", lambda *_args, **_kwargs: True
    )

    def fake_http_request(url, method="GET", headers=None, data=None, timeout=10):
        calls.append((url, method, headers or {}, data, timeout))
        if url.endswith("/rest/tool/fetch") and method == "POST":
            return (200, b"{}")
        if url.endswith("/rest/file") and method == "PUT":
            return (500, b"{}")
        if url.endswith("/rest/system/script/run") and method == "POST":
            return (200, b"{}")
        if url.endswith("/rest/file") and method == "GET":
            return (200, b'[{"name":"disk1/mikroclaw.env.json"}]')
        raise AssertionError(f"unexpected request: {method} {url}")

    monkeypatch.setattr(mikroclaw_install, "http_request", fake_http_request)

    ok = mikroclaw_install.method_rest_deploy(
        "10.0.0.13",
        "admin",
        "pw",
        80,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is True
    assert any(path.endswith("/rest/system/script/run") for path, *_ in calls)


def test_method_rest_deploy_returns_false_when_fetch_fails(monkeypatch):
    monkeypatch.setattr(
        mikroclaw_install, "method_rest_test", lambda *_args, **_kwargs: True
    )

    def fake_http_request(url, method="GET", headers=None, data=None, timeout=10):
        _ = (headers, data, timeout)
        if url.endswith("/rest/tool/fetch") and method == "POST":
            return (500, b"{}")
        raise AssertionError(f"unexpected request: {method} {url}")

    monkeypatch.setattr(mikroclaw_install, "http_request", fake_http_request)

    ok = mikroclaw_install.method_rest_deploy(
        "10.0.0.14",
        "admin",
        "pw",
        443,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is False
