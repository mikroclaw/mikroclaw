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
  local auth_type="bearer"
  
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
      [ -z "$base_url" ] && base_url="http://127.0.0.1:11434/v1"
      [ -z "$model" ] && model="llama3"
      ;;
    anthropic)
      [ -z "$base_url" ] && base_url="https://api.anthropic.com/v1"
      [ -z "$model" ] && model="claude-3-5-sonnet-latest"
      auth_type="api_key"
      ;;
    groq)
      [ -z "$base_url" ] && base_url="https://api.groq.com/openai"
      [ -z "$model" ] && model="llama3-70b-8192"
      ;;
    mistral)
      [ -z "$base_url" ] && base_url="https://api.mistral.ai"
      [ -z "$model" ] && model="mistral-large-latest"
      ;;
    xai)
      [ -z "$base_url" ] && base_url="https://api.x.ai/v1"
      [ -z "$model" ] && model="grok-beta"
      ;;
    deepseek)
      [ -z "$base_url" ] && base_url="https://api.deepseek.com"
      [ -z "$model" ] && model="deepseek-chat"
      ;;
    together)
      [ -z "$base_url" ] && base_url="https://api.together.xyz"
      [ -z "$model" ] && model="meta-llama/Llama-3-70b"
      ;;
    fireworks)
      [ -z "$base_url" ] && base_url="https://api.fireworks.ai/inference"
      [ -z "$model" ] && model="accounts/fireworks/models/llama-v3-70b"
      ;;
    perplexity)
      [ -z "$base_url" ] && base_url="https://api.perplexity.ai"
      [ -z "$model" ] && model="sonar-medium-online"
      ;;
    cohere)
      [ -z "$base_url" ] && base_url="https://api.cohere.com/compatibility"
      [ -z "$model" ] && model="command-r"
      auth_type="api_key"
      ;;
    bedrock)
      [ -z "$base_url" ] && base_url="https://bedrock-runtime.us-east-1.amazonaws.com"
      [ -z "$model" ] && model="anthropic.claude-3-sonnet"
      auth_type="api_key"
      ;;
    kimi)
      [ -z "$base_url" ] && base_url="https://api.moonshot.cn/v1"
      [ -z "$model" ] && model="moonshot-v1-8k"
      ;;
    minimax)
      [ -z "$base_url" ] && base_url="https://api.minimax.chat/v1"
      [ -z "$model" ] && model="abab6.5-chat"
      auth_type="api_key"
      ;;
    zai)
      [ -z "$base_url" ] && base_url="https://api.z.ai/v1"
      [ -z "$model" ] && model="zai-latest"
      ;;
    synthetic)
      [ -z "$base_url" ] && base_url="https://api.synthetic.new/v1"
      [ -z "$model" ] && model="synthetic-large"
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
      --arg auth_type "$auth_type" \
      '{
        BOT_TOKEN: $bot,
        LLM_API_KEY: $key,
        LLM_PROVIDER: $provider,
        LLM_BASE_URL: $url,
        MODEL: $model,
        AUTH_TYPE: $auth_type,
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
    printf '  "BOT_TOKEN": "%s",\n' "$bot_token"
    printf '  "LLM_API_KEY": "%s",\n' "$api_key"
    printf '  "LLM_PROVIDER": "%s",\n' "$provider"
    printf '  "LLM_BASE_URL": "%s",\n' "$base_url"
    printf '  "MODEL": "%s",\n' "$model"
    printf '  "AUTH_TYPE": "%s",\n' "$auth_type"
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
