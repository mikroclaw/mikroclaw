#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-v0.3.0}"
OUT_DIR="release-${VERSION}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

make clean && make
./scripts/verify-release.sh
make mikrotik-docker
make

cp mikroclaw "$OUT_DIR/"
cp mikrotik-docker "$OUT_DIR/"
cp LICENSE "$OUT_DIR/"
cp README.md "$OUT_DIR/"
cp CHANGELOG.md "$OUT_DIR/"

if [ -f mikroclaw-static-musl ]; then
  cp mikroclaw-static-musl "$OUT_DIR/"
fi

sha256sum "$OUT_DIR"/* > "$OUT_DIR/SHA256SUMS.txt"
tar -czf "${OUT_DIR}.tar.gz" "$OUT_DIR"

echo "Release artifacts created: ${OUT_DIR}.tar.gz"
echo "Checksums: ${OUT_DIR}/SHA256SUMS.txt"
