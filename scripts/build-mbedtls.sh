#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_PREFIX="${1:-$PROJECT_ROOT/third_party/mbedtls}"
BUILD_DIR="$PROJECT_ROOT/build/mbedtls"

if [ -n "${MBEDTLS_SRC:-}" ] && [ -d "${MBEDTLS_SRC}" ]; then
    SRC_DIR="${MBEDTLS_SRC}"
elif [ -d "/usr/src/mbedtls" ]; then
    SRC_DIR="/usr/src/mbedtls"
elif [ -d "$PROJECT_ROOT/vendor/mbedtls" ]; then
    SRC_DIR="$PROJECT_ROOT/vendor/mbedtls"
else
    echo "Error: mbedTLS source not found"
    echo "Set MBEDTLS_SRC or install source to /usr/src/mbedtls or $PROJECT_ROOT/vendor/mbedtls"
    exit 1
fi

echo "Building mbedTLS from $SRC_DIR"
echo "Install prefix: $INSTALL_PREFIX"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_PREFIX/lib" "$INSTALL_PREFIX/include"

cp "$PROJECT_ROOT/vendor/mbedtls_config_mikroclaw.h" "$BUILD_DIR/mbedtls_config.h"

NPROC=1
if command -v nproc >/dev/null 2>&1; then
    NPROC="$(nproc)"
fi

make -C "$SRC_DIR" clean >/dev/null 2>&1 || true

CFLAGS_BASE="-Os -ffunction-sections -fdata-sections -fPIC -I$BUILD_DIR -DMBEDTLS_CONFIG_FILE='\"mbedtls_config.h\"'"

if [ -n "${CC:-}" ]; then
    make -C "$SRC_DIR" lib CC="$CC" CFLAGS="$CFLAGS_BASE" -j"$NPROC"
else
    make -C "$SRC_DIR" lib CFLAGS="$CFLAGS_BASE" -j"$NPROC"
fi

cp "$SRC_DIR/library/libmbedcrypto.a" "$INSTALL_PREFIX/lib/"
cp "$SRC_DIR/library/libmbedx509.a" "$INSTALL_PREFIX/lib/"
cp "$SRC_DIR/library/libmbedtls.a" "$INSTALL_PREFIX/lib/"
cp -r "$SRC_DIR/include/mbedtls" "$INSTALL_PREFIX/include/"

echo "Built libraries:"
ls -lh "$INSTALL_PREFIX/lib/"*.a
