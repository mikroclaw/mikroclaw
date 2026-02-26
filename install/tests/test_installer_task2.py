# pyright: basic
import io
import sys
from pathlib import Path
import importlib.util

INSTALL_DIR = Path(__file__).resolve().parents[1]
SCRIPT_PATH = INSTALL_DIR / "mikroclaw-install.py"


def load_module():
    spec = importlib.util.spec_from_file_location("mikroclaw_install", SCRIPT_PATH)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_ui_menu_returns_selection_from_input():
    mod = load_module()
    # Simulate user selecting option 2
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("2\n")
    try:
        result = mod.ui_menu("Test Menu", "Option A", "Option B", "Option C")
        assert result == 2, f"Expected 2, got {result}"
    finally:
        sys.stdin = old_stdin


def test_ui_menu_returns_first_option():
    mod = load_module()
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("1\n")
    try:
        result = mod.ui_menu("Test", "A", "B", "C")
        assert result == 1
    finally:
        sys.stdin = old_stdin


def test_ui_menu_returns_last_option():
    mod = load_module()
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("3\n")
    try:
        result = mod.ui_menu("Test", "A", "B", "C")
        assert result == 3
    finally:
        sys.stdin = old_stdin


def test_ui_menu_prints_to_stderr():
    mod = load_module()
    old_stdin = sys.stdin
    old_stderr = sys.stderr
    sys.stdin = io.StringIO("1\n")
    sys.stderr = io.StringIO()
    try:
        mod.ui_menu("Test Title", "Opt1", "Opt2")
        stderr_output = sys.stderr.getvalue()
        assert "Test Title" in stderr_output
        assert "[1] Opt1" in stderr_output
        assert "[2] Opt2" in stderr_output
    finally:
        sys.stdin = old_stdin
        sys.stderr = old_stderr


def test_ui_input_returns_value():
    mod = load_module()
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("test_value\n")
    try:
        result = mod.ui_input("Enter value")
        assert result == "test_value"
    finally:
        sys.stdin = old_stdin


def test_ui_input_returns_default_on_empty():
    mod = load_module()
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("\n")
    try:
        result = mod.ui_input("Enter value", default="default_val")
        assert result == "default_val"
    finally:
        sys.stdin = old_stdin


def test_ui_input_ignores_default_when_value_entered():
    mod = load_module()
    old_stdin = sys.stdin
    sys.stdin = io.StringIO("entered_value\n")
    try:
        result = mod.ui_input("Enter value", default="default_val")
        assert result == "entered_value"
    finally:
        sys.stdin = old_stdin


def test_ui_input_prints_prompt_to_stderr():
    mod = load_module()
    old_stdin = sys.stdin
    old_stderr = sys.stderr
    sys.stdin = io.StringIO("value\n")
    sys.stderr = io.StringIO()
    try:
        mod.ui_input("Test prompt")
        stderr_output = sys.stderr.getvalue()
        assert "Test prompt" in stderr_output
    finally:
        sys.stdin = old_stdin
        sys.stderr = old_stderr


def test_ui_input_shows_default_in_prompt():
    mod = load_module()
    old_stdin = sys.stdin
    old_stderr = sys.stderr
    sys.stdin = io.StringIO("\n")
    sys.stderr = io.StringIO()
    try:
        mod.ui_input("Test prompt", default="mydefault")
        stderr_output = sys.stderr.getvalue()
        assert "[mydefault]" in stderr_output
    finally:
        sys.stdin = old_stdin
        sys.stderr = old_stderr


def test_ui_progress_prints_to_stderr():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_progress("Testing progress")
        stderr_output = sys.stderr.getvalue()
        assert "⏳ Testing progress..." in stderr_output
    finally:
        sys.stderr = old_stderr


def test_ui_done_prints_to_stderr():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_done()
        stderr_output = sys.stderr.getvalue()
        assert "✅" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_ui_error_prints_to_stderr():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_error("Something went wrong")
        stderr_output = sys.stderr.getvalue()
        assert "❌ Something went wrong" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_ui_msg_prints_to_stderr():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_msg("Information message")
        stderr_output = sys.stderr.getvalue()
        assert "Information message" in stderr_output
        # Should have leading spaces (2 spaces)
        lines = stderr_output.split("\n")
        msg_line = [l for l in lines if "Information message" in l][0]
        assert msg_line.startswith("  ")
    finally:
        sys.stderr = old_stderr


def test_ui_banner_prints_to_stderr():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_banner()
        stderr_output = sys.stderr.getvalue()
        assert "MikroClaw" in stderr_output
        assert "╔" in stderr_output  # Box drawing character
        assert "╝" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_ui_banner_includes_version():
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.ui_banner()
        stderr_output = sys.stderr.getvalue()
        assert "2025.02.25:BETA2" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_ui_clear_exists():
    mod = load_module()
    # Just verify function exists and is callable
    assert callable(mod.ui_clear)


def test_ui_input_secret_exists():
    mod = load_module()
    # Just verify function exists and is callable
    assert callable(mod.ui_input_secret)
