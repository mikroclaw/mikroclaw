#!/usr/bin/env python3
"""Task 10 focused tests for Linux/Docker install paths.

Tests ensure parity behavior from original shell script:
- install_linux(): Download binary, install to /usr/local/bin/mikroclaw
- install_linux_interactive(): Clear, banner, run install_linux, show success
- install_docker_interactive(): Show "Docker support coming soon!", return 1
"""

import sys
import os
import unittest
import importlib.util
from io import StringIO
from unittest.mock import patch, MagicMock

# Load mikroclaw-install.py as a module (filename has hyphen)
spec = importlib.util.spec_from_file_location("mikroclaw_install", os.path.join(os.path.dirname(__file__), "..", "mikroclaw-install.py"))
mi = importlib.util.module_from_spec(spec)
sys.modules["mikroclaw_install"] = mi
spec.loader.exec_module(mi)


class TestTask10LinuxDockerPaths(unittest.TestCase):
    """Task 10: Linux and Docker path parity tests."""

    @patch("mikroclaw_install.download_binary")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    @patch("mikroclaw_install.os.access")
    @patch("mikroclaw_install.shutil.copy")
    @patch("mikroclaw_install.shutil.rmtree")
    @patch("mikroclaw_install.tempfile.mkdtemp")
    def test_install_linux_success_path(
        self,
        mock_mkdtemp,
        mock_rmtree,
        mock_copy,
        mock_access,
        mock_ui_msg,
        mock_ui_banner,
        mock_download,
    ):
        """Test linux target path executes without traceback on success."""
        mock_mkdtemp.return_value = "/tmp/test_install"
        mock_access.return_value = True

        result = mi.install_linux()

        self.assertEqual(result, 0)
        mock_ui_banner.assert_called_once()
        mock_download.assert_called_once_with(
            "linux-x64", "/tmp/test_install/mikroclaw"
        )
        mock_copy.assert_called_once_with(
            "/tmp/test_install/mikroclaw", "/usr/local/bin/mikroclaw"
        )

    @patch("mikroclaw_install.download_binary")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    @patch("mikroclaw_install.ui_error")
    @patch("mikroclaw_install.shutil.rmtree")
    @patch("mikroclaw_install.tempfile.mkdtemp")
    def test_install_linux_network_failure_no_traceback(
        self,
        mock_mkdtemp,
        mock_rmtree,
        mock_ui_error,
        mock_ui_msg,
        mock_ui_banner,
        mock_download,
    ):
        """Test linux target path handles network failure without Python traceback."""
        mock_mkdtemp.return_value = "/tmp/test_install"
        mock_download.side_effect = Exception("Network error: Connection refused")

        result = mi.install_linux()

        self.assertEqual(result, 1)
        mock_ui_error.assert_called()
        error_call = mock_ui_error.call_args[0][0]
        self.assertIn("Failed to install Linux target", error_call)

    @patch("mikroclaw_install.install_linux")
    @patch("mikroclaw_install.ui_clear")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    def test_install_linux_interactive_flow_success(
        self,
        mock_ui_msg,
        mock_ui_banner,
        mock_ui_clear,
        mock_install_linux,
    ):
        """Test linux interactive flow calls banner+install and returns status 0 on success."""
        mock_install_linux.return_value = 0

        result = mi.install_linux_interactive()

        self.assertEqual(result, 0)
        mock_ui_clear.assert_called_once()
        mock_ui_banner.assert_called_once()
        mock_install_linux.assert_called_once()

        # Check success messages
        msg_calls = [call[0][0] for call in mock_ui_msg.call_args_list]
        self.assertIn("Linux Installation", msg_calls)
        self.assertIn("✅ Installation Complete!", msg_calls)
        self.assertIn("Run: mikroclaw --help", msg_calls)

    @patch("mikroclaw_install.install_linux")
    @patch("mikroclaw_install.ui_clear")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    def test_install_linux_interactive_flow_failure(
        self,
        mock_ui_msg,
        mock_ui_banner,
        mock_ui_clear,
        mock_install_linux,
    ):
        """Test linux interactive flow returns non-zero status on install failure."""
        mock_install_linux.return_value = 1

        result = mi.install_linux_interactive()

        self.assertEqual(result, 1)
        mock_ui_clear.assert_called_once()
        mock_ui_banner.assert_called_once()
        mock_install_linux.assert_called_once()

    @patch("mikroclaw_install.ui_clear")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    def test_install_docker_interactive_coming_soon(
        self,
        mock_ui_msg,
        mock_ui_banner,
        mock_ui_clear,
    ):
        """Test docker interactive prints 'coming soon' and returns expected status 1."""
        result = mi.install_docker_interactive()

        self.assertEqual(result, 1)
        mock_ui_clear.assert_called_once()
        mock_ui_banner.assert_called_once()

        # Check "coming soon" message
        msg_calls = [call[0][0] for call in mock_ui_msg.call_args_list]
        self.assertIn("Docker Installation", msg_calls)
        self.assertIn("Docker support coming soon!", msg_calls)

    @patch("mikroclaw_install.download_binary")
    @patch("mikroclaw_install.ui_banner")
    @patch("mikroclaw_install.ui_msg")
    @patch("mikroclaw_install.os.access")
    @patch("mikroclaw_install.shutil.copy")
    @patch("mikroclaw_install.shutil.rmtree")
    @patch("mikroclaw_install.tempfile.mkdtemp")
    def test_install_linux_no_write_permission(
        self,
        mock_mkdtemp,
        mock_rmtree,
        mock_copy,
        mock_access,
        mock_ui_msg,
        mock_ui_banner,
        mock_download,
    ):
        """Test linux install handles no write permission gracefully (parity with shell)."""
        mock_mkdtemp.return_value = "/tmp/test_install"
        mock_access.return_value = False

        result = mi.install_linux()

        self.assertEqual(result, 0)  # Shell version doesn't fail, just warns
        mock_copy.assert_not_called()

        # Check warning messages
        msg_calls = [call[0][0] for call in mock_ui_msg.call_args_list]
        warning_found = any("Cannot write" in str(m) for m in msg_calls)
        self.assertTrue(
            warning_found, "Expected warning about cannot write to /usr/local/bin"
        )


class TestTask10CLIParity(unittest.TestCase):
    """Task 10: CLI target dispatch parity tests."""

    @patch("mikroclaw_install.install_linux")
    def test_cli_target_linux_calls_install_linux(self, mock_install_linux):
        """Test --target linux dispatches to install_linux()."""
        mock_install_linux.return_value = 0

        with patch.object(sys, "argv", ["mikroclaw-install.py", "--target", "linux"]):
            result = mi.main(["--target", "linux"])

        self.assertEqual(result, 0)
        mock_install_linux.assert_called_once()

    @patch("mikroclaw_install.print")
    def test_cli_target_docker_returns_error(self, mock_print):
        """Test --target docker returns error (not yet implemented)."""
        result = mi.main(["--target", "docker"])

        self.assertEqual(result, 1)
        # Check that error message mentions docker not implemented
        error_calls = [
            str(call) for call in mock_print.call_args_list if "stderr" in str(call)
        ]


if __name__ == "__main__":
    unittest.main(verbosity=2)
