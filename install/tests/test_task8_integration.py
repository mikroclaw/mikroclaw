# pyright: basic
import importlib.util
import subprocess
import sys
import time
from pathlib import Path


INSTALL_DIR = Path(__file__).resolve().parents[1]
SCRIPT_PATH = INSTALL_DIR / "mikroclaw-install.py"


def load_module():
    spec = importlib.util.spec_from_file_location("mikroclaw_install", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("Failed to load installer module spec")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_installer(*args, timeout=20):
    return subprocess.run(
        [sys.executable, str(SCRIPT_PATH), *args],
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def test_checkpoint_8a_main_dispatches_routeros_cli(monkeypatch):
    mod = load_module()
    args = mod.InstallerConfig(
        target="routeros",
        ip="127.0.0.1",
        user="admin",
        password="bad",
        method="ssh",
        provider="openrouter",
        bot_token="",
        api_key="",
        base_url="",
        model="",
        ssh_port=None,
        api_port=None,
    )
    monkeypatch.setattr(mod, "parse_args", lambda _argv=None: args)

    calls = []

    def fake_install_routeros_cli(passed):
        calls.append(passed)
        return 1

    monkeypatch.setattr(mod, "install_routeros_cli", fake_install_routeros_cli)

    rc = mod.main([])
    assert rc == 1
    assert calls == [args]


def test_checkpoint_8b_install_routeros_interactive_happy_path(monkeypatch):
    mod = load_module()

    inputs = iter(["192.0.2.1", "admin", "tg-token", "api-key", "", "", ""])
    secrets = iter(["router-pass"])

    monkeypatch.setattr(mod, "ui_clear", lambda: None)
    monkeypatch.setattr(mod, "ui_banner", lambda: None)
    monkeypatch.setattr(mod, "ui_msg", lambda _m: None)
    monkeypatch.setattr(mod, "ui_error", lambda _m: None)
    monkeypatch.setattr(
        mod, "ui_input", lambda _p, default=None: next(inputs) or default or ""
    )
    monkeypatch.setattr(mod, "ui_input_secret", lambda _p: next(secrets))
    monkeypatch.setattr(mod, "ui_menu", lambda _title, *_opts: 1)
    monkeypatch.setattr(mod, "detect_all_methods", lambda *_a: [("ssh", 22)])
    monkeypatch.setattr(mod, "select_method_menu", lambda methods: methods[0])
    monkeypatch.setattr(mod, "deploy_with_method", lambda *_a, **_k: True)

    rc = mod.install_routeros_interactive()
    assert rc == 0


def test_checkpoint_8c_main_dispatches_linux_and_docker_interactive(monkeypatch):
    mod = load_module()

    calls = []
    monkeypatch.setattr(mod, "ui_init", lambda: None)
    monkeypatch.setattr(mod, "ui_clear", lambda: None)
    monkeypatch.setattr(mod, "ui_banner", lambda: None)
    monkeypatch.setattr(mod, "ui_error", lambda _m: None)
    monkeypatch.setattr(
        mod, "install_routeros_interactive", lambda: calls.append("routeros") or 0
    )
    monkeypatch.setattr(
        mod, "install_linux_interactive", lambda: calls.append("linux") or 0
    )
    monkeypatch.setattr(
        mod, "install_docker_interactive", lambda: calls.append("docker") or 0
    )

    monkeypatch.setattr(mod, "ui_menu", lambda _title, *_opts: 2)
    assert mod.main_interactive() == 0
    monkeypatch.setattr(mod, "ui_menu", lambda _title, *_opts: 3)
    assert mod.main_interactive() == 0

    assert calls == ["linux", "docker"]


def test_checkpoint_8d_ctrl_c_returns_130_without_traceback():
    code = (
        "import importlib.util;"
        f"p=r'{SCRIPT_PATH}';"
        "spec=importlib.util.spec_from_file_location('mikroclaw_install', p);"
        "mod=importlib.util.module_from_spec(spec);"
        "spec.loader.exec_module(mod);"
        "mod.parse_args=lambda _argv=None: (_ for _ in ()).throw(KeyboardInterrupt());"
        "raise SystemExit(mod.main([]))"
    )
    result = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
    )

    assert result.returncode == 130
    assert "Traceback" not in (result.stdout + result.stderr)


def test_cli_routeros_methods_fail_gracefully_with_normalized_category():
    methods = ("ssh", "rest", "api")

    for method in methods:
        start = time.monotonic()
        result = run_installer(
            "--target",
            "routeros",
            "--ip",
            "127.0.0.1",
            "--method",
            method,
            "--user",
            "admin",
            "--pass",
            "bad",
            "--provider",
            "openrouter",
            timeout=20,
        )
        elapsed = time.monotonic() - start
        output = (result.stdout + result.stderr).lower()

        assert result.returncode == 1, output
        assert elapsed < 15.0, f"method={method} elapsed={elapsed} output={output}"
        assert ("connect" in output) or ("timeout" in output) or ("auth" in output), (
            output
        )
