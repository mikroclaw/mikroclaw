#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

run_docker_build() {
    if ! command -v docker >/dev/null 2>&1; then
        echo "Error: docker not available for musl fallback build"
        exit 1
    fi
    docker build -f Dockerfile.musl -t mikroclaw-musl-build .
    docker run --rm -v "$PROJECT_ROOT:/build/mikroclaw" -w /build/mikroclaw mikroclaw-musl-build make static-musl
}

if [ -f /.dockerenv ]; then
    CC_BIN="gcc"
elif command -v musl-gcc >/dev/null 2>&1; then
    CC_BIN="musl-gcc"
else
    run_docker_build
    exit 0
fi

MAKE_ARGS=("CC=$CC_BIN")
if [ "${USE_CUSTOM_MBEDTLS:-0}" = "1" ]; then
    if [ "${FORCE_REBUILD_CUSTOM_MBEDTLS:-0}" = "1" ] || [ ! -f "$PROJECT_ROOT/third_party/mbedtls-musl/lib/libmbedtls.a" ]; then
        CC="$CC_BIN" ./scripts/build-mbedtls.sh "$PROJECT_ROOT/third_party/mbedtls-musl"
    fi
    MAKE_ARGS+=("USE_CUSTOM_MBEDTLS=1" "CUSTOM_MBEDTLS_PREFIX=$PROJECT_ROOT/third_party/mbedtls-musl")
fi

make clean
if ! make "${MAKE_ARGS[@]}" static-musl; then
    if [ "$CC_BIN" = "musl-gcc" ]; then
        run_docker_build
        exit 0
    fi
    exit 1
fi

file ./mikroclaw-static-musl
ldd ./mikroclaw-static-musl 2>&1 || true
ls -lh ./mikroclaw-static-musl
