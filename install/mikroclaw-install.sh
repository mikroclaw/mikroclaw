#!/bin/sh
# MikroClaw TUI Installer 2025.02.25:BETA2
# Human-friendly installer for MikroClaw AI Agent

set -e

INSTALLER_VERSION="2025.02.25:BETA2"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load libraries
. "$SCRIPT_DIR/lib/ui.sh"
. "$SCRIPT_DIR/lib/config.sh"

# Show usage
usage() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "MikroClaw Installer ${INSTALLER_VERSION}"
  echo ""
  echo "Options:"
  echo "  --target TARGET    Install target: routeros, linux"
  echo "  --ip IP            Router IP (for routeros target)"
  echo "  --user USER        Router username (default: admin)"
  echo "  --pass PASS        Router password"
  echo "  --provider NAME    LLM provider (default: openrouter)"
  echo "  --bot-token TOKEN  Telegram bot token (routeros target)"
  echo "  --api-key KEY      LLM API key (routeros target)"
  echo "  --base-url URL     Override LLM base URL"
  echo "  --model MODEL      Override LLM model"
  echo "  --help             Show this help"
  echo ""
  echo "Examples:"
  echo "  $0                             # Interactive mode"
  echo "  $0 --target routeros --ip 192.168.88.1 --user admin --pass secret --provider groq"
  echo ""
}

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

is_valid_ipv4() {
  ip="$1"
  oldifs="$IFS"
  IFS='.'
  set -- $ip
  IFS="$oldifs"
  [ $# -eq 4 ] || return 1
  for octet in "$@"; do
    case "$octet" in
      ''|*[!0-9]*) return 1 ;;
    esac
    [ "$octet" -ge 0 ] 2>/dev/null || return 1
    [ "$octet" -le 255 ] 2>/dev/null || return 1
  done
  return 0
}

is_valid_user() {
  user="$1"
  case "$user" in
    ''|*[!A-Za-z0-9._-]*) return 1 ;;
  esac
  return 0
}

is_valid_provider() {
  provider="$1"
  case "$provider" in
    openrouter|openai|anthropic|ollama|groq|mistral|xai|deepseek|together|fireworks|perplexity|cohere|bedrock|kimi|minimax|zai|synthetic)
      return 0
      ;;
  esac
  return 1
}

ROUTEROS_LAST_ERROR=""
ROUTEROS_LAST_HINT=""

routeros_set_connection_error() {
  local raw_error="$1"
  ROUTEROS_LAST_ERROR="$raw_error"
  ROUTEROS_LAST_HINT=""

  if printf '%s' "$raw_error" | grep -qi "permission denied"; then
    ROUTEROS_LAST_HINT="Authentication failed. Verify router username/password."
  elif printf '%s' "$raw_error" | grep -Eqi "connection timed out|operation timed out|no route to host|network is unreachable"; then
    ROUTEROS_LAST_HINT="Network timeout. Verify router IP, routing, and firewall access to SSH (tcp/22)."
  elif printf '%s' "$raw_error" | grep -qi "connection refused"; then
    ROUTEROS_LAST_HINT="SSH is not accepting connections. Enable SSH service on RouterOS and confirm port 22."
  elif printf '%s' "$raw_error" | grep -qi "host key verification failed"; then
    ROUTEROS_LAST_HINT="Host key mismatch. Remove stale known_hosts entry and retry."
  elif printf '%s' "$raw_error" | grep -qi "could not resolve hostname"; then
    ROUTEROS_LAST_HINT="Hostname/IP could not be resolved. Check the router address."
  elif printf '%s' "$raw_error" | grep -qi "connection closed"; then
    ROUTEROS_LAST_HINT="Connection closed by remote host. Confirm SSH service and account policy on router."
  else
    ROUTEROS_LAST_HINT="Connection failed. Verify IP, username, password, and SSH availability."
  fi
}

routeros_show_connection_hint() {
  if [ -n "$ROUTEROS_LAST_HINT" ]; then
    ui_msg "Hint: $ROUTEROS_LAST_HINT"
  fi
  if [ -n "$ROUTEROS_LAST_ERROR" ]; then
    ui_msg "Details: $ROUTEROS_LAST_ERROR"
  fi
}

# Check SSH connectivity to router
routeros_check_ssh() {
  local ip="$1"
  local user="$2"

  if ! is_valid_ipv4 "$ip"; then
    ui_error "Invalid router IP"
    return 1
  fi
  if ! is_valid_user "$user"; then
    ui_error "Invalid router username"
    return 1
  fi
  
  ui_progress "Checking SSH connectivity to $ip"
  
  if ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes "$user@$ip" ":put \"OK\"" 2>/dev/null | grep -q "OK"; then
    ui_done
    return 0
  else
    ui_error "Cannot connect to router"
    return 1
  fi
}

routeros_can_connect() {
  local ip="$1"
  local user="$2"
  local pass="$3"

  local err_file
  err_file=$(mktemp)
  ROUTEROS_LAST_ERROR=""
  ROUTEROS_LAST_HINT=""

  if command -v sshpass >/dev/null 2>&1; then
    if sshpass -p "$pass" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$user@$ip" ":put \"OK\"" 2>"$err_file" | grep -q "OK"; then
      rm -f "$err_file"
      return 0
    fi
    routeros_set_connection_error "$(tr '\n' ' ' <"$err_file")"
    : >"$err_file"
  fi

  if ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes "$user@$ip" ":put \"OK\"" 2>"$err_file" | grep -q "OK"; then
    rm -f "$err_file"
    return 0
  fi

  routeros_set_connection_error "$(tr '\n' ' ' <"$err_file")"
  rm -f "$err_file"

  return 1
}

# Install to RouterOS
install_routeros() {
  local router_ip="$1"
  local router_user="$2"
  local router_pass="$3"
  local config="$4"
  
  if ! is_valid_ipv4 "$router_ip"; then
    ui_error "Invalid router IP"
    return 1
  fi
  if ! is_valid_user "$router_user"; then
    ui_error "Invalid router username"
    return 1
  fi

  ui_banner
  ui_msg "Installing MikroClaw to RouterOS"
  ui_msg "Router: $router_user@$router_ip"
  ui_msg ""
  
  # Check connectivity
  if ! routeros_can_connect "$router_ip" "$router_user" "$router_pass"; then
    ui_error "Failed to connect to router. Check IP, user, and password."
    routeros_show_connection_hint
    return 1
  fi
  ui_msg "✓ Connected to router"
  
  # Create temp directory
  local tmpdir=$(mktemp -d)
  trap "rm -rf $tmpdir" EXIT
  
  # Download binary
  local binary="$tmpdir/mikroclaw"
  if ! download_binary "linux-x64" "$binary"; then
    ui_error "Failed to download binary"
    return 1
  fi
  
  ui_msg "✓ Binary downloaded"
  ui_msg ""
  local config_file="$tmpdir/mikroclaw.env.json"
  printf '%s\n' "$config" > "$config_file"
  ui_msg "✓ Config generated: $config_file"
  ui_msg ""
  ui_msg "Note: Full deployment requires manual container setup on RouterOS."
  ui_msg "Binary is ready at: $binary"
  ui_msg ""
  ui_msg "Next steps:"
  ui_msg "  1. Copy binary to router: scp $binary $router_user@$router_ip:/disk1/"
  ui_msg "  2. Copy config: scp $config_file $router_user@$router_ip:/disk1/"
  ui_msg "  3. Configure container on RouterOS"
  ui_msg "  4. Set environment variables from the JSON config"

  return 0
}

# Install to Linux
install_linux() {
  ui_banner
  ui_msg "Installing MikroClaw to Linux"
  ui_msg ""
  
  local tmpdir=$(mktemp -d)
  trap "rm -rf $tmpdir" EXIT
  
  local binary="$tmpdir/mikroclaw"
  if ! download_binary "linux-x64" "$binary"; then
    ui_error "Failed to download binary"
    exit 1
  fi
  
  # Install to /usr/local/bin
  if [ -w "/usr/local/bin" ]; then
    cp "$binary" /usr/local/bin/mikroclaw
    ui_msg "✓ Installed to /usr/local/bin/mikroclaw"
  else
    ui_msg "⚠ Cannot write to /usr/local/bin"
    ui_msg "  Copy manually: sudo cp $binary /usr/local/bin/"
  fi
}

# Main interactive flow
main_interactive() {
  ui_init
  ui_clear
  ui_banner
  
  # Select target
  local target=$(ui_menu "Select Install Target" \
    "RouterOS (MikroTik router)" \
    "Linux (standalone binary)" \
    "Docker (container)" \
    "Exit")
  
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
  ui_clear
  ui_banner
  ui_msg "RouterOS Installation"
  ui_msg ""
  
  # Get router credentials
  local router_ip=$(ui_input "Router IP address")
  local router_user=$(ui_input "Router username" "admin")
  local router_pass=$(ui_input_secret "Router password")

  while true; do
    if routeros_can_connect "$router_ip" "$router_user" "$router_pass"; then
      break
    fi

    ui_error "Unable to connect to $router_user@$router_ip"
    routeros_show_connection_hint

    local preflight_choice=$(ui_menu "Router Connection Failed" \
      "Retry with same credentials" \
      "Re-enter router credentials" \
      "Cancel installation")

    case "$preflight_choice" in
      1)
        ;;
      2)
        router_ip=$(ui_input "Router IP address" "$router_ip")
        router_user=$(ui_input "Router username" "$router_user")
        router_pass=$(ui_input_secret "Router password")
        ;;
      3|4)
        ui_msg "Installation cancelled"
        return 1
        ;;
      *)
        ui_error "Invalid selection"
        ;;
    esac
  done
  
  # Get bot token
  ui_msg ""
  local bot_token=$(ui_input_secret "Telegram Bot Token")
  
  # Get LLM config
  ui_msg ""
  local provider_choice=$(ui_menu "Select LLM Provider" \
    "OpenRouter (recommended)" \
    "OpenAI" \
    "Anthropic" \
    "Ollama/LocalAI" \
    "Groq" \
    "Mistral" \
    "XAI (Grok)" \
    "DeepSeek" \
    "Together" \
    "Fireworks" \
    "Perplexity" \
    "Cohere" \
    "Bedrock (AWS)" \
    "Kimi (Moonshot)" \
    "MiniMax" \
    "Z.AI" \
    "Synthetic.New")
  
  local provider="openrouter"
  case "$provider_choice" in
    1) provider="openrouter" ;;
    2) provider="openai" ;;
    3) provider="anthropic" ;;
    4) provider="ollama" ;;
    5) provider="groq" ;;
    6) provider="mistral" ;;
    7) provider="xai" ;;
    8) provider="deepseek" ;;
    9) provider="together" ;;
    10) provider="fireworks" ;;
    11) provider="perplexity" ;;
    12) provider="cohere" ;;
    13) provider="bedrock" ;;
    14) provider="kimi" ;;
    15) provider="minimax" ;;
    16) provider="zai" ;;
    17) provider="synthetic" ;;
  esac
  
  local api_key=$(ui_input_secret "LLM API Key")

  local tg_allowlist=$(ui_input "Telegram allowlist (comma-separated, * for all, empty deny-all)")
  local dc_allowlist=$(ui_input "Discord allowlist (comma-separated, * for all, empty deny-all)")
  local sk_allowlist=$(ui_input "Slack allowlist (comma-separated, * for all, empty deny-all)")
  
  # Generate config (in memory only!)
  local config=$(config_create "$bot_token" "$api_key" "$provider")
  config=$(echo "$config" | sed "s/\"telegram\": \"\"/\"telegram\": \"$tg_allowlist\"/" | \
    sed "s/\"discord\": \"\"/\"discord\": \"$dc_allowlist\"/" | \
    sed "s/\"slack\": \"\"/\"slack\": \"$sk_allowlist\"/")
  
  ui_msg ""
  ui_msg "Configuration complete"
  ui_msg "Installing to $router_ip..."

  while true; do
    if install_routeros "$router_ip" "$router_user" "$router_pass" "$config"; then
      break
    fi

    local install_choice=$(ui_menu "Installation Failed" \
      "Retry install" \
      "Re-enter router credentials" \
      "Cancel installation")

    case "$install_choice" in
      1)
        ;;
      2)
        router_ip=$(ui_input "Router IP address" "$router_ip")
        router_user=$(ui_input "Router username" "$router_user")
        router_pass=$(ui_input_secret "Router password")
        ;;
      3|4)
        ui_msg "Installation cancelled"
        return 1
        ;;
      *)
        ui_error "Invalid selection"
        ;;
    esac
  done
}

# Linux interactive install
install_linux_interactive() {
  ui_clear
  ui_banner
  ui_msg "Linux Installation"
  ui_msg ""
  
  install_linux
  
  ui_msg ""
  ui_msg "✅ Installation Complete!"
  ui_msg "Run: mikroclaw --help"
}

# Docker interactive install
install_docker_interactive() {
  ui_clear
  ui_banner
  ui_msg "Docker Installation"
  ui_msg ""
  ui_msg "Docker support coming soon!"
}

# Parse command line
TARGET=""
ROUTER_IP=""
ROUTER_USER="admin"
ROUTER_PASS=""
PROVIDER="openrouter"
BOT_TOKEN=""
API_KEY=""
BASE_URL=""
MODEL=""

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
    --pass)
      ROUTER_PASS="$2"
      shift 2
      ;;
    --provider)
      PROVIDER="$2"
      shift 2
      ;;
    --bot-token)
      BOT_TOKEN="$2"
      shift 2
      ;;
    --api-key)
      API_KEY="$2"
      shift 2
      ;;
    --base-url)
      BASE_URL="$2"
      shift 2
      ;;
    --model)
      MODEL="$2"
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
  if ! is_valid_provider "$PROVIDER"; then
    echo "Error: Unsupported provider '$PROVIDER'"
    echo "Valid providers: openrouter, openai, anthropic, ollama, groq, mistral, xai, deepseek, together, fireworks, perplexity, cohere, bedrock, kimi, minimax, zai, synthetic"
    exit 1
  fi

  case "$TARGET" in
    routeros)
      if [ -z "$ROUTER_IP" ]; then
        echo "Error: --ip required for routeros target"
        exit 1
      fi

      config=$(config_create "$BOT_TOKEN" "$API_KEY" "$PROVIDER" "$BASE_URL" "$MODEL")
      install_routeros "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS" "$config"
      ;;
    linux)
      install_linux
      ;;
    docker)
      echo "Error: docker target is not yet implemented in CLI mode"
      exit 1
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
