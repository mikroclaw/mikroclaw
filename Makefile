# MikroClaw Build System
# Standard Makefile for RouterOS/Linux builds

CC = gcc

SANITIZE ?= 0
COVERAGE ?= 0
WERROR ?= 0

# mbedTLS - custom minimal build or system
USE_CUSTOM_MBEDTLS ?= 0
CUSTOM_MBEDTLS_PREFIX ?= third_party/mbedtls

ifeq ($(USE_CUSTOM_MBEDTLS),1)
MBEDTLS_CFLAGS = -I$(CUSTOM_MBEDTLS_PREFIX)/include -Ivendor -DMBEDTLS_CONFIG_FILE='"mbedtls_config_mikroclaw.h"'
MBEDTLS_LIBS = -L$(CUSTOM_MBEDTLS_PREFIX)/lib -lmbedtls -lmbedx509 -lmbedcrypto
else
MBEDTLS_CFLAGS =
MBEDTLS_LIBS = -lmbedtls -lmbedx509 -lmbedcrypto
endif

CURL_CFLAGS =
CURL_LIBS = -lcurl

CFLAGS = -Os -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS += -fmerge-all-constants -fno-stack-protector
CFLAGS += -I. -Isrc -Ivendor $(MBEDTLS_CFLAGS) $(CURL_CFLAGS)

ifeq ($(SANITIZE),1)
SANITIZE_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
else
SANITIZE_FLAGS =
endif

ifeq ($(COVERAGE),1)
COVERAGE_FLAGS = --coverage
else
COVERAGE_FLAGS =
endif

CFLAGS += $(SANITIZE_FLAGS) $(COVERAGE_FLAGS)

ifeq ($(WERROR),1)
CFLAGS += -Werror -Wformat -Wformat-security
endif

# Linker flags
LDFLAGS = -Wl,--gc-sections -Wl,--strip-all
LDFLAGS += $(SANITIZE_FLAGS) $(COVERAGE_FLAGS)

# mbedTLS libraries
LDLIBS = $(MBEDTLS_LIBS) $(CURL_LIBS)

# Feature flags
FLAGS = \
    -DCHANNEL_TELEGRAM=1 \
    -DCHANNEL_DISCORD=1 \
    -DCHANNEL_SLACK=1 \
    -DUSE_MEMU_CLOUD=1 \
    -DSTORAGE_LOCAL_ENABLED=1 \
    -DENABLE_GATEWAY=1 \
    -DENABLE_HEARTBEAT=1

# Source files
SRCS = \
    vendor/jsmn.c \
    vendor/mbedtls_integration.c \
    src/http.c \
    src/json.c \
    src/storage_local.c \
    src/memu_client.c \
    src/memu_client_stub.c \
    src/config_memu.c \
	src/config_validate.c \
	src/functions.c \
	src/buf.c \
	src/http_client.c \
    src/base64.c \
    src/crypto.c \
    src/routeros.c \
    src/llm.c \
    src/llm_stream.c \
    src/provider_registry.c \
    src/identity.c \
    src/log.c \
    src/cli.c \
    src/channels/telegram.c \
    src/channels/allowlist.c \
    src/channels/discord.c \
    src/channels/slack.c \
    src/gateway.c \
    src/gateway_auth.c \
    src/rate_limit.c \
    src/channel_supervisor.c \
    src/task_queue.c \
    src/task_handlers.c \
    src/worker_pool.c \
    src/subagent.c \
    src/tasks/investigate.c \
    src/tasks/analyze.c \
    src/tasks/summarize.c \
    src/tasks/skill_invoke.c \
    src/cron.c \
    src/mikroclaw.c \
    src/main.c

TARGET = mikroclaw

.PHONY: all clean size test test-sanitize coverage install static-mbedtls mbedtls-minimal mikroclaw-minimal static-musl mikroclaw-static-musl mikrotik-docker cppcheck analyze

all: $(TARGET) size

$(TARGET): $(SRCS)
	@echo "Building MikroClaw..."
	$(CC) $(CFLAGS) $(FLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET)-static-mbedtls $(TARGET)-static-musl

size: $(TARGET)
	@echo "=== MikroClaw Binary Size ==="
	@ls -lh $(TARGET)
	@echo ""
	@size $(TARGET) 2>/dev/null || echo "size command not available"

TEST_BINARIES = \
	test_tls \
	test_json_escape \
	test_buf \
	test_tls_verify \
	test_base64 \
	test_routeros_auth \
	test_json_hardening \
	test_telegram_parse \
	test_storage_local_path \
	test_discord \
	test_slack \
	test_discord_inbound \
	test_slack_inbound \
	test_functions \
	test_memu_client \
	test_config_memu \
	test_gateway_auth \
	test_rate_limit \
	test_gateway_port \
	test_task_queue \
	test_subagent \
	test_cli \
	test_config_validate \
	test_crypto \
	test_identity \
	test_channel_supervisor \
	test_provider_registry \
	test_llm_stream \
	test_allowlist \
	test_schema \
	test_tool_security

TEST_SRCS_test_tls = tests/test_tls.c src/http.c src/json.c src/base64.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_json_escape = tests/test_json_escape.c src/json.c src/base64.c vendor/jsmn.c
TEST_SRCS_test_buf = tests/test_buf.c src/buf.c
TEST_SRCS_test_tls_verify = tests/test_tls_verify.c vendor/mbedtls_integration.c src/http.c src/json.c vendor/jsmn.c
TEST_SRCS_test_base64 = tests/test_base64.c src/base64.c
TEST_SRCS_test_routeros_auth = tests/test_routeros_auth.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_json_hardening = tests/test_json_hardening.c src/channels/telegram.c src/channels/allowlist.c src/cron.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_telegram_parse = tests/test_telegram_parse.c src/channels/telegram.c src/channels/allowlist.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_storage_local_path = tests/test_storage_local_path.c src/storage_local.c
TEST_SRCS_test_discord = tests/test_discord.c src/channels/discord.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_slack = tests/test_slack.c src/channels/slack.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_discord_inbound = tests/test_discord_inbound.c src/channels/discord.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_slack_inbound = tests/test_slack_inbound.c src/channels/slack.c src/channels/allowlist.c src/http_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_functions = tests/test_functions.c src/functions.c src/buf.c src/memu_client.c src/routeros.c src/http.c src/http_client.c src/json.c src/base64.c src/channels/allowlist.c src/llm_stream.c src/provider_registry.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_memu_client = tests/test_memu_client.c src/memu_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_config_memu = tests/test_config_memu.c src/config_memu.c src/memu_client.c src/json.c vendor/jsmn.c
TEST_SRCS_test_gateway_auth = tests/test_gateway_auth.c src/gateway_auth.c vendor/mbedtls_integration.c
TEST_SRCS_test_rate_limit = tests/test_rate_limit.c src/rate_limit.c
TEST_SRCS_test_gateway_port = tests/test_gateway_port.c src/gateway.c
TEST_SRCS_test_task_queue = tests/test_task_queue.c src/task_queue.c
TEST_SRCS_test_subagent = tests/test_subagent.c src/subagent.c src/worker_pool.c src/task_queue.c src/task_handlers.c src/tasks/investigate.c src/tasks/analyze.c src/tasks/summarize.c src/tasks/skill_invoke.c src/memu_client_stub.c src/routeros.c src/llm.c src/llm_stream.c src/provider_registry.c src/http.c src/json.c src/base64.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_cli = tests/test_cli.c src/cli.c
TEST_SRCS_test_config_validate = tests/test_config_validate.c src/config_validate.c
TEST_SRCS_test_crypto = tests/test_crypto.c src/crypto.c
TEST_SRCS_test_identity = tests/test_identity.c src/identity.c src/memu_client.c src/json.c src/http_client.c src/base64.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_channel_supervisor = tests/test_channel_supervisor.c src/channel_supervisor.c
TEST_SRCS_test_provider_registry = tests/test_provider_registry.c src/provider_registry.c
TEST_SRCS_test_llm_stream = tests/test_llm_stream.c src/llm_stream.c
TEST_SRCS_test_allowlist = tests/test_allowlist.c src/channels/allowlist.c
TEST_SRCS_test_schema = tests/test_schema.c src/functions.c src/buf.c src/memu_client_stub.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c
TEST_SRCS_test_tool_security = tests/test_tool_security.c src/functions.c src/buf.c src/memu_client_stub.c src/routeros.c src/base64.c src/http.c src/json.c vendor/jsmn.c vendor/mbedtls_integration.c

TEST_DEFS_test_schema = -DDISABLE_WEB_SEARCH -UUSE_MEMU_CLOUD
TEST_DEFS_test_tool_security = -DDISABLE_WEB_SEARCH -UUSE_MEMU_CLOUD
TEST_DEFS_test_subagent = -UUSE_MEMU_CLOUD

TEST_LIBS_test_tls = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_tls_verify = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_routeros_auth = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_gateway_auth = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_json_hardening = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_telegram_parse = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_discord = -lcurl
TEST_LIBS_test_slack = -lcurl
TEST_LIBS_test_discord_inbound = -lcurl
TEST_LIBS_test_slack_inbound = -lcurl
TEST_LIBS_test_functions = -lcurl -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_memu_client = -lcurl
TEST_LIBS_test_config_memu = -lcurl
TEST_LIBS_test_identity = -lcurl -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_subagent = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_schema = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_tool_security = -lmbedtls -lmbedx509 -lmbedcrypto
TEST_LIBS_test_crypto = -lmbedtls -lmbedx509 -lmbedcrypto

TEST_RUN_ENV_test_functions = MEMU_MOCK_RETRIEVE_TEXT=v
TEST_RUN_ENV_test_crypto = MEMU_ENCRYPTION_KEY=test-key
TEST_RUN_ENV_test_identity = MEMU_MOCK_RETRIEVE_TEXT=id

TEST_TIMEOUT_SECONDS ?= 120

define RUN_TEST_CASE
	echo "Compiling $(1)..."; \
	$(CC) $(CFLAGS) $(FLAGS) $(TEST_DEFS_$(1)) -I. -Isrc $(TEST_SRCS_$(1)) -o $(1) $(TEST_LIBS_$(1)); \
	if [ $$? -ne 0 ]; then \
		echo "FAIL [compile]: $(1)"; \
		failed=$$((failed + 1)); \
	else \
		echo "PASS [compile]: $(1)"; \
		$(TEST_RUN_ENV_$(1)) timeout $(TEST_TIMEOUT_SECONDS) ./$(1) >/dev/null 2>&1; \
		if [ $$? -ne 0 ]; then \
			echo "FAIL [run]: $(1)"; \
			failed=$$((failed + 1)); \
		else \
			echo "PASS [run]: $(1)"; \
		fi; \
	fi

endef

test: $(TARGET)
	@failed=0; \
	$(foreach test,$(TEST_BINARIES),$(call RUN_TEST_CASE,$(test))) \
	if [ "$${failed:-0}" -eq 0 ]; then \
		echo "All tests passed"; \
		exit 0; \
	else \
		echo "Tests failed: $${failed:-0}"; \
		exit 1; \
	fi

test-sanitize: SANITIZE=1
test-sanitize: test

coverage: COVERAGE=1
coverage: clean
	@mkdir -p coverage_html
	@$(MAKE) COVERAGE=1 test
	@if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then \
		lcov --capture --directory . --output-file coverage.info >/dev/null 2>&1; \
		genhtml coverage.info --output-directory coverage_html >/dev/null 2>&1; \
		echo "Coverage report generated at coverage_html/index.html"; \
	else \
		echo "lcov/genhtml not available; raw gcov artifacts generated"; \
	fi

cppcheck:
	@mkdir -p build
	@cppcheck --enable=all --inconclusive --suppress=missingInclude \
		--xml --xml-version=2 $(SRCS) 2> build/cppcheck-report.xml
	@cppcheck --enable=all --inconclusive --suppress=missingInclude \
		$(SRCS) 2>&1 | tee build/cppcheck-report.txt
	@echo "cppcheck complete -- see build/cppcheck-report.txt"

analyze:
	@mkdir -p build/analyze
	@scan-build -o build/analyze $(MAKE) clean all CC=clang 2>&1 | tee build/analyze-report.txt
	@echo "scan-build complete -- see build/analyze/"

install: $(TARGET)
	@echo "Installing to /usr/local/bin..."
	@cp $(TARGET) /usr/local/bin/
	@chmod +x /usr/local/bin/$(TARGET)
	@echo "Installed"

static-mbedtls:
	@echo "Building with static mbedTLS libs..."
	$(CC) $(CFLAGS) $(FLAGS) -o $(TARGET)-static-mbedtls $(SRCS) \
		-Wl,-Bstatic -lmbedtls -lmbedx509 -lmbedcrypto \
		-Wl,-Bdynamic -lc -lpthread -Wl,--gc-sections -Wl,--strip-all
	@echo "Built: $(TARGET)-static-mbedtls"

# Build custom minimal mbedTLS
mbedtls-minimal:
	@echo "Building custom minimal mbedTLS..."
	USE_CUSTOM_MBEDTLS=1 ./scripts/build-mbedtls.sh "$(CUSTOM_MBEDTLS_PREFIX)"

# Build with custom mbedTLS
mikroclaw-minimal: USE_CUSTOM_MBEDTLS=1
mikroclaw-minimal: CFLAGS += -Os -flto
mikroclaw-minimal: LDFLAGS += -Wl,--gc-sections -flto
mikroclaw-minimal: $(TARGET)
	@echo "=== MikroClaw with custom mbedTLS ==="
	@ls -lh $(TARGET)
	@size $(TARGET) 2>/dev/null || true

# Fully static musl build
static-musl: clean
	@echo "Building fully static musl binary..."
	$(CC) $(CFLAGS) -DDISABLE_WEB_SEARCH=1 -flto -fno-unwind-tables -fno-asynchronous-unwind-tables -static $(FLAGS) -UCHANNEL_DISCORD -UCHANNEL_SLACK -UUSE_MEMU_CLOUD -o $(TARGET)-static-musl $(filter-out src/http_client.c src/channels/discord.c src/channels/slack.c src/memu_client.c src/config_memu.c,$(SRCS)) \
		$(filter-out -lcurl,$(LDLIBS)) $(LDFLAGS) -flto -static
	@echo "=== Fully Static Binary ==="
	@file $(TARGET)-static-musl
	@ls -lh $(TARGET)-static-musl
	@echo "Verification (should show 'not a dynamic executable'):"
	@ldd $(TARGET)-static-musl 2>&1 || true

mikroclaw-static-musl: static-musl

mikrotik-docker: clean
	@echo "Building RouterOS container binary..."
	$(CC) $(CFLAGS) -DROUTEROS_CONTAINER=1 -DDISABLE_WEB_SEARCH=1 $(FLAGS) -UCHANNEL_DISCORD -UCHANNEL_SLACK -UUSE_MEMU_CLOUD -o $@ $(filter-out src/http_client.c src/channels/discord.c src/channels/slack.c src/memu_client.c src/config_memu.c,$(SRCS)) $(filter-out -lcurl,$(LDLIBS)) $(LDFLAGS) -static
	@ls -lh $@
