#!/bin/sh
# MikroClaw TUI Installer 2026.02.25:BETA
# Human-friendly installer for MikroClaw AI Agent

set -e

INSTALLER_VERSION="2026.02.25:BETA"

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
  echo "  --target TARGET    Install target: routeros, linux, docker"
  echo "  --ip IP            Router IP (for routeros target)"
  echo "  --user USER        Router username (default: admin)"
  echo "  --pass PASS        Router password"
  echo "  --help             Show this help"
  echo ""
  echo "Examples:"
  echo "  $0                             # Interactive mode"
  echo "  $0 --target routeros --ip 192.168.88.1 --user admin --pass secret"
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

# Install to RouterOS
install_routeros() {
  local router_ip="$1"
  local router_user="$2"
  local router_pass="$3"
  local config="$4"
  
  if ! is_valid_ipv4 "$router_ip"; then
    ui_error "Invalid router IP"
    exit 1
  fi
  if ! is_valid_user "$router_user"; then
    ui_error "Invalid router username"
    exit 1
  fi

  ui_banner
  ui_msg "Installing MikroClaw to RouterOS"
  ui_msg "Router: $router_user@$router_ip"
  ui_msg ""
  
  # Check connectivity
  if ! sshpass -p "$router_pass" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$router_user@$router_ip" ":put \"OK\"" 2>/dev/null | grep -q "OK"; then
    if ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes "$router_user@$router_ip" ":put \"OK\"" 2>/dev/null | grep -q "OK"; then
      ui_error "Failed to connect to router. Check IP, user, and password."
      exit 1
    fi
  fi
  ui_msg "✓ Connected to router"
  
  # Create temp directory
  local tmpdir=$(mktemp -d)
  trap "rm -rf $tmpdir" EXIT
  
  # Download binary
  local binary="$tmpdir/mikroclaw"
  if ! download_binary "linux-x64" "$binary"; then
    ui_error "Failed to download binary"
    exit 1
  fi
  
  ui_msg "✓ Binary downloaded"
  ui_msg ""
  ui_msg "Note: Full deployment requires manual container setup on RouterOS."
  ui_msg "Binary is ready at: $binary"
  ui_msg ""
  ui_msg "Next steps:"
  ui_msg "  1. Copy binary to router: scp $binary $router_user@$router_ip:/disk1/"
  ui_msg "  2. Configure container on RouterOS"
  ui_msg "  3. Set environment variables from config"
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
  
  # Get bot token
  ui_msg ""
  local bot_token=$(ui_input_secret "Telegram Bot Token")
  
  # Get LLM config
  ui_msg ""
  local provider_choice=$(ui_menu "Select LLM Provider" \
    "OpenRouter (recommended)" \
    "OpenAI" \
    "Anthropic" \
    "Ollama/LocalAI")
  
  local provider="openrouter"
  case "$provider_choice" in
    1) provider="openrouter" ;;
    2) provider="openai" ;;
    3) provider="anthropic" ;;
    4) provider="ollama" ;;
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
  ui_msg "Configuration complete (not saved to disk)"
  ui_msg "Installing to $router_ip..."
  
  # Install
  install_routeros "$router_ip" "$router_user" "$router_pass" "$config"
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
      # Create minimal config for CLI mode
      config=$(config_create "" "" "openrouter")
      install_routeros "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS" "$config"
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
