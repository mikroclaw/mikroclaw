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
    localai|ollama)
      [ -z "$base_url" ] && base_url="http://localhost:8080/v1"
      [ -z "$model" ] && model="llama3"
      ;;
    anthropic)
      [ -z "$base_url" ] && base_url="https://api.anthropic.com/v1"
      [ -z "$model" ] && model="claude-3-5-sonnet-latest"
      ;;
  esac
  
  # Generate JSON using jq if available, otherwise manual
  if command -v jq >/dev/null 2>&1; then
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
        allowlists: {
          telegram: "",
          discord: "",
          slack: ""
        },
        memory: {
          enabled: false,
          provider: null
        }
      }'
  else
    # Fallback to manual JSON (no jq)
    printf '{\n'
    printf '  "bot_token": "%s",\n' "$bot_token"
    printf '  "api_key": "%s",\n' "$api_key"
    printf '  "provider": "%s",\n' "$provider"
    printf '  "base_url": "%s",\n' "$base_url"
    printf '  "model": "%s",\n' "$model"
    printf '  "auth_type": "bearer",\n'
    printf '  "allowlists": {\n'
    printf '    "telegram": "",\n'
    printf '    "discord": "",\n'
    printf '    "slack": ""\n'
    printf '  },\n'
    printf '  "memory": {\n'
    printf '    "enabled": false,\n'
    printf '    "provider": null\n'
    printf '  }\n'
    printf '}\n'
  fi
}

config_add_memory() {
  local config="$1"
  local provider="$2"
  
  if command -v jq >/dev/null 2>&1; then
    echo "$config" | jq \
      --arg provider "$provider" \
      '.memory = {
        enabled: true,
        provider: $provider
      }'
  else
    # Simple string replacement for non-jq systems
    echo "$config" | sed 's/"enabled": false/"enabled": true/' | \
      sed "s/\"provider\": null/\"provider\": \"$provider\"/"
  fi
}
