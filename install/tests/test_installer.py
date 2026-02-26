#!/usr/bin/env python3
import importlib.util
import json
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


def test_valid_ipv4_ok():
    mod = load_module()
    assert mod.is_valid_ipv4("192.168.88.1") is True


def test_valid_ipv4_reject_invalid():
    mod = load_module()
    assert mod.is_valid_ipv4("999.999.999.999") is False


def test_valid_user_ok():
    mod = load_module()
    assert mod.is_valid_user("admin.user-1") is True


def test_valid_user_reject_space():
    mod = load_module()
    assert mod.is_valid_user("bad user") is False


def test_provider_valid_openrouter():
    mod = load_module()
    assert mod.is_valid_provider("openrouter") is True


def test_provider_invalid_rejected():
    mod = load_module()
    assert mod.is_valid_provider("invalid-provider") is False


def test_config_create_defaults_openrouter():
    mod = load_module()
    cfg = json.loads(mod.config_create("bot", "key", "openrouter"))
    assert cfg["LLM_BASE_URL"] == "https://openrouter.ai/api/v1"
    assert cfg["MODEL"] == "google/gemini-flash"
    assert cfg["AUTH_TYPE"] == "bearer"


def test_config_create_auth_type_anthropic():
    mod = load_module()
    cfg = json.loads(mod.config_create("bot", "key", "anthropic"))
    assert cfg["AUTH_TYPE"] == "api_key"


def test_port_value_accepts_valid():
    mod = load_module()
    assert mod.port_value("22") == 22


def test_port_value_rejects_zero():
    mod = load_module()
    try:
        mod.port_value("0")
        assert False
    except Exception:
        assert True


def test_api_encode_length_1byte():
    mod = load_module()
    assert mod._api_encode_length(5) == b"\x05"


def test_api_encode_length_2byte():
    mod = load_module()
    out = mod._api_encode_length(200)
    assert len(out) == 2
    assert out[0] & 0x80 == 0x80


def test_api_encode_length_3byte():
    mod = load_module()
    out = mod._api_encode_length(20000)
    assert len(out) == 3
    assert out[0] & 0xC0 == 0xC0


def test_detect_all_methods_returns_list():
    mod = load_module()
    methods = mod.detect_all_methods("127.0.0.1", "admin", "bad")
    assert isinstance(methods, list)


def test_select_method_menu_single_auto_select():
    mod = load_module()
    selected = mod.select_method_menu([("ssh", 22)])
    assert selected == ("ssh", 22)


def test_deploy_with_method_unknown_raises():
    mod = load_module()
    try:
        mod.deploy_with_method("unknown", "1.2.3.4", "u", "p", 1, "url", "{}")
        assert False
    except ValueError:
        assert True
