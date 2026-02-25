# MikroClaw Build System
# Standard Makefile for RouterOS/Linux builds

CC = gcc

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

# Linker flags
LDFLAGS = -Wl,--gc-sections -Wl,--strip-all

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

.PHONY: all clean size test install static-mbedtls mbedtls-minimal mikroclaw-minimal static-musl mikroclaw-static-musl mikrotik-docker cppcheck analyze

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

test: $(TARGET)
	@echo "Running tests..."
	./test_tls 2>/dev/null && echo "PASS TLS test" || echo "FAIL TLS test"
	./test_json_escape 2>/dev/null && echo "PASS JSON escape test" || echo "FAIL JSON escape test"

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
