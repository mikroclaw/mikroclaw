#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

ERRORS=0
PASS=0

test_start() {
    printf "Testing %s... " "$1"
}

test_pass() {
    echo "PASS"
    PASS=$((PASS + 1))
}

test_fail() {
    echo "FAIL: $1"
    ERRORS=$((ERRORS + 1))
}

build_test_binary() {
    local out="$1"
    shift
    gcc -I. -Isrc "$@" -o "$out"
}

echo "=== MikroClaw Release Verification ==="
echo "Date: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
echo "Git: $(git rev-parse --short HEAD 2>/dev/null || echo 'not-a-git-repo')"
echo

test_start "clean build with zero warnings"
make clean >/dev/null 2>&1
BUILD_LOG="$(mktemp)"
if make >"$BUILD_LOG" 2>&1; then
    WARNINGS=$(grep -i warning "$BUILD_LOG" 2>/dev/null | wc -l || true)
    if [ "$WARNINGS" -eq 0 ]; then
        test_pass
    else
        test_fail "$WARNINGS warnings found"
    fi
else
    test_fail "build failed"
fi
rm -f "$BUILD_LOG"

test_start "binary exists and executable"
if [ -x "./mikroclaw" ]; then
    test_pass
else
    test_fail "./mikroclaw missing"
fi

test_start "binary size <96KB"
SIZE=$(stat -c%s ./mikroclaw 2>/dev/null || stat -f%z ./mikroclaw)
if [ "$SIZE" -lt 98304 ]; then
    test_pass
else
    test_fail "size is $SIZE bytes"
fi

build_test_binary tests/test_tls_verify tests/test_tls_verify.c vendor/mbedtls_integration.c src/http.c src/json.c vendor/jsmn.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_base64 tests/test_base64.c src/base64.c
build_test_binary tests/test_routeros_auth tests/test_routeros_auth.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_json_hardening tests/test_json_hardening.c src/channels/telegram.c src/channels/allowlist.c src/cron.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_telegram_parse tests/test_telegram_parse.c src/channels/telegram.c src/channels/allowlist.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_storage_local_path tests/test_storage_local_path.c src/storage_local.c
build_test_binary tests/test_discord tests/test_discord.c src/channels/discord.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_slack tests/test_slack.c src/channels/slack.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_discord_inbound tests/test_discord_inbound.c src/channels/discord.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_slack_inbound tests/test_slack_inbound.c src/channels/slack.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_functions tests/test_functions.c src/functions.c src/buf.c src/memu_client.c src/routeros.c src/http.c src/http_client.c src/json.c src/base64.c src/channels/allowlist.c src/llm_stream.c src/provider_registry.c vendor/jsmn.c vendor/mbedtls_integration.c -lcurl -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_memu_client tests/test_memu_client.c src/memu_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_config_memu tests/test_config_memu.c src/config_memu.c src/memu_client.c src/json.c vendor/jsmn.c -lcurl
build_test_binary tests/test_gateway_auth tests/test_gateway_auth.c src/gateway_auth.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_rate_limit tests/test_rate_limit.c src/rate_limit.c
build_test_binary tests/test_gateway_port tests/test_gateway_port.c src/gateway.c
build_test_binary tests/test_task_queue tests/test_task_queue.c src/task_queue.c
build_test_binary tests/test_subagent tests/test_subagent.c src/subagent.c src/worker_pool.c src/task_queue.c src/task_handlers.c src/tasks/investigate.c src/tasks/analyze.c src/tasks/summarize.c src/tasks/skill_invoke.c src/memu_client_stub.c src/routeros.c src/llm.c src/llm_stream.c src/provider_registry.c src/http.c src/json.c src/base64.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_cli tests/test_cli.c src/cli.c
build_test_binary tests/test_config_validate tests/test_config_validate.c src/config_validate.c
build_test_binary tests/test_crypto tests/test_crypto.c src/crypto.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_identity tests/test_identity.c src/identity.c src/memu_client.c src/json.c src/http_client.c vendor/jsmn.c -lcurl -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_channel_supervisor tests/test_channel_supervisor.c src/channel_supervisor.c
build_test_binary tests/test_provider_registry tests/test_provider_registry.c src/provider_registry.c
build_test_binary tests/test_llm_stream tests/test_llm_stream.c src/llm_stream.c
build_test_binary tests/test_allowlist tests/test_allowlist.c src/channels/allowlist.c
build_test_binary tests/test_schema -DDISABLE_WEB_SEARCH tests/test_schema.c src/functions.c src/buf.c src/memu_client_stub.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto
build_test_binary tests/test_tool_security -DDISABLE_WEB_SEARCH tests/test_tool_security.c src/functions.c src/buf.c src/memu_client_stub.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c -lmbedtls -lmbedx509 -lmbedcrypto

test_start "TLS verification test"
if ./tests/test_tls_verify >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_tls_verify failed"
fi

test_start "base64 encoding test"
if ./tests/test_base64 >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_base64 failed"
fi

test_start "RouterOS auth test"
if ./tests/test_routeros_auth >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_routeros_auth failed"
fi

test_start "JSON injection hardening"
if ./tests/test_json_hardening >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_json_hardening failed"
fi

test_start "Telegram message parsing"
if ./tests/test_telegram_parse >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_telegram_parse failed"
fi

test_start "storage path traversal protection"
if ./tests/test_storage_local_path >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_storage_local_path failed"
fi

test_start "Discord webhook module"
if ./tests/test_discord >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_discord failed"
fi

test_start "Slack webhook module"
if ./tests/test_slack >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_slack failed"
fi

test_start "Discord inbound parsing"
if ./tests/test_discord_inbound >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_discord_inbound failed"
fi

test_start "Slack inbound parsing"
if ./tests/test_slack_inbound >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_slack_inbound failed"
fi

test_start "functions registry"
if MEMU_MOCK_RETRIEVE_TEXT=v ./tests/test_functions >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_functions failed"
fi

test_start "memU client module"
if ./tests/test_memu_client >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_memu_client failed"
fi

test_start "memU boot config rehydration"
if ./tests/test_config_memu >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_config_memu failed"
fi

test_start "gateway pairing auth"
if ./tests/test_gateway_auth >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_gateway_auth failed"
fi

test_start "gateway rate limit"
if ./tests/test_rate_limit >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_rate_limit failed"
fi

test_start "gateway port randomization"
if ./tests/test_gateway_port >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_gateway_port failed"
fi

test_start "task queue module"
if ./tests/test_task_queue >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_task_queue failed"
fi

test_start "subagent worker flow"
if ./tests/test_subagent >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_subagent failed"
fi

test_start "cli mode parsing"
if ./tests/test_cli >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_cli failed"
fi

test_start "config validation"
if ./tests/test_config_validate >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_config_validate failed"
fi

test_start "encrypted secrets crypto"
if MEMU_ENCRYPTION_KEY=test-key ./tests/test_crypto >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_crypto failed"
fi

test_start "identity"
if MEMU_MOCK_RETRIEVE_TEXT=id ./tests/test_identity >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_identity failed"
fi

test_start "channel supervisor backoff"
if ./tests/test_channel_supervisor >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_channel_supervisor failed"
fi

test_start "provider registry"
if ./tests/test_provider_registry >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_provider_registry failed"
fi

test_start "llm stream parser"
if ./tests/test_llm_stream >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_llm_stream failed"
fi

test_start "channel allowlist"
if ./tests/test_allowlist >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_allowlist failed"
fi

test_start "tool JSON schema"
if ./tests/test_schema >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_schema failed"
fi

test_start "tool security policy"
if ./tests/test_tool_security >/dev/null 2>&1; then
    test_pass
else
    test_fail "test_tool_security failed"
fi

test_start "status command"
if ./mikroclaw status >/dev/null 2>&1; then
    test_pass
else
    test_fail "status command failed"
fi

test_start "doctor command"
if ./mikroclaw doctor >/dev/null 2>&1; then
    test_pass
else
    test_fail "doctor command failed"
fi

test_start "channel command"
if ./mikroclaw channel >/dev/null 2>&1; then
    test_pass
else
    test_fail "channel command failed"
fi

test_start "config command"
if ./mikroclaw config --dump >/dev/null 2>&1; then
    test_pass
else
    test_fail "config command failed"
fi

test_start "integrations command"
if ./mikroclaw integrations >/dev/null 2>&1; then
    test_pass
else
    test_fail "integrations command failed"
fi

test_start "identity command"
if ./mikroclaw identity >/dev/null 2>&1; then
    test_pass
else
    test_fail "identity command failed"
fi

test_start "encrypt command"
if MEMU_ENCRYPTION_KEY=test-key ./mikroclaw encrypt OPENROUTER_KEY=abc >/dev/null 2>&1; then
    test_pass
else
    test_fail "encrypt command failed"
fi

test_start "static musl build"
STATIC_BUILD_CMD="./scripts/build-musl.sh"
if [ -d "$PROJECT_ROOT/third_party/mbedtls-musl/lib" ]; then
    STATIC_BUILD_CMD="USE_CUSTOM_MBEDTLS=1 ./scripts/build-musl.sh"
elif [ -d "/tmp/mbedtls" ]; then
    STATIC_BUILD_CMD="USE_CUSTOM_MBEDTLS=1 MBEDTLS_SRC=/tmp/mbedtls ./scripts/build-musl.sh"
fi

if bash -lc "$STATIC_BUILD_CMD" >/tmp/mikroclaw_static_build.log 2>&1; then
    test_pass
    test_start "verify fully static binary"
    LDD_OUT="$(ldd ./mikroclaw-static-musl 2>&1 || true)"
    if echo "$LDD_OUT" | grep -Eiq "not a dynamic executable|not a valid dynamic program"; then
        test_pass
    else
        test_fail "binary is not fully static"
    fi
    test_start "static binary size <640KB"
    STATIC_SIZE=$(stat -c%s ./mikroclaw-static-musl 2>/dev/null || stat -f%z ./mikroclaw-static-musl)
    if [ "$STATIC_SIZE" -lt 655360 ]; then
        test_pass
    else
        test_fail "static size is $STATIC_SIZE bytes"
    fi
else
    test_fail "static musl build failed"
fi

test_start "binary file type"
if [ ! -x "./mikroclaw" ]; then
    make >/dev/null 2>&1 || true
fi
if file ./mikroclaw | grep -q "ELF"; then
    test_pass
else
    test_fail "unexpected file type"
fi

echo
echo "=== Verification Summary ==="
echo "Passed: $PASS"
echo "Failed: $ERRORS"

if [ "$ERRORS" -eq 0 ]; then
    echo "ALL CHECKS PASSED"
    ls -lh ./mikroclaw ./mikroclaw-static-musl 2>/dev/null || ls -lh ./mikroclaw
    exit 0
fi

echo "VERIFICATION FAILED"
exit 1
