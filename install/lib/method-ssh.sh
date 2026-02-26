#!/bin/sh
# SSH deployment method for MikroClaw installer
# Provides SSH connectivity testing, detection, and deployment functions

METHOD_LAST_ERROR=""
METHOD_LAST_HINT=""

# Set connection error with helpful hint
# Usage: _ssh_set_error "raw_error_message"
_ssh_set_error() {
  local raw_error="$1"
  METHOD_LAST_ERROR="$raw_error"
  METHOD_LAST_HINT=""

  if printf '%s' "$raw_error" | grep -qi "permission denied"; then
    METHOD_LAST_HINT="Authentication failed. Verify router username/password."
  elif printf '%s' "$raw_error" | grep -Eqi "connection timed out|operation timed out|no route to host|network is unreachable"; then
    METHOD_LAST_HINT="Network timeout. Verify router IP, routing, and firewall access to SSH."
  elif printf '%s' "$raw_error" | grep -qi "connection refused"; then
    METHOD_LAST_HINT="SSH is not accepting connections. Enable SSH service on RouterOS and confirm port."
  elif printf '%s' "$raw_error" | grep -qi "host key verification failed"; then
    METHOD_LAST_HINT="Host key mismatch. Remove stale known_hosts entry and retry."
  elif printf '%s' "$raw_error" | grep -qi "could not resolve hostname"; then
    METHOD_LAST_HINT="Hostname/IP could not be resolved. Check the router address."
  elif printf '%s' "$raw_error" | grep -qi "connection closed"; then
    METHOD_LAST_HINT="Connection closed by remote host. Confirm SSH service and account policy on router."
  else
    METHOD_LAST_HINT="Connection failed. Verify IP, username, password, and SSH availability."
  fi
}

# Show connection hint and error details
# Usage: method_ssh_show_error
method_ssh_show_error() {
  if [ -n "$METHOD_LAST_HINT" ]; then
    ui_msg "Hint: $METHOD_LAST_HINT"
  fi
  if [ -n "$METHOD_LAST_ERROR" ]; then
    ui_msg "Details: $METHOD_LAST_ERROR"
  fi
}

# Test SSH connectivity on given port
# Usage: method_ssh_test <ip> <port>
# Returns: 0 if OK, 1 otherwise
method_ssh_test() {
  local ip="$1"
  local port="${2:-22}"
  
  METHOD_LAST_ERROR=""
  METHOD_LAST_HINT=""
  
  # First do a quick TCP probe with netcat
  if command -v nc >/dev/null 2>&1; then
    if ! nc -z -w 2 "$ip" "$port" 2>/dev/null; then
      METHOD_LAST_ERROR="TCP connection to port $port failed"
      METHOD_LAST_HINT="Port $port is not reachable. Check firewall and SSH service."
      return 1
    fi
  fi
  
  return 0
}

# Detect SSH on common ports
# Usage: method_ssh_detect <ip>
# Output: "ssh:<port>" on success, nothing on failure
method_ssh_detect() {
  local ip="$1"
  local ports="22 2222 8022"
  
  for port in $ports; do
    if method_ssh_test "$ip" "$port"; then
      echo "ssh:$port"
      return 0
    fi
  done
  
  return 1
}

# Test full SSH connection with authentication
# Usage: _ssh_test_auth <ip> <port> <user> [pass]
# Returns: 0 if OK, 1 otherwise
_ssh_test_auth() {
  local ip="$1"
  local port="${2:-22}"
  local user="$3"
  local pass="$4"
  
  local err_file
  err_file=$(mktemp)
  METHOD_LAST_ERROR=""
  METHOD_LAST_HINT=""
  
  # Try sshpass first if password provided
  if [ -n "$pass" ] && command -v sshpass >/dev/null 2>&1; then
    if sshpass -p "$pass" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -p "$port" "$user@$ip" ":put \"OK\"" 2>"$err_file" | grep -q "OK"; then
      rm -f "$err_file"
      return 0
    fi
    _ssh_set_error "$(tr '\n' ' ' <"$err_file")"
    : >"$err_file"
  fi
  
  # Try key-based auth
  if ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes -p "$port" "$user@$ip" ":put \"OK\"" 2>"$err_file" | grep -q "OK"; then
    rm -f "$err_file"
    return 0
  fi
  
  _ssh_set_error "$(tr '\n' ' ' <"$err_file")"
  rm -f "$err_file"
  return 1
}

# Deploy via SSH/SCP
# Usage: method_ssh_deploy <ip> <port> <user> [pass] <config>
# Returns: 0 if OK, 1 otherwise
method_ssh_deploy() {
  local ip="$1"
  local port="${2:-22}"
  local user="$3"
  local pass="$4"
  local config="$5"
  
  METHOD_LAST_ERROR=""
  METHOD_LAST_HINT=""
  
  # Test connectivity first
  ui_progress "Testing SSH connectivity to $ip:$port"
  if ! _ssh_test_auth "$ip" "$port" "$user" "$pass"; then
    ui_error "Cannot connect to router"
    return 1
  fi
  ui_done
  
  # Create temp directory
  local tmpdir
  tmpdir=$(mktemp -d)
  trap "rm -rf $tmpdir" EXIT
  
  # Download binary
  local binary="$tmpdir/mikroclaw"
  ui_progress "Downloading MikroClaw binary"
  if ! download_binary "linux-x64" "$binary"; then
    ui_error "Failed to download binary"
    return 1
  fi
  ui_done
  
  # Write config file
  local config_file="$tmpdir/mikroclaw.env.json"
  printf '%s\n' "$config" > "$config_file"
  
  # Upload binary via SCP
  ui_progress "Uploading MikroClaw binary to /disk1/"
  local upload_err
  upload_err=$(mktemp)
  
  if [ -n "$pass" ] && command -v sshpass >/dev/null 2>&1; then
    if ! sshpass -p "$pass" scp -o StrictHostKeyChecking=no -o ConnectTimeout=10 -P "$port" "$binary" "$user@$ip:/disk1/mikroclaw" 2>"$upload_err"; then
      _ssh_set_error "$(cat "$upload_err")"
      rm -f "$upload_err"
      ui_error "Failed to upload binary"
      return 1
    fi
  else
    if ! scp -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes -P "$port" "$binary" "$user@$ip:/disk1/mikroclaw" 2>"$upload_err"; then
      _ssh_set_error "$(cat "$upload_err")"
      rm -f "$upload_err"
      ui_error "Failed to upload binary"
      return 1
    fi
  fi
  rm -f "$upload_err"
  ui_done
  
  # Upload config via SCP
  ui_progress "Uploading config to /disk1/"
  upload_err=$(mktemp)
  
  if [ -n "$pass" ] && command -v sshpass >/dev/null 2>&1; then
    if ! sshpass -p "$pass" scp -o StrictHostKeyChecking=no -o ConnectTimeout=10 -P "$port" "$config_file" "$user@$ip:/disk1/mikroclaw.env.json" 2>"$upload_err"; then
      _ssh_set_error "$(cat "$upload_err")"
      rm -f "$upload_err"
      ui_error "Failed to upload config"
      return 1
    fi
  else
    if ! scp -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes -P "$port" "$config_file" "$user@$ip:/disk1/mikroclaw.env.json" 2>"$upload_err"; then
      _ssh_set_error "$(cat "$upload_err")"
      rm -f "$upload_err"
      ui_error "Failed to upload config"
      return 1
    fi
  fi
  rm -f "$upload_err"
  ui_done
  
  ui_msg "✓ MikroClaw deployed to /disk1/ on $ip"
  return 0
}
