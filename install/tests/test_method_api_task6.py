#!/usr/bin/env python3
# pyright: basic

import importlib.util
import os
import socket


_spec = importlib.util.spec_from_file_location(
    "mikroclaw_install",
    os.path.join(os.path.dirname(__file__), "..", "mikroclaw-install.py"),
)
assert _spec is not None
assert _spec.loader is not None
mikroclaw_install = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mikroclaw_install)


def test_api_encode_length_encodes_1_2_3_byte_ranges():
    assert mikroclaw_install._api_encode_length(5) == b"\x05"
    assert mikroclaw_install._api_encode_length(200) == b"\x80\xc8"
    assert mikroclaw_install._api_encode_length(20000) == b"\xc0N "


def test_api_send_sentence_writes_words_with_null_terminator():
    left, right = socket.socketpair()
    try:
        mikroclaw_install._api_send_sentence(left, ["!login", "=name=admin"])
        payload = right.recv(1024)
    finally:
        left.close()
        right.close()

    expected = (
        mikroclaw_install._api_encode_length(len("!login"))
        + b"!login"
        + mikroclaw_install._api_encode_length(len("=name=admin"))
        + b"=name=admin"
        + b"\x00"
    )
    assert payload == expected


def test_method_api_test_delegates_to_tcp_probe(monkeypatch):
    calls = []

    def fake_probe(host, port, timeout=2):
        calls.append((host, port, timeout))
        return True

    monkeypatch.setattr(mikroclaw_install, "tcp_probe", fake_probe)
    assert mikroclaw_install.method_api_test("10.0.0.10", 8728) is True
    assert calls == [("10.0.0.10", 8728, 2)]


def test_method_api_detect_tries_8729_then_8728(monkeypatch):
    attempted = []

    def fake_test(_ip, port=8728):
        attempted.append(port)
        return port == 8728

    monkeypatch.setattr(mikroclaw_install, "method_api_test", fake_test)

    result = mikroclaw_install.method_api_detect("10.0.0.11", "admin", "pw")
    assert result == ("api", 8728)
    assert attempted == [8729, 8728]


def test_method_api_deploy_plain_connects_and_sends_commands(monkeypatch):
    class FakeSocket:
        def __init__(self):
            self.sent = []
            self.closed = False

        def sendall(self, data):
            self.sent.append(data)

        def close(self):
            self.closed = True

    fake_sock = FakeSocket()

    def fake_create_connection(addr, timeout=None):
        assert addr == ("10.0.0.12", 8728)
        assert timeout == mikroclaw_install.CONNECT_TIMEOUT
        return fake_sock

    monkeypatch.setattr(socket, "create_connection", fake_create_connection)

    ok = mikroclaw_install.method_api_deploy(
        "10.0.0.12",
        "admin",
        "pw",
        8728,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is True
    assert fake_sock.closed is True
    assert len(fake_sock.sent) == 3


def test_method_api_deploy_ssl_wraps_socket_for_8729(monkeypatch):
    class FakeSocket:
        def __init__(self):
            self.sent = []
            self.closed = False

        def sendall(self, data):
            self.sent.append(data)

        def close(self):
            self.closed = True

    plain_sock = FakeSocket()
    ssl_sock = FakeSocket()

    monkeypatch.setattr(
        socket,
        "create_connection",
        lambda addr, timeout=None: plain_sock,
    )

    wrapped = []

    def fake_wrap_socket(sock):
        wrapped.append(sock)
        return ssl_sock

    monkeypatch.setattr(
        mikroclaw_install.ssl, "wrap_socket", fake_wrap_socket, raising=False
    )

    ok = mikroclaw_install.method_api_deploy(
        "10.0.0.13",
        "admin",
        "pw",
        8729,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is True
    assert wrapped == [plain_sock]
    assert ssl_sock.closed is True
    assert len(ssl_sock.sent) == 3


def test_method_api_deploy_returns_false_on_connect_error(monkeypatch):
    def fake_create_connection(_addr, timeout=None):
        _ = timeout
        raise OSError("connect failed")

    monkeypatch.setattr(socket, "create_connection", fake_create_connection)

    ok = mikroclaw_install.method_api_deploy(
        "10.0.0.14",
        "admin",
        "pw",
        8728,
        "https://example.invalid/mikroclaw",
        '{"BOT_TOKEN":"abc"}',
    )

    assert ok is False
