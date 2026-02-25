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

  if [ ! -t 0 ] || [ ! -t 1 ]; then
    _ui_menu_ascii "$title" "$@"
    return
  fi
  
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
  
  echo "" >&2
  echo "═══════════════════════════════════════════" >&2
  echo "  $title" >&2
  echo "═══════════════════════════════════════════" >&2
  echo "" >&2
  
  local i=1
  for option in "$@"; do
    echo "   [$i] $option" >&2
    i=$((i + 1))
  done
  
  echo "" >&2
  printf "➤ Select option: " >&2
  if [ -t 0 ] && [ -r /dev/tty ]; then
    read selection < /dev/tty
  else
    read selection
  fi
  echo "$selection"
}

# Whiptail/Dialog menu
_ui_menu_whiptail() {
  local title="$1"
  shift

  local i=1
  local menu_items=""
  local menu_height=10
  local selection=""

  for option in "$@"; do
    menu_items="$menu_items $i \"$option\""
    i=$((i + 1))
  done

  if [ "$UI_BACKEND" = "whiptail" ]; then
    selection=$(eval "whiptail --title \"$title\" --menu \"$title\" 20 78 $menu_height $menu_items 3>&1 1>&2 2>&3")
  else
    selection=$(eval "dialog --clear --title \"$title\" --menu \"$title\" 20 78 $menu_height $menu_items --stdout")
  fi

  if [ $? -ne 0 ] || [ -z "$selection" ]; then
    echo "4"
    return
  fi

  echo "$selection"
}

# Input prompt
# Usage: ui_input "Prompt message" [default_value]
ui_input() {
  local prompt="$1"
  local default="${2:-}"
  
  if [ -n "$default" ]; then
    printf "❓ $prompt [$default]: " >&2
  else
    printf "❓ $prompt: " >&2
  fi
  
  if [ -t 0 ] && [ -r /dev/tty ]; then
    read value < /dev/tty
  else
    read value
  fi
  if [ -z "$value" ] && [ -n "$default" ]; then
    echo "$default"
  else
    echo "$value"
  fi
}

# Secret input (masked)
ui_input_secret() {
  local prompt="$1"
  
  printf "🔑 $prompt: " >&2
  stty -echo 2>/dev/null
  if [ -t 0 ] && [ -r /dev/tty ]; then
    read value < /dev/tty
  else
    read value
  fi
  stty echo 2>/dev/null
  echo "" >&2
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
  echo "╔══════════════════════════════════════════╗"
  echo "║   MikroClaw Installer 2026.02.25:BETA   ║"
  echo "║     AI Agent for MikroTik RouterOS      ║"
  echo "╚══════════════════════════════════════════╝"
  echo ""
}

ui_clear() {
  clear 2>/dev/null || true
}
