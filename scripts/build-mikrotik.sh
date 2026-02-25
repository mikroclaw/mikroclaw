#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== MikroClaw RouterOS Build ==="
make clean
make mikrotik-docker

LDD_OUT="$(ldd ./mikrotik-docker 2>&1 || true)"
if echo "$LDD_OUT" | grep -Eiq "not a dynamic executable|not a valid dynamic program"; then
    echo "Static binary verified"
else
    echo "Build failed: mikrotik-docker is not static"
    exit 1
fi

SIZE_BYTES=$(stat -c%s ./mikrotik-docker 2>/dev/null || stat -f%z ./mikrotik-docker)
echo "mikrotik-docker size: ${SIZE_BYTES} bytes"

tar -czf mikroclaw-routeros.tar.gz mikrotik-docker install/mikroclaw-install.sh
echo "Package created: mikroclaw-routeros.tar.gz"
