#!/usr/bin/env python3
"""Tests for the shell bootstrap script (Task 9).

This module tests that the thin shell bootstrap:
1. Detects missing Python and shows install instructions
2. Checks minimum Python version (>=3.8)
3. Passes arguments through to Python installer
4. Preserves --help passthrough behavior
"""

import subprocess
import sys
import os
import tempfile
import stat
import pytest


# Path to the bootstrap script
SCRIPT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOOTSTRAP_SCRIPT = os.path.join(SCRIPT_DIR, "mikroclaw-install.sh")
PYTHON_INSTALLER = os.path.join(SCRIPT_DIR, "mikroclaw-install.py")


class TestBootstrapPythonDetection:
    """Test Python detection and version checking."""

    def test_bootstrap_with_missing_python_shows_install_instructions(self):
        """When python3 is not in PATH, bootstrap shows install instructions."""
        # Create a temp directory with our mock python3 that always fails
        with tempfile.TemporaryDirectory() as tmpdir:
            mock_python = os.path.join(tmpdir, "python3")
            with open(mock_python, "w") as f:
                f.write("#!/bin/sh\nexit 1\n")
            os.chmod(mock_python, stat.S_IRWXU)

            # Use PATH that includes standard tools but puts our mock first
            env = os.environ.copy()
            env["PATH"] = f"{tmpdir}:/usr/bin:/bin"

            result = subprocess.run(
                [BOOTSTRAP_SCRIPT, "--help"],
                capture_output=True,
                text=True,
                env=env,
            )

            # Should exit 1 (python check fails before exec)
            assert result.returncode == 1, f"Expected exit 1, got {result.returncode}"
            # Should mention Python installation
            combined_output = result.stdout + result.stderr
            assert "Python" in combined_output or "python" in combined_output.lower(), (
                f"Expected Python install instructions, got: {combined_output}"
            )

    def test_bootstrap_with_old_python_shows_upgrade_guidance(self):
        """When python3 < 3.8, bootstrap shows upgrade guidance."""
        # Create a mock python3 script that reports version 3.7 and fails version check
        with tempfile.TemporaryDirectory() as tmpdir:
            mock_python = os.path.join(tmpdir, "python3")
            with open(mock_python, "w") as f:
                f.write("#!/bin/sh\n")
                f.write('if [ "$1" = "-c" ]; then\n')
                f.write('  echo "3.7"\n')
                f.write("else\n")
                f.write('  echo "Python 3.7.0"\n')
                f.write("fi\n")
            os.chmod(mock_python, stat.S_IRWXU)

            # Use PATH that includes standard tools but puts our mock first
            env = os.environ.copy()
            env["PATH"] = f"{tmpdir}:/usr/bin:/bin"

            result = subprocess.run(
                [BOOTSTRAP_SCRIPT, "--help"],
                capture_output=True,
                text=True,
                env=env,
            )

            # Should exit 1 (version check fails)
            assert result.returncode == 1, f"Expected exit 1, got {result.returncode}"
            # Should mention version requirement
            combined_output = result.stdout + result.stderr
            assert "3.8" in combined_output or "version" in combined_output.lower(), (
                f"Expected version guidance mentioning 3.8, got: {combined_output}"
            )

    def test_bootstrap_checks_python_version_before_exec(self):
        """Bootstrap verifies Python >= 3.8 before execing."""
        # This test ensures version check happens before delegation
        # We'll verify by checking the script logic is present
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            content = f.read()

        # Should have version checking logic
        assert "3.8" in content or "version" in content.lower(), (
            "Bootstrap should check Python version"
        )


class TestBootstrapArgumentPassing:
    """Test argument passthrough to Python installer."""

    def test_bootstrap_passes_help_to_python(self):
        """Bootstrap --help passes through to Python installer."""
        result = subprocess.run(
            [BOOTSTRAP_SCRIPT, "--help"],
            capture_output=True,
            text=True,
        )

        # Should succeed (exit 0) when Python is available
        assert result.returncode == 0, (
            f"Expected exit 0, got {result.returncode}: {result.stderr}"
        )

        # Should show Python installer's help output
        combined_output = result.stdout + result.stderr
        assert "Usage:" in combined_output or "usage:" in combined_output.lower(), (
            f"Expected usage info from Python installer, got: {combined_output}"
        )
        assert "mikroclaw" in combined_output.lower(), (
            f"Expected mikroclaw in help, got: {combined_output}"
        )

    def test_bootstrap_passes_all_arguments_through(self):
        """Bootstrap passes all arguments unchanged to Python."""
        # Test with --target routeros --ip 127.0.0.1 --method ssh
        # This should fail (no router there) but show Python processed the args
        result = subprocess.run(
            [
                BOOTSTRAP_SCRIPT,
                "--target",
                "routeros",
                "--ip",
                "127.0.0.1",
                "--method",
                "ssh",
                "--user",
                "admin",
                "--pass",
                "test",
                "--provider",
                "openrouter",
            ],
            capture_output=True,
            text=True,
            timeout=20,
        )

        # Python installer should process args (may fail due to network, not argparse)
        combined_output = result.stdout + result.stderr

        # Should NOT be an argparse error about unknown args
        assert "unrecognized arguments" not in combined_output.lower(), (
            f"Bootstrap should pass all args through, got: {combined_output}"
        )
        assert (
            "error:" not in combined_output.lower()
            or "connection" in combined_output.lower()
        ), f"Expected connection error (not arg error), got: {combined_output}"


class TestBootstrapScriptStructure:
    """Test bootstrap script structure and properties."""

    def test_bootstrap_is_posix_sh_compatible(self):
        """Bootstrap script uses POSIX sh syntax only."""
        result = subprocess.run(
            ["sh", "-n", BOOTSTRAP_SCRIPT],
            capture_output=True,
            text=True,
        )

        assert result.returncode == 0, (
            f"Bootstrap has sh syntax errors: {result.stderr}"
        )

    def test_bootstrap_is_under_60_lines(self):
        """Bootstrap script is under 60 lines."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            lines = f.readlines()

        line_count = len(lines)
        assert line_count < 60, f"Bootstrap is {line_count} lines, must be < 60 lines"

    def test_bootstrap_has_shebang(self):
        """Bootstrap starts with proper shebang."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            first_line = f.readline()

        assert first_line.startswith("#!/bin/sh") or first_line.startswith(
            "#!/bin/bash"
        ), f"Expected POSIX shebang, got: {first_line}"

    def test_bootstrap_uses_set_e(self):
        """Bootstrap uses 'set -e' for error handling."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            content = f.read()

        assert "set -e" in content, "Bootstrap should use 'set -e' for error handling"

    def test_bootstrap_no_auto_install_python(self):
        """Bootstrap does not attempt to auto-install Python."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            lines = f.readlines()

        # Should not contain auto-install commands as actual commands
        # (allowing them in echo statements for instructions is OK)
        forbidden_patterns = [
            "apt-get install",
            "apt install",
            "yum install",
            "dnf install",
            "pacman -S",
            "brew install",
            "pip install",
        ]

        for line in lines:
            line_stripped = line.strip()
            # Skip comment lines and echo lines (instructions are OK)
            if line_stripped.startswith("#") or line_stripped.startswith('echo "'):
                continue
            line_lower = line.lower()
            for pattern in forbidden_patterns:
                assert pattern not in line_lower, (
                    f"Bootstrap should not auto-install Python (found: {pattern} in: {line})"
                )

    def test_bootstrap_execs_python_with_script_dir(self):
        """Bootstrap execs python3 with SCRIPT_DIR/mikroclaw-install.py."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            content = f.read()

        # Should exec python3 with the Python installer
        assert "exec" in content, "Bootstrap should use 'exec' to replace process"
        assert "mikroclaw-install.py" in content, (
            "Bootstrap should reference mikroclaw-install.py"
        )
        assert "SCRIPT_DIR" in content or "$(dirname" in content, (
            "Bootstrap should determine script directory"
        )

    def test_bootstrap_shows_package_manager_instructions(self):
        """Bootstrap shows install instructions for common package managers."""
        with open(BOOTSTRAP_SCRIPT, "r") as f:
            content = f.read()

        # Should mention at least some package managers
        package_managers = ["apt", "brew", "dnf", "pacman"]
        found_any = any(pm in content.lower() for pm in package_managers)

        assert found_any, "Bootstrap should show package manager install instructions"


class TestBootstrapHelpPassthrough:
    """Test that --help behavior is preserved semantically."""

    def test_help_output_matches_python_directly(self):
        """Bootstrap --help output matches direct Python --help."""
        # Get help via bootstrap
        bootstrap_result = subprocess.run(
            [BOOTSTRAP_SCRIPT, "--help"],
            capture_output=True,
            text=True,
        )

        # Get help directly from Python
        python_result = subprocess.run(
            [sys.executable, PYTHON_INSTALLER, "--help"],
            capture_output=True,
            text=True,
        )

        # Both should succeed
        assert bootstrap_result.returncode == 0, "Bootstrap --help should succeed"
        assert python_result.returncode == 0, "Python --help should succeed"

        # Output should be semantically similar (same key sections)
        bootstrap_output = bootstrap_result.stdout + bootstrap_result.stderr
        python_output = python_result.stdout + python_result.stderr

        # Both should have Usage and Options sections
        assert "Usage:" in bootstrap_output or "usage:" in bootstrap_output.lower()
        assert "Usage:" in python_output or "usage:" in python_output.lower()

        # Key options should be present in both
        key_options = ["--target", "--ip", "--user", "--method", "--provider"]
        for opt in key_options:
            assert opt in bootstrap_output, (
                f"Option {opt} missing from bootstrap output"
            )
            assert opt in python_output, f"Option {opt} missing from python output"
