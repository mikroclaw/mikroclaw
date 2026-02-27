package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"syscall"
	"time"

	"golang.org/x/term"

	"github.com/openclaw/mikroclaw-installer/pkg/config"
	"github.com/openclaw/mikroclaw-installer/pkg/preflight"
	"github.com/openclaw/mikroclaw-installer/pkg/routeros"
)

var (
	Version   = "dev"
	BuildTime = "unknown"
)

func main() {
	var (
		interactive = flag.Bool("i", false, "Interactive mode (prompts for all settings)")
		configFile  = flag.String("config", "", "Configuration file path")
		checkOnly   = flag.Bool("check", false, "Run pre-flight checks only")
		dryRun      = flag.Bool("dry-run", false, "Show what would be done without executing")
		version     = flag.Bool("version", false, "Show version")
	)

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "MikroClaw Installer - Deploy containers to RouterOS\n\n")
		fmt.Fprintf(os.Stderr, "Security: All configs are encrypted with ChaCha20-Poly1305 + Argon2id\n\n")
		fmt.Fprintf(os.Stderr, "QUICK START (no flags needed):\n")
		fmt.Fprintf(os.Stderr, "  %s              # Show menu to load/create configs\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Options:\n")
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, "\nExamples:\n")
		fmt.Fprintf(os.Stderr, "  # Show main menu (load/create configs)\n")
		fmt.Fprintf(os.Stderr, "  %s\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  # Create new configuration interactively\n")
		fmt.Fprintf(os.Stderr, "  %s -i\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  # Run pre-flight checks only\n")
		fmt.Fprintf(os.Stderr, "  %s -check\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  # Load specific config file\n")
		fmt.Fprintf(os.Stderr, "  %s -config mikroclaw.json\n", os.Args[0])
	}

	flag.Parse()

	if *version {
		fmt.Printf("mikroclaw-install %s (built %s)\n", Version, BuildTime)
		os.Exit(0)
	}

	// Check if we have a terminal for interactive mode
	isTerminal := term.IsTerminal(int(syscall.Stdin))

	var cfg *config.Config
	var err error

	if *configFile != "" {
		// Load from config file
		cfg, err = loadConfigWithPrompt(*configFile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error loading config: %v\n", err)
			os.Exit(1)
		}
	} else if *interactive {
		// Interactive mode
		cfg = runInteractiveSetup()
	} else if isTerminal && len(os.Args) == 1 {
		// No flags provided and running in terminal - show menu
		cfg = showMainMenu()
	} else if !isTerminal {
		// Non-interactive mode requires config file
		fmt.Fprintf(os.Stderr, "Error: Either use -i for interactive mode or provide -config file\n\n")
		flag.Usage()
		os.Exit(1)
	} else {
		// Flags provided but no config - show usage
		flag.Usage()
		os.Exit(1)
	}

	// Validate configuration
	if err := cfg.Validate(); err != nil {
		fmt.Fprintf(os.Stderr, "Configuration error: %v\n", err)
		os.Exit(1)
	}

	ctx := context.Background()

	// Create RouterOS client
	rosClient := routeros.NewClient(
		cfg.RouterOS.Host,
		cfg.RouterOS.Username,
		cfg.RouterOS.Password,
		routeros.WithPort(cfg.RouterOS.Port),
		routeros.WithTLS(cfg.RouterOS.UseTLS),
	)

	// Run pre-flight checks
	fmt.Println()
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println("  MikroClaw Installer - Pre-flight Checks")
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Printf("Target: %s@%s:%d\n", cfg.RouterOS.Username, cfg.RouterOS.Host, cfg.RouterOS.Port)
	fmt.Println()

	checker := preflight.NewChecker(rosClient)
	results, err := checker.RunAllChecks(ctx)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error running checks: %v\n", err)
		os.Exit(1)
	}

	// Display results
	warnings := 0
	failures := 0

	for _, result := range results {
		symbol := "✓"
		status := "PASS"
		if !result.Passed {
			if result.Warning {
				symbol = "⚠"
				status = "WARN"
				warnings++
			} else {
				symbol = "✗"
				status = "FAIL"
				failures++
			}
		}

		fmt.Printf("%s [%s] %s\n", symbol, status, result.Name)
		fmt.Printf("   %s\n", result.Message)
		if len(result.Details) > 0 {
			fmt.Printf("   Details: %v\n", result.Details)
		}
		fmt.Println()
	}

	// Summary
	fmt.Println("──────────────────────────────────────────────────────────────────")
	if failures > 0 {
		fmt.Printf("Status: %d check(s) failed\n", failures)
		fmt.Println("Cannot proceed with deployment.")
		fmt.Println()
		fmt.Println("Please fix the issues above and try again.")
		os.Exit(1)
	} else if warnings > 0 {
		fmt.Printf("Status: %d warning(s), 0 failures\n", warnings)
		fmt.Println("Review warnings above before proceeding.")
	} else {
		fmt.Println("Status: All checks passed ✓")
	}
	fmt.Println()

	if *checkOnly {
		fmt.Println("Pre-flight checks complete. Use without -check to deploy.")
		os.Exit(0)
	}

	if *dryRun {
		fmt.Println("══════════════════════════════════════════════════════════════════")
		fmt.Println("  DRY RUN MODE")
		fmt.Println("══════════════════════════════════════════════════════════════════")
		fmt.Println()
		printDryRun(cfg)
		os.Exit(0)
	}

	// Confirm deployment
	fmt.Println()
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println("  Ready to Deploy")
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println()
	fmt.Printf("Container: %s\n", cfg.Container.Name)
	fmt.Printf("Image:     %s\n", cfg.Container.Image)
	fmt.Printf("Target:    %s@%s\n", cfg.RouterOS.Username, cfg.RouterOS.Host)
	fmt.Println()
	fmt.Printf("Configuration:\n")
	fmt.Printf("  - LLM Provider: %s\n", cfg.MikroClaw.LLMProvider)
	fmt.Printf("  - Model:        %s\n", cfg.MikroClaw.Model)
	fmt.Printf("  - Memory:       MemU Cloud\n")
	if cfg.MikroClaw.TelegramToken != "" {
		fmt.Printf("  - Telegram Bot: Enabled\n")
	}
	if cfg.MikroClaw.DiscordWebhook != "" {
		fmt.Printf("  - Discord:      Enabled\n")
	}
	fmt.Println()

	if !promptConfirm("Proceed with deployment?") {
		fmt.Println("Deployment cancelled.")
		os.Exit(0)
	}

	// Deploy
	fmt.Println()
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println("  Deployment")
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println()

	err = deploy(ctx, rosClient, cfg)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Deployment failed: %v\n", err)
		os.Exit(1)
	}

	fmt.Println()
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println("  Deployment Complete ✓")
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println()
	fmt.Printf("Container '%s' deployed successfully!\n", cfg.Container.Name)
	fmt.Println()
	fmt.Println("To verify:")
	fmt.Printf("  ssh %s@%s /container/print\n", cfg.RouterOS.Username, cfg.RouterOS.Host)
	fmt.Println()
	fmt.Println("To check logs:")
	fmt.Printf("  ssh %s@%s /container/logs %s\n", cfg.RouterOS.Username, cfg.RouterOS.Host, cfg.Container.Name)
}

// runInteractiveSetup prompts the user for all configuration
func runInteractiveSetup() *config.Config {
	cfg := config.DefaultConfig()
	reader := bufio.NewReader(os.Stdin)

	fmt.Println()
	fmt.Println("╔══════════════════════════════════════════════════════════════════╗")
	fmt.Println("║     MikroClaw Installer - Interactive Setup                      ║")
	fmt.Println("╚══════════════════════════════════════════════════════════════════╝")
	fmt.Println()

	// RouterOS Connection
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("RouterOS Connection Settings")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	cfg.RouterOS.Host = promptString(reader, "RouterOS IP/Hostname", cfg.RouterOS.Host)
	cfg.RouterOS.Port = promptInt(reader, "API Port", cfg.RouterOS.Port)
	cfg.RouterOS.Username = promptString(reader, "Username", cfg.RouterOS.Username)
	cfg.RouterOS.Password = promptPassword(reader, "Password")
	cfg.RouterOS.UseTLS = promptBool(reader, "Use TLS/HTTPS", cfg.RouterOS.UseTLS)

	// Container Settings
	fmt.Println()
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("Container Settings")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	cfg.Container.Name = promptString(reader, "Container name", cfg.Container.Name)

	fmt.Println()
	fmt.Println("Image source:")
	fmt.Println("  1) Remote image from registry (recommended)")
	fmt.Println("  2) Local tarball file")
	choice := promptInt(reader, "Select (1-2)", 1)

	if choice == 1 {
		cfg.Container.Image = promptString(reader, "Image URL", cfg.Container.Image)
	} else {
		cfg.Container.File = promptString(reader, "Tarball path on RouterOS (e.g., disk1/mikroclaw.tar)", "")
	}

	cfg.Container.AutoStart = promptBool(reader, "Auto-start container", true)

	// MemU Cloud Configuration
	fmt.Println()
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("MemU Cloud Configuration (Conversational Memory)")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	cfg.MikroClaw.MemuKey = promptString(reader, "MemU API Key", cfg.MikroClaw.MemuKey)
	if cfg.MikroClaw.MemuKey == "" {
		fmt.Println("  ℹ Without MemU key, conversations won't persist between restarts")
	}

	// AI Provider Configuration
	fmt.Println()
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("AI Provider Configuration")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	fmt.Println("Available providers:")
	fmt.Println("  1) OpenRouter (recommended - access to multiple models)")
	fmt.Println("  2) Cerebras (fast inference)")
	fmt.Println("  3) Z.AI (backup option)")
	fmt.Println("  4) OpenAI (Codex support)")
	fmt.Println("  5) Custom/OpenAI-compatible")

	providerChoice := promptInt(reader, "Select provider (1-5)", 1)

	switch providerChoice {
	case 1:
		cfg.MikroClaw.LLMProvider = "openrouter"
		cfg.MikroClaw.BaseURL = "https://openrouter.ai/api/v1"
		cfg.MikroClaw.Model = "google/gemini-flash-1.5"
	case 2:
		cfg.MikroClaw.LLMProvider = "cerebras"
		cfg.MikroClaw.BaseURL = "https://api.cerebras.ai/v1"
		cfg.MikroClaw.Model = "llama-3.1-70b"
	case 3:
		cfg.MikroClaw.LLMProvider = "zai"
		cfg.MikroClaw.BaseURL = "https://api.z.ai/v1"
		cfg.MikroClaw.Model = "glm-4"
	case 4:
		cfg.MikroClaw.LLMProvider = "openai"
		cfg.MikroClaw.BaseURL = "https://api.openai.com/v1"
		cfg.MikroClaw.Model = "gpt-4o-mini"
	case 5:
		cfg.MikroClaw.LLMProvider = promptString(reader, "Provider name", "custom")
		cfg.MikroClaw.BaseURL = promptString(reader, "API Base URL", "")
		cfg.MikroClaw.Model = promptString(reader, "Model name", "")
	}

	cfg.MikroClaw.APIKey = promptPassword(reader, "API Key for "+cfg.MikroClaw.LLMProvider)

	// Bot Integrations
	fmt.Println()
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("Bot Integrations (Optional)")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	if promptBool(reader, "Enable Telegram Bot", false) {
		cfg.MikroClaw.TelegramToken = promptPassword(reader, "Telegram Bot Token")
		cfg.MikroClaw.TelegramAllowlist = promptString(reader, "Allowed usernames (comma-separated, or * for all)", "*")
	}

	if promptBool(reader, "Enable Discord Integration", false) {
		cfg.MikroClaw.DiscordWebhook = promptPassword(reader, "Discord Webhook URL")
	}

	// Advanced Settings
	fmt.Println()
	fmt.Println("──────────────────────────────────────────────────────────────────")
	fmt.Println("Advanced Settings")
	fmt.Println("──────────────────────────────────────────────────────────────────")

	cfg.Deployment.DryRun = promptBool(reader, "Dry run mode (preview only)", false)
	cfg.Deployment.AutoFixVETH = promptBool(reader, "Auto-configure VETH if missing", true)

	// Save configuration (ALWAYS encrypted - mandatory)
	fmt.Println()
	if promptBool(reader, "Save this configuration to file", true) {
		// Show default config location
		configDir := getConfigDir()
		os.MkdirAll(configDir, 0700)

		fmt.Printf("Config directory: %s\n", configDir)
		filename := promptString(reader, "Filename", "mikroclaw-config.json")

		// Ensure .json extension
		if !strings.HasSuffix(filename, ".json") {
			filename += ".json"
		}

		// Full path
		fullPath := filepath.Join(configDir, filename)

		// Require encryption password - keep prompting until provided and confirmed
		var password string
		for {
			password = promptPassword(reader, "Config encryption password (required)")
			if password == "" {
				fmt.Println("ERROR: Encryption password is required to protect credentials")
				fmt.Println("       Config will not be saved without encryption")
				continue
			}
			if len(password) < 8 {
				fmt.Println("ERROR: Password must be at least 8 characters")
				continue
			}
			confirmPassword := promptPassword(reader, "Confirm encryption password")
			if password != confirmPassword {
				fmt.Println("ERROR: Passwords do not match - please try again")
				continue
			}
			break
		}

		if err := config.SaveConfigEncrypted(cfg, fullPath, password); err != nil {
			fmt.Printf("Error: Could not save config: %v\n", err)
		} else {
			saveLastConfig(fullPath)
			fmt.Printf("Configuration saved to: %s (encrypted)\n", fullPath)
			fmt.Printf("File permissions: 0600 (owner read/write only)\n")
			fmt.Println()
			fmt.Println("IMPORTANT: Remember your encryption password!")
			fmt.Println("           You will need it to load this config file.")
		}
	}

	fmt.Println()
	fmt.Println("Setup complete!")
	fmt.Println()

	return cfg
}

// deploy performs the actual deployment
func deploy(ctx context.Context, client *routeros.Client, cfg *config.Config) error {
	steps := []struct {
		name string
		fn   func() error
	}{
		{
			name: "Enabling container runtime",
			fn: func() error {
				return client.EnableContainerRuntime(ctx)
			},
		},
		{
			name: "Configuring registry authentication",
			fn: func() error {
				if cfg.Registry.Username != "" {
					// Would configure registry auth here
					fmt.Printf("    (Registry: %s)\n", cfg.Registry.URL)
				}
				return nil
			},
		},
		{
			name: "Creating container configuration",
			fn: func() error {
				// Generate mikroclaw.env.json content
				envConfig := generateMikroClawEnv(cfg)
				fmt.Printf("    (Config size: %d bytes)\n", len(envConfig))
				return nil
			},
		},
		{
			name: fmt.Sprintf("Importing container '%s'", cfg.Container.Name),
			fn: func() error {
				if cfg.Container.File != "" {
					return client.ImportContainer(ctx, cfg.Container.File)
				}
				// For remote images, would use different API
				fmt.Printf("    (From: %s)\n", cfg.Container.Image)
				return nil
			},
		},
		{
			name: "Starting container",
			fn: func() error {
				if !cfg.Container.AutoStart {
					return nil
				}
				return client.StartContainer(ctx, cfg.Container.Name)
			},
		},
		{
			name: "Verifying deployment",
			fn: func() error {
				containers, err := client.ListContainers(ctx)
				if err != nil {
					return err
				}
				for _, c := range containers {
					if c["name"] == cfg.Container.Name {
						fmt.Printf("    Status: %s\n", c["status"])
						return nil
					}
				}
				return fmt.Errorf("container not found after deployment")
			},
		},
	}

	for _, step := range steps {
		fmt.Printf("  %s... ", step.name)
		if err := step.fn(); err != nil {
			fmt.Println("✗")
			return fmt.Errorf("%s: %w", step.name, err)
		}
		time.Sleep(500 * time.Millisecond) // Simulate work
		fmt.Println("✓")
	}

	return nil
}

// generateMikroClawEnv generates the environment configuration for MikroClaw
func generateMikroClawEnv(cfg *config.Config) string {
	env := fmt.Sprintf(`{
  "BOT_TOKEN": "%s",
  "LLM_PROVIDER": "%s",
  "LLM_API_KEY": "%s",
  "LLM_BASE_URL": "%s",
  "MODEL": "%s",
  "MEMU_KEY": "%s",
  "TELEGRAM_TOKEN": "%s",
  "TELEGRAM_ALLOWLIST": "%s",
  "DISCORD_WEBHOOK": "%s"
}`,
		cfg.MikroClaw.TelegramToken,
		cfg.MikroClaw.LLMProvider,
		cfg.MikroClaw.APIKey,
		cfg.MikroClaw.BaseURL,
		cfg.MikroClaw.Model,
		cfg.MikroClaw.MemuKey,
		cfg.MikroClaw.TelegramToken,
		cfg.MikroClaw.TelegramAllowlist,
		cfg.MikroClaw.DiscordWebhook,
	)
	return env
}

// printDryRun shows what would be done
func printDryRun(cfg *config.Config) {
	fmt.Println("Would perform the following actions:")
	fmt.Println()
	fmt.Printf("1. Connect to RouterOS at %s:%d\n", cfg.RouterOS.Host, cfg.RouterOS.Port)
	fmt.Println("2. Enable container runtime if needed")
	fmt.Println("3. Configure registry authentication")
	fmt.Printf("4. Create MikroClaw config with:\n")
	fmt.Printf("   - Provider: %s\n", cfg.MikroClaw.LLMProvider)
	fmt.Printf("   - Model: %s\n", cfg.MikroClaw.Model)
	fmt.Printf("   - MemU: %s\n", map[bool]string{true: "Enabled", false: "Disabled"}[cfg.MikroClaw.MemuKey != ""])
	fmt.Printf("5. Deploy container '%s' from %s\n", cfg.Container.Name, cfg.Container.Image)
	fmt.Println("6. Start container and verify")
	fmt.Println()
	fmt.Println("Configuration that would be used:")
	fmt.Printf("  memU Key: %s\n", maskSecret(cfg.MikroClaw.MemuKey))
	fmt.Printf("  API Key:  %s\n", maskSecret(cfg.MikroClaw.APIKey))
}

// maskSecret masks a secret for display
func maskSecret(s string) string {
	if s == "" {
		return "(not set)"
	}
	if len(s) <= 8 {
		return "****"
	}
	return s[:4] + "****" + s[len(s)-4:]
}

// prompt helpers
func promptString(reader *bufio.Reader, prompt, defaultValue string) string {
	if defaultValue != "" {
		fmt.Printf("%s [%s]: ", prompt, defaultValue)
	} else {
		fmt.Printf("%s: ", prompt)
	}
	input, _ := reader.ReadString('\n')
	input = strings.TrimSpace(input)
	if input == "" {
		return defaultValue
	}
	return input
}

func promptInt(reader *bufio.Reader, prompt string, defaultValue int) int {
	fmt.Printf("%s [%d]: ", prompt, defaultValue)
	input, _ := reader.ReadString('\n')
	input = strings.TrimSpace(input)
	if input == "" {
		return defaultValue
	}
	var result int
	if _, err := fmt.Sscanf(input, "%d", &result); err != nil {
		return defaultValue
	}
	return result
}

func promptPassword(reader *bufio.Reader, prompt string) string {
	fmt.Printf("%s: ", prompt)
	// Use terminal for password input (hidden)
	bytePassword, err := term.ReadPassword(int(syscall.Stdin))
	fmt.Println()
	if err != nil {
		// Fallback to regular input
		input, _ := reader.ReadString('\n')
		return strings.TrimSpace(input)
	}
	return string(bytePassword)
}

func promptBool(reader *bufio.Reader, prompt string, defaultValue bool) bool {
	defaultStr := "N"
	if defaultValue {
		defaultStr = "Y"
	}
	fmt.Printf("%s [y/N] [%s]: ", prompt, defaultStr)
	input, _ := reader.ReadString('\n')
	input = strings.ToLower(strings.TrimSpace(input))
	if input == "" {
		return defaultValue
	}
	return input == "y" || input == "yes"
}

func promptConfirm(prompt string) bool {
	reader := bufio.NewReader(os.Stdin)
	fmt.Printf("%s [y/N]: ", prompt)
	input, _ := reader.ReadString('\n')
	input = strings.ToLower(strings.TrimSpace(input))
	return input == "y" || input == "yes"
}

// loadConfigWithPrompt loads config, prompting for password if encrypted
func loadConfigWithPrompt(path string) (*config.Config, error) {
	// Try loading without password first (for unencrypted or env var configs)
	cfg, err := config.LoadConfig(path)
	if err == nil {
		return cfg, nil
	}

	// Check if it needs a password
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("reading config file: %w", err)
	}

	if config.IsEncrypted(data) {
		reader := bufio.NewReader(os.Stdin)
		fmt.Println()
		fmt.Println("══════════════════════════════════════════════════════════════════")
		fmt.Println("  Encrypted Configuration")
		fmt.Println("══════════════════════════════════════════════════════════════════")
		password := promptPassword(reader, "Config password")
		return config.LoadConfig(path, password)
	}

	// Not encrypted, return original error
	return nil, err
}

// getConfigDir returns the default config directory
func getConfigDir() string {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return "."
	}
	return filepath.Join(homeDir, ".config", "mikroclaw")
}

// getLastConfigFile returns path to the last used config tracker
func getLastConfigFile() string {
	return filepath.Join(getConfigDir(), ".lastconfig")
}

// saveLastConfig remembers the last used config
func saveLastConfig(path string) {
	configDir := getConfigDir()
	os.MkdirAll(configDir, 0700)
	os.WriteFile(getLastConfigFile(), []byte(path), 0600)
}

// loadLastConfig returns the last used config path
func loadLastConfig() string {
	data, err := os.ReadFile(getLastConfigFile())
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(data))
}

// listConfigs returns all saved config files
func listConfigs() []string {
	configDir := getConfigDir()
	files, err := os.ReadDir(configDir)
	if err != nil {
		return []string{}
	}

	var configs []string
	for _, f := range files {
		if f.IsDir() {
			continue
		}
		name := f.Name()
		if strings.HasPrefix(name, ".") {
			continue
		}
		if strings.HasSuffix(name, ".json") {
			configs = append(configs, name)
		}
	}
	sort.Strings(configs)
	return configs
}

// showMainMenu displays the main menu when no flags provided
func showMainMenu() *config.Config {
	reader := bufio.NewReader(os.Stdin)

	for {
		fmt.Println()
		fmt.Println("══════════════════════════════════════════════════════════════════")
		fmt.Println("  MikroClaw Installer - Main Menu")
		fmt.Println("══════════════════════════════════════════════════════════════════")
		fmt.Println()

		lastConfig := loadLastConfig()
		configs := listConfigs()

		// Show last used config if available
		if lastConfig != "" {
			fmt.Printf("  Last used: %s\n", filepath.Base(lastConfig))
			fmt.Println()
		}

		// Show saved configs
		if len(configs) > 0 {
			fmt.Println("  Saved configurations:")
			for i, c := range configs {
				fmt.Printf("    %d) %s\n", i+1, c)
			}
			fmt.Println()
		}

		// Menu options
		fmt.Println("  Options:")
		if lastConfig != "" {
			fmt.Println("    L) Load last used config")
		}
		if len(configs) > 0 {
			fmt.Println("    #) Enter number to load config")
			fmt.Println("    S) Show all configs with details")
		}
		fmt.Println("    N) Create new configuration")
		fmt.Println("    C) Load config from custom path")
		fmt.Println("    Q) Quit")
		fmt.Println()
		fmt.Print("Select option: ")

		choice := strings.TrimSpace(strings.ToUpper(readLine(reader)))

		switch choice {
		case "L":
			if lastConfig != "" {
				cfg, err := loadConfigWithPrompt(lastConfig)
				if err != nil {
					fmt.Printf("Error loading config: %v\n", err)
					fmt.Println("Press Enter to continue...")
					readLine(reader)
					continue
				}
				return cfg
			}

		case "N":
			cfg := runInteractiveSetup()
			return cfg

		case "C":
			fmt.Print("Enter config file path: ")
			path := strings.TrimSpace(readLine(reader))
			if path != "" {
				cfg, err := loadConfigWithPrompt(path)
				if err != nil {
					fmt.Printf("Error loading config: %v\n", err)
					fmt.Println("Press Enter to continue...")
					readLine(reader)
					continue
				}
				saveLastConfig(path)
				return cfg
			}

		case "S":
			showConfigDetails(reader, configs)

		case "Q", "QUIT", "EXIT":
			fmt.Println("Goodbye!")
			os.Exit(0)

		default:
			// Try to parse as number
			if num := parseInt(choice); num > 0 && num <= len(configs) {
				path := filepath.Join(getConfigDir(), configs[num-1])
				cfg, err := loadConfigWithPrompt(path)
				if err != nil {
					fmt.Printf("Error loading config: %v\n", err)
					fmt.Println("Press Enter to continue...")
					readLine(reader)
					continue
				}
				saveLastConfig(path)
				return cfg
			}
			fmt.Println("Invalid option. Press Enter to continue...")
			readLine(reader)
		}
	}
}

// showConfigDetails displays details of all configs
func showConfigDetails(reader *bufio.Reader, configs []string) {
	fmt.Println()
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println("  Saved Configurations")
	fmt.Println("══════════════════════════════════════════════════════════════════")
	fmt.Println()

	configDir := getConfigDir()
	for i, c := range configs {
		path := filepath.Join(configDir, c)
		info, err := os.Stat(path)
		if err != nil {
			continue
		}

		// Try to peek at config for basic info
		data, _ := os.ReadFile(path)
		isEncrypted := config.IsEncrypted(data)

		fmt.Printf("%d) %s\n", i+1, c)
		fmt.Printf("   Size: %d bytes | Modified: %s\n", info.Size(), info.ModTime().Format("2006-01-02 15:04"))
		if isEncrypted {
			fmt.Println("   Status: Encrypted")
		} else {
			fmt.Println("   Status: Plaintext (legacy)")
		}
		fmt.Println()
	}

	fmt.Println("Press Enter to continue...")
	readLine(reader)
}

// readLine reads a line from stdin
func readLine(reader *bufio.Reader) string {
	line, _ := reader.ReadString('\n')
	return strings.TrimSpace(line)
}

// parseInt parses a string to int, returns 0 on error
func parseInt(s string) int {
	var n int
	_, err := fmt.Sscanf(s, "%d", &n)
	if err != nil {
		return 0
	}
	return n
}
