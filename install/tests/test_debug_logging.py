# pyright: basic
"""Tests for debug logging functionality.

This module tests the debug() function, set_verbose() function,
and CLI verbose flag parsing for the installer.
"""

import io
import sys
import importlib.util
from pathlib import Path

INSTALL_DIR = Path(__file__).resolve().parents[1]
SCRIPT_PATH = INSTALL_DIR / "mikroclaw-install.py"


def load_module():
    spec = importlib.util.spec_from_file_location("installer", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("Failed to load module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_debug_prints_at_level_1():
    """debug() prints when _VERBOSE=1."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.set_verbose(1)
        mod.debug("test message")
        stderr_output = sys.stderr.getvalue()
        assert "test message" in stderr_output
        assert "DEBUG:" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_debug_silent_at_level_0():
    """debug() prints nothing when _VERBOSE=0."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.set_verbose(0)
        mod.debug("should not appear")
        stderr_output = sys.stderr.getvalue()
        assert stderr_output == ""
    finally:
        sys.stderr = old_stderr


def test_debug_filters_by_level():
    """debug(msg, level=2) silent when _VERBOSE=1."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.set_verbose(1)
        mod.debug("level 2 message", level=2)
        stderr_output = sys.stderr.getvalue()
        assert stderr_output == ""
    finally:
        sys.stderr = old_stderr


def test_debug_level_2_prints_when_verbose_2():
    """debug(msg, level=2) prints when _VERBOSE=2."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.set_verbose(2)
        mod.debug("level 2 message", level=2)
        stderr_output = sys.stderr.getvalue()
        assert "level 2 message" in stderr_output
        assert "DEBUG:" in stderr_output
    finally:
        sys.stderr = old_stderr


def test_debug_output_format():
    """Output contains [HH:MM:SS] DEBUG: prefix."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        mod.set_verbose(1)
        mod.debug("format test")
        stderr_output = sys.stderr.getvalue()
        # Check format: [HH:MM:SS] DEBUG: message
        assert "DEBUG:" in stderr_output
        # Check timestamp format [HH:MM:SS]
        import re

        assert re.search(r"\[\d{2}:\d{2}:\d{2}\] DEBUG:", stderr_output)
    finally:
        sys.stderr = old_stderr


def test_set_verbose_changes_global():
    """set_verbose() updates _VERBOSE global."""
    mod = load_module()
    old_stderr = sys.stderr
    sys.stderr = io.StringIO()
    try:
        # Start with verbose 0
        mod.set_verbose(0)
        mod.debug("msg at 0")
        assert sys.stderr.getvalue() == ""

        # Change to verbose 1
        sys.stderr = io.StringIO()
        mod.set_verbose(1)
        mod.debug("msg at 1")
        assert "msg at 1" in sys.stderr.getvalue()
    finally:
        sys.stderr = old_stderr


def test_verbose_flag_parsed_correctly():
    """CLI -v sets verbose=1, -vv sets verbose=2."""
    mod = load_module()
    parser = mod.build_parser()

    # Test -v flag
    args1 = parser.parse_args(["-v"])
    assert args1.verbose == 1

    # Test -vv flag (count action)
    args2 = parser.parse_args(["-vv"])
    assert args2.verbose == 2

    # Test --verbose flag
    args3 = parser.parse_args(["--verbose"])
    assert args3.verbose == 1

    # Test multiple --verbose flags
    args4 = parser.parse_args(["--verbose", "--verbose"])
    assert args4.verbose == 2


def test_no_verbose_flag_defaults_to_zero():
    """No flag sets verbose=0."""
    mod = load_module()
    parser = mod.build_parser()

    args = parser.parse_args([])
    assert args.verbose == 0
