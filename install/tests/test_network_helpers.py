#!/usr/bin/env python3
"""Tests for Task 3: Network helpers (error types, tcp_probe, http_request)."""

import importlib.util
import os
import unittest

# Import mikroclaw-install.py using importlib (file has hyphen in name)
_spec = importlib.util.spec_from_file_location(
    "mikroclaw_install",
    os.path.join(os.path.dirname(__file__), "..", "mikroclaw-install.py"),
)
assert _spec is not None
assert _spec.loader is not None
mikroclaw_install = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mikroclaw_install)

# Import the components we need to test
InstallerError = mikroclaw_install.InstallerError
ConnectionError = mikroclaw_install.ConnectionError
AuthError = mikroclaw_install.AuthError
TimeoutError = mikroclaw_install.TimeoutError
CONNECT_TIMEOUT = mikroclaw_install.CONNECT_TIMEOUT
TRANSFER_TIMEOUT = mikroclaw_install.TRANSFER_TIMEOUT
tcp_probe = mikroclaw_install.tcp_probe
http_request = mikroclaw_install.http_request


class TestErrorClasses(unittest.TestCase):
    """Test error class hierarchy and attributes."""

    def test_installer_error_base_class(self):
        """InstallerError should be an Exception subclass."""
        self.assertTrue(issubclass(InstallerError, Exception))
        err = InstallerError("test message")
        self.assertEqual(str(err), "test message")

    def test_connection_error_inheritance(self):
        """ConnectionError should inherit from InstallerError."""
        self.assertTrue(issubclass(ConnectionError, InstallerError))
        err = ConnectionError("connection failed")
        self.assertEqual(str(err), "connection failed")

    def test_auth_error_inheritance(self):
        """AuthError should inherit from InstallerError."""
        self.assertTrue(issubclass(AuthError, InstallerError))
        err = AuthError("auth failed")
        self.assertEqual(str(err), "auth failed")

    def test_timeout_error_inheritance(self):
        """TimeoutError should inherit from InstallerError."""
        self.assertTrue(issubclass(TimeoutError, InstallerError))
        err = TimeoutError("timed out")
        self.assertEqual(str(err), "timed out")


class TestConstants(unittest.TestCase):
    """Test timeout constants."""

    def test_connect_timeout_constant(self):
        """CONNECT_TIMEOUT should be 10 seconds."""
        self.assertEqual(CONNECT_TIMEOUT, 10)

    def test_transfer_timeout_constant(self):
        """TRANSFER_TIMEOUT should be 120 seconds."""
        self.assertEqual(TRANSFER_TIMEOUT, 120)


class TestTcpProbe(unittest.TestCase):
    """Test tcp_probe function."""

    def test_tcp_probe_closed_port_returns_false(self):
        """tcp_probe should return False for closed port."""
        # Use a high port that's unlikely to be open
        result = tcp_probe("127.0.0.1", 59999, timeout=2)
        self.assertFalse(result)

    def test_tcp_probe_respects_timeout(self):
        """tcp_probe should complete within timeout + small buffer."""
        import time

        start = time.time()
        result = tcp_probe("192.0.2.1", 12345, timeout=1)  # TEST-NET-1, won't respond
        elapsed = time.time() - start
        self.assertFalse(result)
        self.assertLess(elapsed, 3)  # Should complete within 3 seconds


class TestHttpRequest(unittest.TestCase):
    """Test http_request function."""

    def test_http_request_raises_timeout_for_unreachable(self):
        """http_request should raise TimeoutError for unreachable host."""
        # 192.0.2.0/24 is TEST-NET-1, reserved for documentation, won't respond
        with self.assertRaises(TimeoutError):
            http_request(
                "https://192.0.2.1:443/test",
                method="GET",
                headers={},
                data=None,
                timeout=2,
            )

    def test_http_request_raises_connection_error_for_refused(self):
        """http_request should raise ConnectionError for connection refused."""
        # localhost:59999 should refuse connection
        with self.assertRaises(ConnectionError):
            http_request(
                "http://127.0.0.1:59999/",
                method="GET",
                headers={},
                data=None,
                timeout=2,
            )

    def test_http_request_invalid_method(self):
        """http_request should raise ValueError for invalid HTTP method."""
        with self.assertRaises(ValueError):
            http_request(
                "http://127.0.0.1:80/",
                method="INVALID",
                headers={},
                data=None,
                timeout=2,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
