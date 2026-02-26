#!/bin/sh
# MikroClaw TUI Installer 2025.02.25:BETA2
# Human-friendly installer for MikroClaw AI Agent

set -e

INSTALLER_VERSION="2025.02.25:BETA2"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load libraries
. "$SCRIPT_DIR/lib/ui.sh"
. "$SCRIPT_DIR/lib/config.sh"
. "$SCRIPT_DIR/lib/method-ssh.sh"
. "$SCRIPT_DIR/lib/method-rest.sh"
. "$SCRIPT_DIR/lib/method-api.sh"

# Global for method errors
METHOD_LAST_ERROR=""

# Show method error helper
method_show_error() {
  if [ -n "$METHOD_LAST_ERROR" ]; then
    ui_msg "Details: $METHOD_LAST_ERROR"
  fi
  if command -v method_ssh_show_error >/dev/null 2>&1; then
    method_ssh_show_error
  fi
}

# Show usage
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
  echo "  --method METHOD    Deployment method: ssh, rest, api (auto-detect if not specified)"
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
  echo "  $0 --target routeros --ip 192.168.88.1 --user admin --pass secret --method ssh"
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

# Detect all available connection methods
# Usage: method_detect_all <ip> <user> <pass>
# Sets: AVAILABLE_METHODS array with "method:port" entries
method_detect_all() {
  local ip="$1"
  local user="$2"
  local pass="$3"
  
  AVAILABLE_METHODS=""
  
  ui_progress "Detecting available connection methods"
  
  # Try SSH first
  local ssh_result
  ssh_result=$(method_ssh_detect "$ip" 2>/dev/null)
  if [ -n "$ssh_result" ]; then
    AVAILABLE_METHODS="$AVAILABLE_METHODS $ssh_result"
  fi
  
  # Try REST API
  local rest_result
  rest_result=$(method_rest_detect "$ip" "$user" "$pass" 2>/dev/null)
  if [ -n "$rest_result" ]; then
    AVAILABLE_METHODS="$AVAILABLE_METHODS $rest_result"
  fi
  
  # Try Binary API
  local api_result
  api_result=$(method_api_detect "$ip" "$user" "$pass" 2>/dev/null)
  if [ -n "$api_result" ]; then
    AVAILABLE_METHODS="$AVAILABLE_METHODS $api_result"
  fi
  
  ui_done
  
  # Trim leading space
  AVAILABLE_METHODS=$(echo "$AVAILABLE_METHODS" | sed 's/^ *//')
}

# Show method selection menu
# Usage: method_select_menu
# Returns: "method:port" selected by user
method_select_menu() {
  if [ -z "$AVAILABLE_METHODS" ]; then
    ui_error "No connection methods detected"
    return 1
  fi
  
  # Build menu options
  local menu_options=""
  local i=1
  
  for method in $AVAILABLE_METHODS; do
    case "$method" in
      ssh:*) menu_options="$menu_options ${i} 'SSH (port ${method#ssh:})'" ;;
      https:*) menu_options="$menu_options ${i} 'REST API HTTPS (port ${method#https:})'" ;;
      http:*) menu_options="$menu_options ${i} 'REST API HTTP (port ${method#http:})'" ;;
      api-ssl:*) menu_options="$menu_options ${i} 'Binary API SSL (port ${method#api-ssl:})'" ;;
      api:*) menu_options="$menu_options ${i} 'Binary API (port ${method#api:})'" ;;
    esac
    i=$((i + 1))
  done
  
  # Show menu using ui_menu - need to eval the options
  echo ""
  ui_msg "Select connection method:"
  
  # Build array for menu
  local opt_array=""
  for method in $AVAILABLE_METHODS; do
    case "$method" in
      ssh:*) opt_array="$opt_array 'SSH (port ${method#ssh:})'" ;;
      https:*) opt_array="$opt_array 'REST API HTTPS (port ${method#https:})'" ;;
      http:*) opt_array="$opt_array 'REST API HTTP (port ${method#http:})'" ;;
      api-ssl:*) opt_array="$opt_array 'Binary API SSL (port ${method#api-ssl:})'" ;;
      api:*) opt_array="$opt_array 'Binary API (port ${method#api:})'" ;;
    esac
  done
  
  local selection
  # Use ASCII menu directly to avoid complexity
  echo ""
  echo "═══════════════════════════════════════════" >&2
  echo "  Select Connection Method" >&2
  echo "═══════════════════════════════════════════" >&2
  echo "" >&2
  
  i=1
  for method in $AVAILABLE_METHODS; do
    case "$method" in
      ssh:*) echo "   [$i] SSH (port ${method#ssh:})" >&2 ;;
      https:*) echo "   [$i] REST API HTTPS (port ${method#https:})" >&2 ;;
      http:*) echo "   [$i] REST API HTTP (port ${method#http:})" >&2 ;;
      api-ssl:*) echo "   [$i] Binary API SSL (port ${method#api-ssl:})" >&2 ;;
      api:*) echo "   [$i] Binary API (port ${method#api:})" >&2 ;;
    esac
    i=$((i + 1))
  done
  
  echo "" >&2
  printf "➤ Select option: " >&2
  if [ -t 0 ] && [ -r /dev/tty ]; then
    read selection < /dev/tty
  else
    read selection
  fi
  
  # Get selected method
  i=1
  for method in $AVAILABLE_METHODS; do
    if [ "$i" = "$selection" ]; then
      echo "$method"
      return 0
    fi
    i=$((i + 1))
  done
  
  # Invalid selection - return first available
  echo "$AVAILABLE_METHODS" | cut -d' ' -f1
}

# Deploy using specified method
# Usage: routeros_deploy <ip> <user> <pass> <method:port> <config>
routeros_deploy() {
  local router_ip="$1"
  local router_user="$2"
  local router_pass="$3"
  local method_spec="$4"
  local config="$5"
  
  local method="${method_spec%%:*}"
  local port="${method_spec##*:}"
  
  # Get binary URL
  local binary_url="https://github.com/mikroclaw/mikroclaw/releases/latest/download/mikroclaw-linux-x64"
  
  case "$method" in
    ssh)
      method_ssh_deploy "$router_ip" "$port" "$router_user" "$router_pass" "$config"
      ;;
    https|http)
      # For REST API, we need to download binary locally then deploy
      local tmpdir
      tmpdir=$(mktemp -d)
      trap "rm -rf $tmpdir" EXIT
      
      local binary="$tmpdir/mikroclaw"
      if ! download_binary "linux-x64" "$binary"; then
        ui_error "Failed to download binary"
        return 1
      fi
      
      local config_file="$tmpdir/mikroclaw.env.json"
      printf '%s\n' "$config" > "$config_file"
      
      method_rest_deploy "$router_ip" "$port" "$router_user" "$router_pass" "$binary" "$config_file"
      ;;
    api|api-ssl)
      method_api_deploy "$router_ip" "$router_user" "$router_pass" "$port" "$binary_url" "$config"
      ;;
    *)
      ui_error "Unknown deployment method: $method"
      return 1
      ;;
  esac
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
  
  # Detect available methods
  method_detect_all "$router_ip" "$router_user" "$router_pass"
  
  if [ -z "$AVAILABLE_METHODS" ]; then
    ui_error "No connection methods detected on $router_ip"
    ui_msg "Please ensure at least one of these is enabled:"
    ui_msg "  - SSH (port 22, 2222, or 8022)"
    ui_msg "  - REST API (port 80 or 443)"
    ui_msg "  - Binary API (port 8728 or 8729)"
    return 1
  fi
  
  # Show available methods
  ui_msg ""
  ui_msg "Detected connection methods:"
  for method in $AVAILABLE_METHODS; do
    case "$method" in
      ssh:*) ui_msg "  - SSH on port ${method#ssh:}" ;;
      https:*) ui_msg "  - REST API (HTTPS) on port ${method#https:}" ;;
      http:*) ui_msg "  - REST API (HTTP) on port ${method#http:}" ;;
      api-ssl:*) ui_msg "  - Binary API (SSL) on port ${method#api-ssl:}" ;;
      api:*) ui_msg "  - Binary API on port ${method#api:}" ;;
    esac
  done
  
  # Let user select method
  local selected_method=$(method_select_menu)
  if [ $? -ne 0 ]; then
    ui_error "No method selected"
    return 1
  fi
  
  ui_msg ""
  ui_msg "Selected: $selected_method"
  
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
  ui_msg "Installing to $router_ip via $selected_method..."

  while true; do
    if routeros_deploy "$router_ip" "$router_user" "$router_pass" "$selected_method" "$config"; then
      ui_msg ""
      ui_msg "✅ Installation Complete!"
      break
    fi

    ui_error "Installation failed"
    method_show_error

    local install_choice=$(ui_menu "Installation Failed" \
      "Retry install" \
      "Re-enter router credentials" \
      "Select different method" \
      "Cancel installation")

    case "$install_choice" in
      1)
        ;;
      2)
        router_ip=$(ui_input "Router IP address" "$router_ip")
        router_user=$(ui_input "Router username" "$router_user")
        router_pass=$(ui_input_secret "Router password")
        # Re-detect with new credentials
        method_detect_all "$router_ip" "$router_user" "$router_pass"
        if [ -n "$AVAILABLE_METHODS" ]; then
          selected_method=$(method_select_menu)
        fi
        ;;
      3)
        selected_method=$(method_select_menu)
        ;;
      4|5)
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
METHOD=""  # Auto-detect if empty
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
    --method)
      METHOD="$2"
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
      
      # Determine method to use
      if [ -n "$METHOD" ]; then
        # Use specified method
        case "$METHOD" in
          ssh)
            method_spec="ssh:22"
            ;;
          rest)
            # Auto-detect REST port
            method_spec=$(method_rest_detect "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS")
            if [ -z "$method_spec" ]; then
              echo "Error: REST API not available on $ROUTER_IP" >&2
              exit 1
            fi
            ;;
          api)
            # Auto-detect API port
            method_spec=$(method_api_detect "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS")
            if [ -z "$method_spec" ]; then
              echo "Error: Binary API not available on $ROUTER_IP" >&2
              exit 1
            fi
            ;;
          *)
            echo "Error: Unknown method '$METHOD'. Use: ssh, rest, or api" >&2
            exit 1
            ;;
        esac
      else
        # Auto-detect
        method_detect_all "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS"
        if [ -z "$AVAILABLE_METHODS" ]; then
          echo "Error: No connection methods detected on $ROUTER_IP" >&2
          echo "Ensure SSH, REST API, or Binary API is enabled" >&2
          exit 1
        fi
        # Use first available method
        method_spec=$(echo "$AVAILABLE_METHODS" | cut -d' ' -f1)
      fi
      
      ui_progress "Deploying via $method_spec"
      if routeros_deploy "$ROUTER_IP" "$ROUTER_USER" "$ROUTER_PASS" "$method_spec" "$config"; then
        ui_done
        echo ""
        echo "✅ Installation Complete!"
      else
        ui_done
        echo ""
        echo "❌ Installation failed"
        exit 1
      fi
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

