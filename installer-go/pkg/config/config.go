package config

import (
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"strings"
)

// Config represents the installer configuration
type Config struct {
	// RouterOS connection settings
	RouterOS RouterOSConfig `json:"routeros"`

	// Container settings
	Container ContainerConfig `json:"container"`

	// Registry settings
	Registry RegistryConfig `json:"registry"`

	// Deployment settings
	Deployment DeploymentConfig `json:"deployment"`

	// MikroClaw-specific settings
	MikroClaw MikroClawConfig `json:"mikroclaw"`
}

// MikroClawConfig contains MikroClaw runtime configuration
type MikroClawConfig struct {
	// MemU Cloud memory key
	MemuKey string `json:"memu_key,omitempty"`

	// LLM Provider settings
	LLMProvider string `json:"llm_provider,omitempty"`
	APIKey      string `json:"api_key,omitempty"`
	BaseURL     string `json:"base_url,omitempty"`
	Model       string `json:"model,omitempty"`

	// Telegram Bot
	TelegramToken     string `json:"telegram_token,omitempty"`
	TelegramAllowlist string `json:"telegram_allowlist,omitempty"`

	// Discord Integration
	DiscordWebhook string `json:"discord_webhook,omitempty"`

	// Slack Integration
	SlackWebhook string `json:"slack_webhook,omitempty"`
}

// RouterOSConfig contains RouterOS connection settings
type RouterOSConfig struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	Username string `json:"username"`
	Password string `json:"password"`
	UseTLS   bool   `json:"use_tls"`
}

// ContainerConfig contains container-specific settings
type ContainerConfig struct {
	Name        string            `json:"name"`
	Image       string            `json:"image"`
	File        string            `json:"file,omitempty"`
	EnvVars     map[string]string `json:"env_vars,omitempty"`
	Mounts      map[string]string `json:"mounts,omitempty"`
	Interface   string            `json:"interface,omitempty"`
	AutoStart   bool              `json:"auto_start"`
}

// RegistryConfig contains registry authentication settings
type RegistryConfig struct {
	URL      string `json:"url"`
	Username string `json:"username,omitempty"`
	Password string `json:"password,omitempty"`
	Token    string `json:"token,omitempty"`
}

// DeploymentConfig contains deployment behavior settings
type DeploymentConfig struct {
	DryRun         bool `json:"dry_run"`
	SkipPreflight  bool `json:"skip_preflight"`
	AutoFixVETH    bool `json:"auto_fix_veth"`
	ForceOverwrite bool `json:"force_overwrite"`
}

// DefaultConfig returns a default configuration
func DefaultConfig() *Config {
	return &Config{
		RouterOS: RouterOSConfig{
			Host:     "192.168.88.1",
			Port:     443,
			Username: "admin",
			UseTLS:   true,
		},
		Container: ContainerConfig{
			Name:      "mikroclaw",
			Image:     "ghcr.io/openclaw/mikroclaw:latest",
			AutoStart: true,
		},
		Registry: RegistryConfig{
			URL: "https://ghcr.io",
		},
		Deployment: DeploymentConfig{
			DryRun:         false,
			SkipPreflight:  false,
			AutoFixVETH:    false,
			ForceOverwrite: false,
		},
		MikroClaw: MikroClawConfig{
			LLMProvider: "openrouter",
			BaseURL:     "https://openrouter.ai/api/v1",
			Model:       "google/gemini-flash-1.5",
		},
	}
}

// LoadConfig loads configuration from a file with optional decryption
// Supports ${ENV_VAR} and ${ENV_VAR:-default} syntax for unencrypted configs
func LoadConfig(path string, password ...string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("reading config file: %w", err)
	}

	// Check if encrypted
	if IsEncrypted(data) {
		if len(password) == 0 || password[0] == "" {
			return nil, fmt.Errorf("config is encrypted - password required")
		}
		data, err = DecryptConfig(data, password[0])
		if err != nil {
			return nil, fmt.Errorf("decrypting config: %w", err)
		}
	}

	// Expand environment variables
	expanded := ExpandEnv(string(data))

	var config Config
	if err := json.Unmarshal([]byte(expanded), &config); err != nil {
		return nil, fmt.Errorf("parsing config: %w", err)
	}

	return &config, nil
}

// ExpandEnv replaces ${VAR} and ${VAR:-default} patterns with environment variable values
func ExpandEnv(input string) string {
	// Pattern: ${VAR} or ${VAR:-default}
	re := regexp.MustCompile(`\$\{([^}]+)\}`)

	return re.ReplaceAllStringFunc(input, func(match string) string {
		// Remove ${ and }
		content := match[2 : len(match)-1]

		// Check for default value syntax: VAR:-default
		if idx := strings.Index(content, ":-"); idx != -1 {
			varName := content[:idx]
			defaultValue := content[idx+2:]
			if value := os.Getenv(varName); value != "" {
				return value
			}
			return defaultValue
		}

		// Simple variable substitution
		if value := os.Getenv(content); value != "" {
			return value
		}

		// Return original if not found (allows literal ${...} in values if needed)
		return match
	})
}

// SaveConfig saves configuration to a file
func SaveConfig(config *Config, path string) error {
	data, err := json.MarshalIndent(config, "", "  ")
	if err != nil {
		return fmt.Errorf("encoding config: %w", err)
	}

	if err := os.WriteFile(path, data, 0600); err != nil {
		return fmt.Errorf("writing config file: %w", err)
	}

	return nil
}

// SaveConfigEncrypted saves configuration encrypted with a password
func SaveConfigEncrypted(config *Config, path string, password string) error {
	data, err := json.MarshalIndent(config, "", "  ")
	if err != nil {
		return fmt.Errorf("encoding config: %w", err)
	}

	encrypted, err := EncryptConfig(data, password)
	if err != nil {
		return fmt.Errorf("encrypting config: %w", err)
	}

	if err := os.WriteFile(path, encrypted, 0600); err != nil {
		return fmt.Errorf("writing config file: %w", err)
	}

	return nil
}

// Validate validates the configuration
func (c *Config) Validate() error {
	if c.RouterOS.Host == "" {
		return fmt.Errorf("routeros host is required")
	}

	if c.RouterOS.Username == "" {
		return fmt.Errorf("routeros username is required")
	}

	if c.Container.Name == "" {
		return fmt.Errorf("container name is required")
	}

	if c.Container.Image == "" && c.Container.File == "" {
		return fmt.Errorf("either container image or file must be specified")
	}

	// Port 7778 is the API SSL port - must use TLS
	if c.RouterOS.Port == 7778 && !c.RouterOS.UseTLS {
		return fmt.Errorf("port 7778 requires TLS - set use_tls to true")
	}

	// Warn about password security
	if c.RouterOS.Password != "" && len(c.RouterOS.Password) < 12 {
		fmt.Fprintf(os.Stderr, "Warning: RouterOS password is less than 12 characters\n")
	}

	return nil
}

// GenerateConfig generates a sample configuration file
func GenerateConfig(path string) error {
	config := DefaultConfig()

	// Add sample MikroClaw configuration
	config.MikroClaw.MemuKey = "your-memu-api-key"
	config.MikroClaw.APIKey = "your-llm-api-key"
	config.MikroClaw.TelegramToken = "your-telegram-bot-token"
	config.MikroClaw.TelegramAllowlist = "*"

	// Add sample mounts
	config.Container.Mounts = map[string]string{
		"disk1/mikroclaw-data": "/app/data",
	}

	return SaveConfig(config, path)
}

// GenerateSecureConfig generates a config template using environment variables
func GenerateSecureConfig(path string) error {
	template := `{
  "routeros": {
    "host": "${MIKROCLAW_ROUTEROS_HOST:-192.168.88.1}",
    "port": ${MIKROCLAW_ROUTEROS_PORT:-443},
    "username": "${MIKROCLAW_ROUTEROS_USERNAME:-admin}",
    "password": "${MIKROCLAW_ROUTEROS_PASSWORD}",
    "use_tls": true
  },
  "container": {
    "name": "mikroclaw",
    "image": "ghcr.io/openclaw/mikroclaw:latest",
    "auto_start": true,
    "mounts": {
      "disk1/mikroclaw-data": "/app/data"
    }
  },
  "registry": {
    "url": "https://ghcr.io"
  },
  "deployment": {
    "dry_run": false,
    "skip_preflight": false,
    "auto_fix_veth": true,
    "force_overwrite": false
  },
  "mikroclaw": {
    "llm_provider": "openrouter",
    "api_key": "${MIKROCLAW_API_KEY}",
    "base_url": "https://openrouter.ai/api/v1",
    "model": "google/gemini-flash-1.5",
    "memu_key": "${MIKROCLAW_MEMU_KEY}",
    "telegram_token": "${MIKROCLAW_TELEGRAM_TOKEN}",
    "telegram_allowlist": "*"
  }
}
`
	if err := os.WriteFile(path, []byte(template), 0600); err != nil {
		return fmt.Errorf("writing config file: %w", err)
	}
	return nil
}
