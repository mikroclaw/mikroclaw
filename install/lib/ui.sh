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

# Whiptail/Dialog menu
_ui_menu_whiptail() {
  local title="$1"
  shift
  
  # Fallback to ASCII for now (whiptail requires TTY)
  _ui_menu_ascii "$title" "$@"
}

# Input prompt
# Usage: ui_input "Prompt message" [default_value]
ui_input() {
  local prompt="$1"
  local default="${2:-}"
  
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

# Secret input (masked)
ui_input_secret() {
  local prompt="$1"
  
  printf "🔑 $prompt: "
  stty -echo 2>/dev/null
  read value
  stty echo 2>/dev/null
  echo ""  # Newline after masked input
  echo "$value"
}

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
