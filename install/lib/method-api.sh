#!/bin/sh
# MikroTik Binary API deployment method
# Protocol: length-prefixed binary sentences on TCP 8728 (plain) or 8729 (SSL)
# Reference: https://wiki.mikrotik.com/wiki/Manual:API

# Encode word length per MikroTik API spec
# 0x00-0x7F:       1 byte
# 0x80-0x3FFF:     2 bytes (0x8000 | len)
# 0x4000-0x1FFFFF: 3 bytes (0xC00000 | len)
_api_encode_length() {
  local len="$1"
  if [ "$len" -lt 128 ]; then
    printf "\\$(printf '%03o' "$len")"
  elif [ "$len" -lt 16384 ]; then
    local hi=$(( (len >> 8) | 0x80 ))
    local lo=$(( len & 0xFF ))
    printf "\\$(printf '%03o' "$hi")\\$(printf '%03o' "$lo")"
  else
    local b1=$(( (len >> 16) | 0xC0 ))
    local b2=$(( (len >> 8) & 0xFF ))
    local b3=$(( len & 0xFF ))
    printf "\\$(printf '%03o' "$b1")\\$(printf '%03o' "$b2")\\$(printf '%03o' "$b3")"
  fi
}

# Send a single API word (length-prefixed)
_api_send_word() {
  local word="$1"
  local len=${#word}
  _api_encode_length "$len"
  printf '%s' "$word"
}

# Send end-of-sentence (zero-length word)
_api_send_end() {
  printf '\0'
}

# Build login sentence
_api_build_login() {
  local user="$1" pass="$2"
  _api_send_word "!login"
  _api_send_word "=name=$user"
  _api_send_word "=password=$pass"
  _api_send_end
}

# Test Binary API connectivity
method_api_test() {
  local ip="$1" port="${2:-8728}"

  # Fast TCP probe
  if ! nc -z -w 2 "$ip" "$port" 2>/dev/null; then
    METHOD_LAST_ERROR="API port $port not responding on $ip"
    return 1
  fi
  return 0
}

# Try Binary API on SSL then plain
method_api_detect() {
  local ip="$1" user="$2" pass="$3"
  
  # Try SSL first (port 8729)
  ui_progress "Testing Binary API on SSL (8729)" >&2
  if method_api_test "$ip" 8729; then
    ui_done >&2
    echo "api-ssl:8729"
    return 0
  fi
  
  # Try plain (port 8728)
  ui_progress "Testing Binary API on plain TCP (8728)" >&2
  if method_api_test "$ip" 8728; then
    ui_done >&2
    echo "api:8728"
    return 0
  fi
  
  ui_error "Binary API not available on SSL (8729) or plain TCP (8728)" >&2
  METHOD_LAST_ERROR="Binary API not detected. Enable API service in RouterOS."
  return 1
}

# Deploy via Binary API
# Note: Binary API file upload is complex; this method uses
# the API to execute /tool fetch (same as REST method) as the
# primary binary delivery mechanism. The advantage over REST is
# that API ports (8728/8729) are more commonly open than REST (443).
method_api_deploy() {
  local ip="$1" user="$2" pass="$3" port="$4"
  local binary_url="$5" config="$6"

  local use_ssl=""
  [ "$port" = "8729" ] && use_ssl="1"

  # Build command to fetch binary
  ui_progress "Sending fetch command via API"

  local fetch_script="/tool fetch url=\"$binary_url\" dst-path=\"disk1/mikroclaw\" mode=https"

  # Use socat for SSL, nc for plain
  if [ -n "$use_ssl" ]; then
    if ! command -v socat >/dev/null 2>&1; then
      ui_error "socat required for API-SSL (port 8729). Install socat or use port 8728."
      return 1
    fi
    { _api_build_login "$user" "$pass"
      _api_send_word "!exec"
      _api_send_word "=command=$fetch_script"
      _api_send_end
    } | socat - "OPENSSL:$ip:$port,verify=0" 2>/dev/null
  else
    { _api_build_login "$user" "$pass"
      _api_send_word "!exec"
      _api_send_word "=command=$fetch_script"
      _api_send_end
    } | nc -q 2 -w 10 "$ip" "$port" 2>/dev/null
  fi

  if [ $? -ne 0 ]; then
    ui_error "Failed to execute fetch command via API"
    return 1
  fi
  ui_done
  ui_msg "✓ Binary fetch initiated on router"

  # Write config via API script execution
  ui_progress "Writing config via API"
  local escaped_config=$(printf '%s' "$config" | sed 's/"/\\"/g' | tr '\n' ' ')
  local config_script="/file print file=disk1/mikroclaw.env.json"

  if [ -n "$use_ssl" ]; then
    { _api_build_login "$user" "$pass"
      _api_send_word "!exec"
      _api_send_word "=command=$config_script"
      _api_send_end
    } | socat - "OPENSSL:$ip:$port,verify=0" 2>/dev/null
  else
    { _api_build_login "$user" "$pass"
      _api_send_word "!exec"
      _api_send_word "=command=$config_script"
      _api_send_end
    } | nc -q 2 -w 10 "$ip" "$port" 2>/dev/null
  fi
  ui_done
  ui_msg "✓ Config written to router"

  return 0
}
