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


def test_method_ssh_test_returns_false_when_tcp_probe_fails(monkeypatch):
    monkeypatch.setattr(
        mikroclaw_install, "tcp_probe", lambda _ip, _port, timeout=2: False
    )

    def fail_if_called(*_args, **_kwargs):
        raise AssertionError("subprocess.run should not be called when TCP probe fails")

    monkeypatch.setattr(subprocess, "run", fail_if_called)

    assert (
        mikroclaw_install.method_ssh_test("192.0.2.1", "admin", "secret", 22) is False
    )


def test_method_ssh_test_uses_sshpass_when_available(monkeypatch):
    monkeypatch.setattr(
        mikroclaw_install, "tcp_probe", lambda _ip, _port, timeout=2: True
    )
    monkeypatch.setattr(
        mikroclaw_install.shutil,
        "which",
        lambda name: "/usr/bin/sshpass" if name == "sshpass" else None,
    )

    calls = []

    def fake_run(cmd, **kwargs):
        calls.append((cmd, kwargs))
        return subprocess.CompletedProcess(cmd, 0, stdout="OK\n", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    ok = mikroclaw_install.method_ssh_test("10.0.0.1", "admin", "pw", 2222)

    assert ok is True
    assert calls
    cmd = calls[0][0]
    assert cmd[0] == "sshpass"
    assert cmd[1] == "-p"
    assert cmd[2] == "pw"
    assert "ssh" in cmd


def test_method_ssh_test_falls_back_to_key_auth(monkeypatch):
    monkeypatch.setattr(
        mikroclaw_install, "tcp_probe", lambda _ip, _port, timeout=2: True
    )
    monkeypatch.setattr(mikroclaw_install.shutil, "which", lambda _name: None)

    calls = []

    def fake_run(cmd, **kwargs):
        calls.append((cmd, kwargs))
        return subprocess.CompletedProcess(cmd, 0, stdout="OK\n", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    ok = mikroclaw_install.method_ssh_test("10.0.0.2", "admin", "pw", 22)

    assert ok is True
    cmd = calls[0][0]
    assert cmd[0] == "ssh"
    assert "-o" in cmd
    assert "BatchMode=yes" in cmd


def test_method_ssh_detect_tries_expected_ports(monkeypatch):
    attempted = []

    def fake_test(_ip, _user, _password, port=22):
        attempted.append(port)
        return port == 2222

    monkeypatch.setattr(mikroclaw_install, "method_ssh_test", fake_test)

    result = mikroclaw_install.method_ssh_detect("10.0.0.3", "admin", "pw")

    assert result == ("ssh", 2222)
    assert attempted == [22, 2222]


def test_method_ssh_deploy_uploads_binary_and_config_and_cleans_tmp(
    monkeypatch, tmp_path
):
    tmpdir = tmp_path / "sshdeploy"
    tmpdir.mkdir()

    monkeypatch.setattr(
        mikroclaw_install, "method_ssh_test", lambda *_args, **_kwargs: True
    )
    monkeypatch.setattr(
        mikroclaw_install.shutil,
        "which",
        lambda name: "/usr/bin/sshpass" if name == "sshpass" else None,
    )
    monkeypatch.setattr(mikroclaw_install.tempfile, "mkdtemp", lambda: str(tmpdir))

    def fake_download(_url, out_path):
        with open(out_path, "wb") as fh:
            fh.write(b"bin")

    monkeypatch.setattr(mikroclaw_install, "_download_to_file", fake_download)

    calls = []

    def fake_run(cmd, **kwargs):
        calls.append((cmd, kwargs))
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    ok = mikroclaw_install.method_ssh_deploy(
        "10.0.0.4",
        "admin",
        "pw",
        22,
        "https://example/binary",
        '{"BOT_TOKEN":"x"}',
    )

    assert ok is True
    assert len(calls) == 2
    assert calls[0][0][-1] == "admin@10.0.0.4:/disk1/mikroclaw"
    assert calls[1][0][-1] == "admin@10.0.0.4:/disk1/mikroclaw.env.json"
    assert not tmpdir.exists()


def test_method_ssh_deploy_cleans_tmp_on_upload_failure(monkeypatch, tmp_path):
    tmpdir = tmp_path / "sshdeploy-fail"
    tmpdir.mkdir()

    monkeypatch.setattr(
        mikroclaw_install, "method_ssh_test", lambda *_args, **_kwargs: True
    )
    monkeypatch.setattr(mikroclaw_install.shutil, "which", lambda _name: None)
    monkeypatch.setattr(mikroclaw_install.tempfile, "mkdtemp", lambda: str(tmpdir))

    def fake_download(_url, out_path):
        with open(out_path, "wb") as fh:
            fh.write(b"bin")

    monkeypatch.setattr(mikroclaw_install, "_download_to_file", fake_download)

    def fake_run(_cmd, **_kwargs):
        return subprocess.CompletedProcess([], 1, stdout="", stderr="scp failed")

    monkeypatch.setattr(subprocess, "run", fake_run)

    ok = mikroclaw_install.method_ssh_deploy(
        "10.0.0.5",
        "admin",
        "pw",
        22,
        "https://example/binary",
        '{"BOT_TOKEN":"x"}',
    )

    assert ok is False
    assert not tmpdir.exists()
