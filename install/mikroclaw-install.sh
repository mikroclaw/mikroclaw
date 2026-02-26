#!/bin/sh
# MikroClaw Bootstrap - Thin shell wrapper for Python installer
# Requires: Python 3.8+

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Check for Python 3
if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: Python 3 is required but not found." >&2
    echo "" >&2
    echo "Install Python 3.8+ using your package manager:" >&2
    echo "  apt:     sudo apt-get install python3" >&2
    echo "  dnf:     sudo dnf install python3" >&2
    echo "  pacman:  sudo pacman -S python" >&2
    echo "  brew:    brew install python@3" >&2
    echo "" >&2
    echo "Or download from: https://www.python.org/downloads/" >&2
    exit 1
fi

# Check Python version (>= 3.8)
PY_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null || echo "0.0")
PY_MAJOR=$(echo "$PY_VERSION" | cut -d. -f1)
PY_MINOR=$(echo "$PY_VERSION" | cut -d. -f2)

if [ "$PY_MAJOR" -lt 3 ] || { [ "$PY_MAJOR" -eq 3 ] && [ "$PY_MINOR" -lt 8 ]; }; then
    echo "Error: Python 3.8+ is required (found: $PY_VERSION)." >&2
    echo "" >&2
    echo "Upgrade Python using your package manager:" >&2
    echo "  apt:     sudo apt-get install python3" >&2
    echo "  dnf:     sudo dnf install python3" >&2
    echo "  pacman:  sudo pacman -S python" >&2
    echo "  brew:    brew install python@3" >&2
    exit 1
fi

# Delegate to Python installer
exec python3 "$SCRIPT_DIR/mikroclaw-install.py" "$@"
