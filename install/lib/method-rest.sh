#!/bin/sh
# REST API deployment method for MikroClaw installer
# Provides functions to deploy MikroClaw via RouterOS REST API

METHOD_LAST_ERROR=""

# Internal: Build Basic auth header
# Usage: _rest_build_auth user pass
_rest_build_auth() {
    local user="$1"
    local pass="$2"
    printf 'Basic %s' "$(printf '%s:%s' "$user" "$pass" | base64 | tr -d '\n')"
}

# Internal: Make REST API call
# Usage: _rest_call method host port auth_header path [data]
# Returns: HTTP status code on stdout, response body on stderr
_rest_call() {
    local method="$1"
    local host="$2"
    local port="$3"
    local auth_header="$4"
    local path="$5"
    local data="${6:-}"
    local protocol="https"
    
    if [ "$port" = "80" ]; then
        protocol="http"
    fi
    
    local url="${protocol}://${host}:${port}${path}"
    local http_code
    
    if [ "$method" = "GET" ]; then
        http_code=$(curl -fsk -w '%{http_code}' \
            -H "Authorization: $auth_header" \
            -H "Accept: application/json" \
            "$url" 2>/dev/null)
    elif [ "$method" = "POST" ]; then
        http_code=$(curl -fsk -w '%{http_code}' \
            -H "Authorization: $auth_header" \
            -H "Content-Type: application/json" \
            -H "Accept: application/json" \
            -d "$data" \
            "$url" 2>/dev/null)
    elif [ "$method" = "PUT" ]; then
        http_code=$(curl -fsk -w '%{http_code}' -X PUT \
            -H "Authorization: $auth_header" \
            -H "Content-Type: application/json" \
            -H "Accept: application/json" \
            -d "$data" \
            "$url" 2>/dev/null)
    else
        echo "000"
        return 1
    fi
    
    echo "$http_code"
}

# Test REST API connectivity with Basic auth
# Usage: method_rest_test host port user pass
# Returns: 0 if REST API is accessible, 1 otherwise
method_rest_test() {
    local host="$1"
    local port="$2"
    local user="$3"
    local pass="$4"
    
    METHOD_LAST_ERROR=""
    
    local auth_header
    auth_header=$(_rest_build_auth "$user" "$pass")
    
    local http_code
    http_code=$(_rest_call "GET" "$host" "$port" "$auth_header" "/rest/system/resource")
    
    case "$http_code" in
        "200")
            return 0
            ;;
        "401")
            METHOD_LAST_ERROR="Authentication failed. Verify router username/password."
            return 1
            ;;
        "000")
            METHOD_LAST_ERROR="Connection failed. REST API may not be enabled or host unreachable."
            return 1
            ;;
        *)
            METHOD_LAST_ERROR="REST API returned HTTP $http_code. Check REST API configuration."
            return 1
            ;;
    esac
}

# Try REST API on HTTPS (443) then HTTP (80)
# Usage: method_rest_detect host user pass
# Returns: "https:port" or "http:port" on stdout, empty if not found
method_rest_detect() {
    local host="$1"
    local user="$2"
    local pass="$3"
    
    METHOD_LAST_ERROR=""
    
    # Try HTTPS first (port 443)
    ui_progress "Testing REST API on HTTPS (443)" >&2
    if method_rest_test "$host" "443" "$user" "$pass"; then
        ui_done >&2
        echo "https:443"
        return 0
    fi
    
    # Try HTTP (port 80)
    ui_progress "Testing REST API on HTTP (80)" >&2
    if method_rest_test "$host" "80" "$user" "$pass"; then
        ui_done >&2
        echo "http:80"
        return 0
    fi
    
    ui_error "REST API not available on HTTPS (443) or HTTP (80)" >&2
    METHOD_LAST_ERROR="REST API not detected. Enable REST API in RouterOS WebFig or CLI."
    return 1
}

# Deploy via REST API + /tool fetch
# Usage: method_rest_deploy host port user pass binary_path config_path
# Returns: 0 on success, 1 on failure
method_rest_deploy() {
    local host="$1"
    local port="$2"
    local user="$3"
    local pass="$4"
    local binary_path="$5"
    local config_path="$6"
    
    METHOD_LAST_ERROR=""
    
    local auth_header
    auth_header=$(_rest_build_auth "$user" "$pass")
    
    local protocol="https"
    if [ "$port" = "80" ]; then
        protocol="http"
    fi
    
    # Get binary URL from GitHub releases
    local binary_url="https://github.com/mikroclaw/mikroclaw/releases/latest/download/mikroclaw-linux-x64"
    local dst_path="disk1/mikroclaw"
    
    # Step 1: Use /tool fetch to download binary
    ui_progress "Downloading binary via /tool/fetch"
    
    local fetch_body
    fetch_body="{\"url\":\"$binary_url\",\"dst-path\":\"$dst_path\",\"mode\":\"https\"}"
    
    local fetch_code
    fetch_code=$(_rest_call "POST" "$host" "$port" "$auth_header" "/rest/tool/fetch" "$fetch_body")
    
    if [ "$fetch_code" != "200" ]; then
        METHOD_LAST_ERROR="Failed to trigger binary download. HTTP $fetch_code"
        ui_error "$METHOD_LAST_ERROR"
        return 1
    fi
    ui_done
    
    # Step 2: Upload config via REST API file endpoint
    ui_progress "Uploading configuration"
    
    local config_content
    config_content=$(cat "$config_path" 2>/dev/null || echo "{}")
    
    # Escape the config content for JSON
    local escaped_config
    escaped_config=$(printf '%s' "$config_content" | sed 's/\\/\\\\/g; s/"/\\"/g; s/\t/\\t/g')
    
    local config_body
    config_body="{\"name\":\"disk1/mikroclaw.env.json\",\"content\":\"$escaped_config\"}"
    
    local config_code
    config_code=$(_rest_call "PUT" "$host" "$port" "$auth_header" "/rest/file" "$config_body")
    
    if [ "$config_code" != "200" ]; then
        # Fallback: Try script execution method
        ui_msg "  REST file upload failed (HTTP $config_code), trying script method"
        
        local script_cmd
        script_cmd="/file print file=mikroclaw.env.json; /file set mikroclaw.env.json content=$escaped_config"
        
        local script_body
        script_body="{\"script\":\"$script_cmd\"}"
        
        local script_code
        script_code=$(_rest_call "POST" "$host" "$port" "$auth_header" "/rest/system/script/run" "$script_body")
        
        if [ "$script_code" != "200" ]; then
            METHOD_LAST_ERROR="Failed to upload configuration. HTTP $script_code"
            ui_error "$METHOD_LAST_ERROR"
            return 1
        fi
    fi
    ui_done
    
    # Step 3: Set executable permissions and verify
    ui_progress "Setting permissions"
    
    local chmod_body
    chmod_body="{\"script\":\"/file set $dst_path attributes=\"\"rwxrwxrwx\"\"\"}"
    
    _rest_call "POST" "$host" "$port" "$auth_header" "/rest/system/script/run" "$chmod_body" >/dev/null 2>&1
    ui_done
    
    ui_msg "✓ Binary deployed to $dst_path"
    ui_msg "✓ Config deployed to disk1/mikroclaw.env.json"
    ui_msg ""
    ui_msg "Next steps:"
    ui_msg "  1. Configure RouterOS container to run the binary"
    ui_msg "  2. Set environment variables from the JSON config"
    
    return 0
}
