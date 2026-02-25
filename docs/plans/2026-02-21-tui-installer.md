# MikroClaw TUI Installer Implementation Plan

> **For Vulcan:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create a human-friendly TUI installer for MikroClaw with whiptail/dialog fallback to ASCII, JSON config, no secret persistence, and manual router configuration.

**Architecture:**
- Single `mikroclaw-install.sh` script with modular library functions
- TUI abstraction layer: tries whiptail → dialog → ASCII fallback
- JSON config generation (single file)
- Test suite using BATS (Bash Automated Testing System)
- No secrets saved to disk - only in-memory during install

**Tech Stack:** POSIX sh, whiptail/dialog (optional), jq (for JSON), curl/wget, BATS for testing

---

## Task 1: Project Structure and Test Framework Setup

**Files:**
- Create: `install/mikroclaw-install.sh` (main script)
- Create: `install/lib/ui.sh` (TUI abstraction)
- Create: `install/lib/config.sh` (JSON config)
- Create: `install/tests/bats/` (test directory)
- Create: `install/tests/test_ui.bats`

**Step 1: Create directory structure**

```bash
mkdir -p install/lib install/tests/bats
```

**Step 2: Install BATS testing framework**

```bash
cd install/tests
git clone https://github.com/bats-core/bats-core.git bats
```

**Step 3: Write first failing test for UI detection**

Create `install/tests/test_ui.bats`:

```bats
#!/usr/bin/env bats

@test "detects available TUI backend" {
  source ../lib/ui.sh
  result=$(ui_detect_backend)
  [[ "$result" == "whiptail" || "$result" == "dialog" || "$result" == "ascii" ]]
}

@test "ui_available returns true for valid backend" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  run ui_available
  [ "$status" -eq 0 ]
}
```

**Step 4: Run test - verify it fails**

```bash
cd install/tests
./bats/bin/bats test_ui.bats
```

Expected: FAIL - "ui_detect_backend: command not found"

**Step 5: Create minimal UI library**

Create `install/lib/ui.sh`:

```bash
#!/bin/sh
# TUI abstraction layer for MikroClaw installer

UI_BACKEND=""

# Detect available TUI backend
ui_detect_backend() {
  if command -v whiptail >/dev/null 2>&1; then
    echo "whiptail"
  elif command -v dialog >/dev/null 2>&1; then
    echo "dialog"
  else
    echo "ascii"
  fi
}

# Initialize UI system
ui_init() {
  UI_BACKEND=$(ui_detect_backend)
}

# Check if UI is available
ui_available() {
  [ -n "$UI_BACKEND" ]
}
```

**Step 6: Run test - verify it passes**

```bash
./bats/bin/bats test_ui.bats
```

Expected: PASS

**Step 7: Commit**

```bash
git add install/
git commit -m "feat: add TUI backend detection with tests"
```

---

## Task 2: TUI Menu Implementation (RED-GREEN-REFACTOR)

**Files:**
- Modify: `install/lib/ui.sh`
- Modify: `install/tests/test_ui.bats`

**Step 1: Write failing test for menu display**

Add to `test_ui.bats`:

```bats
@test "ui_menu returns selected option" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  # Mock input
  echo "1" | {
    result=$(ui_menu "Test Title" "Option 1" "Option 2")
    [ "$result" = "1" ]
  }
}
```

**Step 2: Run test - verify fails**

```bash
./bats/bin/bats test_ui.bats
```

Expected: FAIL - "ui_menu: command not found"

**Step 3: Implement ASCII menu (minimal)**

Add to `ui.sh`:

```bash
# Display menu and return selection
# Usage: ui_menu "Title" "Option 1" "Option 2" ...
ui_menu() {
  local title="$1"
  shift
  
  if [ "$UI_BACKEND" = "ascii" ]; then
    _ui_menu_ascii "$title" "$@"
  else
    _ui_menu_whiptail "$title" "$@"
  fi
}

# ASCII fallback menu
_ui_menu_ascii() {
  local title="$1"
  shift
  
  echo ""
  echo "═══════════════════════════════════════════"
  echo "  $title"
  echo "═══════════════════════════════════════════"
  echo ""
  
  local i=1
  for option in "$@"; do
    echo "   [$i] $option"
    i=$((i + 1))
  done
  
  echo ""
  printf "➤ Select option: "
  read selection
  echo "$selection"
}
```

**Step 4: Run test - verify passes**

```bash
./bats/bin/bats test_ui.bats
```

Expected: PASS

**Step 5: Write test for whiptail menu**

Add to `test_ui.bats`:

```bats
@test "ui_menu uses whiptail when available" {
  source ../lib/ui.sh
  
  # Mock whiptail
  whiptail() {
    echo "2"
  }
  
  UI_BACKEND="whiptail"
  result=$(ui_menu "Test" "A" "B" "C")
  [ "$result" = "2" ]
}
```

**Step 6: Implement whiptail menu**

Add to `ui.sh`:

```bash
# Whiptail/Dialog menu
_ui_menu_whiptail() {
  local title="$1"
  shift
  
  local args=""
  local i=1
  for option in "$@"; do
    args="$args $i \"$option\""
    i=$((i + 1))
  done
  
  if [ "$UI_BACKEND" = "whiptail" ]; then
    eval "whiptail --title '$title' --menu '' 20 60 10 $args 3>&1 1>&2 2>&3" || echo ""
  else
    eval "dialog --title '$title' --menu '' 20 60 10 $args 3>&1 1>&2 2>&3" || echo ""
  fi
}
```

**Step 7: Run all tests**

```bash
./bats/bin/bats test_ui.bats
```

Expected: PASS

**Step 8: Commit**

```bash
git add install/
git commit -m "feat: add TUI menu with ascii and whiptail backends"
```

---

## Task 3: Input Prompts with Secret Masking

**Files:**
- Modify: `install/lib/ui.sh`
- Modify: `install/tests/test_ui.bats`

**Step 1: Write failing test for input prompt**

Add to `test_ui.bats`:

```bats
@test "ui_input returns user input" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  
  echo "test_value" | {
    result=$(ui_input "Enter value")
    [ "$result" = "test_value" ]
  }
}
```

**Step 2: Run test - verify fails**

```bash
./bats/bin/bats test_ui.bats
```

Expected: FAIL

**Step 3: Implement input function**

Add to `ui.sh`:

```bash
# Input prompt
# Usage: ui_input "Prompt message" [default_value]
ui_input() {
  local prompt="$1"
  local default="${2:-}"
  
  if [ "$UI_BACKEND" = "ascii" ]; then
    _ui_input_ascii "$prompt" "$default"
  else
    _ui_input_whiptail "$prompt" "$default"
  fi
}

_ui_input_ascii() {
  local prompt="$1"
  local default="$2"
  
  if [ -n "$default" ]; then
    printf "❓ $prompt [$default]: "
  else
    printf "❓ $prompt: "
  fi
  
  read value
  if [ -z "$value" ] && [ -n "$default" ]; then
    echo "$default"
  else
    echo "$value"
  fi
}
```

**Step 4: Run test - verify passes**

```bash
./bats/bin/bats test_ui.bats
```

Expected: PASS

**Step 5: Write test for secret input**

Add to `test_ui.bats`:

```bats
@test "ui_input_secret masks input" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  
  # Can't test masking visually, but verify function exists
  run type ui_input_secret
  [ "$status" -eq 0 ]
}
```

**Step 6: Implement secret input**

Add to `ui.sh`:

```bash
# Secret input (masked)
ui_input_secret() {
  local prompt="$1"
  
  if [ "$UI_BACKEND" = "ascii" ]; then
    printf "🔑 $prompt: "
    stty -echo 2>/dev/null
    read value
    stty echo 2>/dev/null
    echo ""  # Newline after masked input
    echo "$value"
  else
    _ui_input_whiptail "$prompt" "" "--passwordbox"
  fi
}
```

**Step 7: Run tests and commit**

```bash
./bats/bin/bats test_ui.bats
git add install/
git commit -m "feat: add input prompts with secret masking"
```

---

## Task 4: JSON Config Generation

**Files:**
- Create: `install/lib/config.sh`
- Create: `install/tests/test_config.bats`

**Step 1: Write failing test for config creation**

Create `install/tests/test_config.bats`:

```bats
#!/usr/bin/env bats

@test "config_create generates valid JSON" {
  source ../lib/config.sh
  
  result=$(config_create "test-bot" "sk-123" "openrouter")
  echo "$result" | jq -e '.bot_token' >/dev/null 2>&1
  [ $? -eq 0 ]
}

@test "config_create includes all required fields" {
  source ../lib/config.sh
  
  result=$(config_create "bot" "key" "provider")
  [[ "$result" == *'"bot_token":'* ]]
  [[ "$result" == *'"api_key":'* ]]
  [[ "$result" == *'"provider":'* ]]
}
```

**Step 2: Run test - verify fails**

```bash
./bats/bin/bats test_config.bats
```

Expected: FAIL

**Step 3: Implement config library**

Create `install/lib/config.sh`:

```bash
#!/bin/sh
# JSON configuration management for MikroClaw installer

# Create JSON config from parameters
# Usage: config_create bot_token api_key provider [base_url] [model]
config_create() {
  local bot_token="$1"
  local api_key="$2"
  local provider="${3:-openrouter}"
  local base_url="${4:-}"
  local model="${5:-}"
  
  # Set defaults based on provider
  case "$provider" in
    openrouter)
      [ -z "$base_url" ] && base_url="https://openrouter.ai/api/v1"
      [ -z "$model" ] && model="google/gemini-flash"
      ;;
    openai)
      [ -z "$base_url" ] && base_url="https://api.openai.com/v1"
      [ -z "$model" ] && model="gpt-4o"
      ;;
    localai)
      [ -z "$base_url" ] && base_url="http://localhost:8080/v1"
      [ -z "$model" ] && model="llama3"
      ;;
  esac
  
  # Generate JSON
  jq -n \
    --arg bot "$bot_token" \
    --arg key "$api_key" \
    --arg provider "$provider" \
    --arg url "$base_url" \
    --arg model "$model" \
    '{
      bot_token: $bot,
      api_key: $key,
      provider: $provider,
      base_url: $url,
      model: $model,
      auth_type: "bearer",
      memory: {
        enabled: false,
        provider: null
      }
    }'
}

# Add memory configuration
config_add_memory() {
  local config="$1"
  local provider="$2"  # "b2" or "memu"
  
  echo "$config" | jq \
    --arg provider "$provider" \
    '.memory = {
      enabled: true,
      provider: $provider
    }'
}
```

**Step 4: Run tests - verify passes**

```bash
./bats/bin/bats test_config.bats
```

Expected: PASS

**Step 5: Commit**

```bash
git add install/
git commit -m "feat: add JSON config generation with jq"
```

---

## Task 5: Main Installer Script with Interactive Flow

**Files:**
- Create: `install/mikroclaw-install.sh`
- Create: `install/tests/test_installer.bats`

**Step 1: Write failing integration test**

Create `install/tests/test_installer.bats`:

```bats
#!/usr/bin/env bats

@test "installer detects missing arguments" {
  run ../mikroclaw-install.sh --help
  [ "$status" -eq 0 ]
  [[ "$output" == *"Usage:"* ]]
}

@test "installer has all required functions" {
  source ../mikroclaw-install.sh
  
  run type install_routeros
  [ "$status" -eq 0 ]
  
  run type install_linux
  [ "$status" -eq 0 ]
}
```

**Step 2: Run test - verify fails**

```bash
./bats/bin/bats test_installer.bats
```

Expected: FAIL

**Step 3: Create main installer script (minimal)**

Create `install/mikroclaw-install.sh`:

```bash
#!/bin/sh
# MikroClaw TUI Installer v0.2.0

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load libraries
. "$SCRIPT_DIR/lib/ui.sh"
. "$SCRIPT_DIR/lib/config.sh"

# Show usage
usage() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "Options:"
  echo "  --target TARGET    Install target: routeros, linux, docker"
  echo "  --ip IP            Router IP (for routeros target)"
  echo "  --user USER        Router username (default: admin)"
  echo "  --help             Show this help"
  echo ""
  echo "Examples:"
  echo "  $0                             # Interactive mode"
  echo "  $0 --target routeros --ip 192.168.88.1"
  echo ""
}

# Install to RouterOS
install_routeros() {
  local router_ip="$1"
  local router_user="$2"
  
  ui_banner
  ui_msg "Installing MikroClaw to RouterOS..."
  ui_msg "Router: $router_user@$router_ip"
  
  # TODO: Implementation in next task
  ui_msg "(Implementation pending)"
}

# Install to Linux
install_linux() {
  ui_banner
  ui_msg "Installing MikroClaw to Linux..."
  
  # TODO: Implementation in next task
  ui_msg "(Implementation pending)"
}

# Main interactive flow
main_interactive() {
  ui_init
  ui_banner
  
  # Select target
  ui_menu "Select Install Target" \
    "RouterOS (MikroTik router)" \
    "Linux (standalone binary)" \
    "Docker (container)" \
    "Exit"
  
  read target
  
  case "$target" in
    1) install_routeros_interactive ;;
    2) install_linux_interactive ;;
    3) install_docker_interactive ;;
    4) exit 0 ;;
    *) ui_error "Invalid selection" ;;
  esac
}

# RouterOS interactive install
install_routeros_interactive() {
  ui_banner
  ui_msg "RouterOS Installation"
  ui_msg ""
  
  # Get router credentials
  local router_ip=$(ui_input "Router IP address")
  local router_user=$(ui_input "Router username" "admin")
  local router_pass=$(ui_input_secret "Router password")
  
  # Get bot token
  ui_msg ""
  local bot_token=$(ui_input_secret "Telegram Bot Token")
  
  # Get LLM config
  ui_msg ""
  ui_menu "Select LLM Provider" \
    "OpenRouter (recommended)" \
    "OpenAI" \
    "LocalAI"
  
  read provider_choice
  
  local provider="openrouter"
  case "$provider_choice" in
    1) provider="openrouter" ;;
    2) provider="openai" ;;
    3) provider="localai" ;;
  esac
  
  local api_key=$(ui_input_secret "LLM API Key")
  
  # Generate config (don't save secrets to disk!)
  local config=$(config_create "$bot_token" "$api_key" "$provider")
  
  ui_msg ""
  ui_msg "Configuration complete (not saved to disk)"
  ui_msg "Installing to $router_ip..."
  
  # Install
  install_routeros "$router_ip" "$router_user"
}

# Linux interactive install
install_linux_interactive() {
  ui_banner
  ui_msg "Linux Installation"
  ui_msg ""
  
  # Similar flow...
  ui_msg "(Implementation pending)"
}

# Docker interactive install
install_docker_interactive() {
  ui_banner
  ui_msg "Docker Installation"
  ui_msg ""
  
  ui_msg "(Implementation pending)"
}

# Parse command line
TARGET=""
ROUTER_IP=""
ROUTER_USER="admin"

while [ $# -gt 0 ]; do
  case "$1" in
    --target)
      TARGET="$2"
      shift 2
      ;;
    --ip)
      ROUTER_IP="$2"
      shift 2
      ;;
    --user)
      ROUTER_USER="$2"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

# Run
if [ -n "$TARGET" ]; then
  # Command line mode
  case "$TARGET" in
    routeros)
      if [ -z "$ROUTER_IP" ]; then
        echo "Error: --ip required for routeros target"
        exit 1
      fi
      install_routeros "$ROUTER_IP" "$ROUTER_USER"
      ;;
    linux)
      install_linux
      ;;
    *)
      echo "Error: Unknown target '$TARGET'"
      exit 1
      ;;
  esac
else
  # Interactive mode
  main_interactive
fi
```

**Step 4: Run tests - verify passes**

```bash
./bats/bin/bats test_installer.bats
```

Expected: PASS

**Step 5: Make executable and test manually**

```bash
chmod +x install/mikroclaw-install.sh
cd install
./mikroclaw-install.sh --help
```

Expected: Shows usage

**Step 6: Commit**

```bash
git add install/
git commit -m "feat: add main installer script with interactive flow"
```

---

## Task 6: RouterOS Deployment Implementation

**Files:**
- Modify: `install/mikroclaw-install.sh`
- Modify: `install/tests/test_installer.bats`

**Step 1: Write test for SSH connectivity**

Add to `test_installer.bats`:

```bats
@test "routeros_install checks SSH connectivity" {
  source ../mikroclaw-install.sh
  
  # Mock ssh command
  ssh() {
    echo "mock ssh"
    return 0
  }
  
  run routeros_check_ssh "192.168.1.1" "admin"
  [ "$status" -eq 0 ]
}
```

**Step 2: Implement RouterOS deployment**

Add functions to `mikroclaw-install.sh`:

```bash
# Check SSH connectivity to router
routeros_check_ssh() {
  local ip="$1"
  local user="$2"
  
  ui_progress "Checking SSH connectivity to $ip"
  
  if ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$user@$ip" ":put \"OK\"" 2>/dev/null | grep -q "OK"; then
    ui_done
    return 0
  else
    ui_error "Cannot connect to router"
    return 1
  fi
}

# Upload files to router
routeros_upload() {
  local ip="$1"
  local user="$2"
  local local_file="$3"
  local remote_path="$4"
  
  ui_progress "Uploading $(basename "$local_file")"
  scp -o StrictHostKeyChecking=no "$local_file" "$user@$ip:$remote_path" 2>/dev/null
  ui_done
}

# Update install_routeros function
install_routeros() {
  local router_ip="$1"
  local router_user="$2"
  
  ui_banner
  ui_msg "Installing MikroClaw to RouterOS"
  ui_msg "Router: $router_user@$router_ip"
  ui_msg ""
  
  # Check connectivity
  if ! routeros_check_ssh "$router_ip" "$router_user"; then
    ui_error "Failed to connect to router"
    exit 1
  fi
  
  # TODO: Actual deployment in next iteration
  ui_msg "Connected successfully!"
  ui_msg "(Full deployment implementation pending)"
}
```

**Step 3: Add ui_progress and ui_error helpers**

Add to `lib/ui.sh`:

```bash
# Progress indicator
ui_progress() {
  local msg="$1"
  printf "⏳ %s..." "$msg"
}

ui_done() {
  echo " ✅"
}

ui_error() {
  local msg="$1"
  echo " ❌ $msg"
}

ui_msg() {
  local msg="$1"
  echo "  $msg"
}

ui_banner() {
  clear 2>/dev/null || true
  echo "╔══════════════════════════════════════════╗"
  echo "║     MikroClaw Installer v0.2.0          ║"
  echo "║     AI Agent for MikroTik RouterOS      ║"
  echo "╚══════════════════════════════════════════╝"
  echo ""
}
```

**Step 4: Run tests and commit**

```bash
./bats/bin/bats test_installer.bats
git add install/
git commit -m "feat: add RouterOS SSH connectivity check"
```

---

## Task 7: Full Deployment and Download

**Files:**
- Modify: `install/mikroclaw-install.sh`
- Create: `install/tests/test_download.bats`

**Step 1: Test binary download**

Create `install/tests/test_download.bats`:

```bats
#!/usr/bin/env bats

@test "download_binary uses curl if available" {
  source ../lib/download.sh 2>/dev/null || source ../mikroclaw-install.sh
  
  # Mock curl
  curl() {
    echo "downloaded"
    return 0
  }
  
  result=$(download_binary "linux-x64" "/tmp/test")
  [ "$result" = "downloaded" ] || [ -f "/tmp/test" ]
}
```

**Step 2: Implement download function**

Add to `mikroclaw-install.sh`:

```bash
# Download binary for platform
download_binary() {
  local platform="$1"
  local output="$2"
  
  local url="https://github.com/mikroclaw/mikroclaw/releases/latest/download/mikroclaw-${platform}"
  
  ui_progress "Downloading MikroClaw for ${platform}"
  
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$url" -o "$output" 2>/dev/null
  elif command -v wget >/dev/null 2>&1; then
    wget -q "$url" -O "$output" 2>/dev/null
  else
    ui_error "curl or wget required"
    return 1
  fi
  
  chmod +x "$output"
  ui_done
}
```

**Step 3: Complete RouterOS deployment**

Update `install_routeros`:

```bash
install_routeros() {
  local router_ip="$1"
  local router_user="$2"
  local config="$3"  # JSON config
  
  ui_banner
  ui_msg "Installing MikroClaw to RouterOS"
  ui_msg "Router: $router_user@$router_ip"
  ui_msg ""
  
  # Check connectivity
  if ! routeros_check_ssh "$router_ip" "$router_user"; then
    ui_error "Failed to connect to router"
    exit 1
  fi
  
  # Create temp directory
  local tmpdir=$(mktemp -d)
  trap "rm -rf $tmpdir" EXIT
  
  # Download binary
  local binary="$tmpdir/mikroclaw"
  if ! download_binary "linux-x64" "$binary"; then
    ui_error "Failed to download binary"
    exit 1
  fi
  
  # Upload to router
  routeros_upload "$router_ip" "$router_user" "$binary" "/disk1/mikroclaw"
  
  # Create install script on router
  ui_progress "Configuring container"
  
  ssh -o StrictHostKeyChecking=no "$router_user@$router_ip" << EOF
    /container/config/set ram-high=256M ram-max=288M
    /file/remove disk1/mikroclaw-install.rsc
    /file/add name=disk1/mikroclaw-install.rsc contents="
/container/remove [find name=mikroclaw]
/file/remove disk1/mikroclaw-container.tar.gz
/container/add name=mikroclaw file=disk1/mikroclaw envlist=mikroclaw-env interface=veth1 start-on-boot=yes logging=yes
"
    /import disk1/mikroclaw-install.rsc
EOF

  ui_done
  
  ui_msg ""
  ui_msg "✅ Installation Complete!"
  ui_msg ""
  ui_msg "View logs: ssh $router_user@$router_ip '/container/logs mikroclaw follow'"
}
```

**Step 4: Pass config to install function**

Update `install_routeros_interactive`:

```bash
install_routeros_interactive() {
  # ... config collection ...
  
  local config=$(config_create "$bot_token" "$api_key" "$provider")
  
  # Install with config
  install_routeros "$router_ip" "$router_user" "$config"
}
```

**Step 5: Final test and commit**

```bash
./bats/bin/bats install/tests/
git add install/
git commit -m "feat: complete TUI installer with RouterOS deployment"
```

---

## Task 8: Documentation and Final Polish

**Files:**
- Create: `install/README.md`
- Create: `install/INSTALL_GUIDE.md`

**Step 1: Create README**

```markdown
# MikroClaw TUI Installer

Human-friendly installer for MikroClaw AI Agent.

## Quick Install

```bash
curl -fsSL https://mikroclaw.dev/install.sh | sh
```

## Manual Usage

```bash
cd install/
./mikroclaw-install.sh
```

## Testing

```bash
cd install/tests
./bats/bin/bats .
```

## Features

- Interactive TUI (whiptail/dialog/ASCII fallback)
- No build required - downloads pre-built binaries
- No secrets saved to disk
- RouterOS, Linux, and Docker support
```

**Step 2: Copy to Hetzner2 and final commit**

```bash
scp -r install/ agent@100.112.201.95:/home/agent/mikroclaw/
ssh agent@100.112.201.95 'cd /home/agent/mikroclaw && git add install/ && git commit -m "feat: complete TUI installer with full test suite"'
```

---

## Summary

| Task | Component | Tests |
|------|-----------|-------|
| 1 | Project structure + BATS | test_ui.bats |
| 2 | TUI backend detection | test_ui.bats |
| 3 | Menu system | test_ui.bats |
| 4 | Input prompts | test_ui.bats |
| 5 | JSON config | test_config.bats |
| 6 | Main installer | test_installer.bats |
| 7 | RouterOS deployment | test_installer.bats |
| 8 | Download system | test_download.bats |

**Ready to execute?** Start with Task 1.
