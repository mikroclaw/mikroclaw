package tui

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"syscall"

	"github.com/openclaw/mikroclaw-installer/pkg/config"
	"golang.org/x/term"
)

// MenuEditor provides a menu-driven config editor
type MenuEditor struct {
	reader *bufio.Reader
	cfg    *config.Config
}

// NewMenuEditor creates a new menu editor
func NewMenuEditor(cfg *config.Config) *MenuEditor {
	return &MenuEditor{
		reader: bufio.NewReader(os.Stdin),
		cfg:    cfg,
	}
}

// Run starts the menu editor
func (m *MenuEditor) Run() *config.Config {
	for {
		m.showMainMenu()
		choice := m.readInput("")

		switch choice {
		case "1":
			m.editRouterOS()
		case "2":
			m.editContainer()
		case "3":
			m.editLLM()
		case "4":
			m.editBots()
		case "5":
			m.editAdvanced()
		case "s", "S":
			return m.cfg
		case "q", "Q":
			fmt.Println("Discarding changes.")
			return nil
		default:
			fmt.Println("Invalid option. Press Enter to continue...")
			m.readInput("")
		}
	}
}

func (m *MenuEditor) showMainMenu() {
	fmt.Println()
	fmt.Println("╔════════════════════════════════════════════════════════════════╗")
	fmt.Println("║              MikroClaw Config Editor                           ║")
	fmt.Println("╚════════════════════════════════════════════════════════════════╝")
	fmt.Println()
	fmt.Println("  1) RouterOS Connection")
	fmt.Printf("     Host: %s:%d | User: %s | TLS: %v\n", m.cfg.RouterOS.Host, m.cfg.RouterOS.Port, m.cfg.RouterOS.Username, m.cfg.RouterOS.UseTLS)
	fmt.Println()
	fmt.Println("  2) Container Settings")
	fmt.Printf("     Name: %s | Image: %s | AutoStart: %v\n", m.cfg.Container.Name, m.cfg.Container.Image, m.cfg.Container.AutoStart)
	fmt.Println()
	fmt.Println("  3) LLM Provider")
	fmt.Printf("     Provider: %s | Model: %s\n", m.cfg.MikroClaw.LLMProvider, m.cfg.MikroClaw.Model)
	if m.cfg.MikroClaw.APIKey != "" {
		fmt.Println("     API Key: [set]")
	} else {
		fmt.Println("     API Key: [not set]")
	}
	fmt.Println()
	fmt.Println("  4) Bot Integrations")
	if m.cfg.MikroClaw.TelegramToken != "" {
		fmt.Printf("     Telegram: [enabled] Allowlist: %s\n", m.cfg.MikroClaw.TelegramAllowlist)
	} else {
		fmt.Println("     Telegram: [disabled]")
	}
	if m.cfg.MikroClaw.DiscordWebhook != "" {
		fmt.Println("     Discord: [enabled]")
	}
	if m.cfg.MikroClaw.SlackWebhook != "" {
		fmt.Println("     Slack: [enabled]")
	}
	fmt.Println()
	fmt.Println("  5) Advanced Settings")
	fmt.Printf("     DryRun: %v | AutoFixVETH: %v\n", m.cfg.Deployment.DryRun, m.cfg.Deployment.AutoFixVETH)
	fmt.Println()
	fmt.Println("  S) Save and exit")
	fmt.Println("  Q) Quit without saving")
	fmt.Println()
	fmt.Print("Select option: ")
}

func (m *MenuEditor) editRouterOS() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("RouterOS Connection Settings")
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("  1) Host:", m.cfg.RouterOS.Host)
		fmt.Println("  2) Port:", m.cfg.RouterOS.Port)
		fmt.Println("  3) Username:", m.cfg.RouterOS.Username)
		if m.cfg.RouterOS.Password != "" {
			fmt.Println("  4) Password: [********]")
		} else {
			fmt.Println("  4) Password: [not set]")
		}
		fmt.Println("  5) Use TLS:", m.cfg.RouterOS.UseTLS)
		fmt.Println()
		fmt.Println("  B) Back to main menu")
		fmt.Println()
		fmt.Print("Select field to edit: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			m.cfg.RouterOS.Host = m.readInput("Enter host [%s]: ", m.cfg.RouterOS.Host)
		case "2":
			port := m.readInt("Enter port [%d]: ", m.cfg.RouterOS.Port)
			if port > 0 {
				m.cfg.RouterOS.Port = port
			}
		case "3":
			m.cfg.RouterOS.Username = m.readInput("Enter username [%s]: ", m.cfg.RouterOS.Username)
		case "4":
			pass := m.readPassword("Enter password (or press Enter to keep): ")
			if pass != "" {
				m.cfg.RouterOS.Password = pass
			}
		case "5":
			m.cfg.RouterOS.UseTLS = m.readBool("Use TLS", m.cfg.RouterOS.UseTLS)
		case "b", "B":
			return
		}
	}
}

func (m *MenuEditor) editContainer() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("Container Settings")
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("  1) Name:", m.cfg.Container.Name)
		fmt.Println("  2) Image:", m.cfg.Container.Image)
		fmt.Println("  3) Auto-start:", m.cfg.Container.AutoStart)
		fmt.Println()
		fmt.Println("  B) Back to main menu")
		fmt.Println()
		fmt.Print("Select field to edit: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			m.cfg.Container.Name = m.readInput("Enter container name [%s]: ", m.cfg.Container.Name)
		case "2":
			m.cfg.Container.Image = m.readInput("Enter image URL [%s]: ", m.cfg.Container.Image)
		case "3":
			m.cfg.Container.AutoStart = m.readBool("Auto-start container", m.cfg.Container.AutoStart)
		case "b", "B":
			return
		}
	}
}

func (m *MenuEditor) editLLM() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("LLM Provider Settings")
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("  1) Provider:", m.cfg.MikroClaw.LLMProvider)
		fmt.Println("  2) Base URL:", m.cfg.MikroClaw.BaseURL)
		fmt.Println("  3) Model:", m.cfg.MikroClaw.Model)
		if m.cfg.MikroClaw.APIKey != "" {
			fmt.Println("  4) API Key: [********]")
		} else {
			fmt.Println("  4) API Key: [not set]")
		}
		if m.cfg.MikroClaw.MemuKey != "" {
			fmt.Println("  5) Memu Key: [********]")
		} else {
			fmt.Println("  5) Memu Key: [not set]")
		}
		fmt.Println()
		fmt.Println("  B) Back to main menu")
		fmt.Println()
		fmt.Print("Select field to edit: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			fmt.Println("Available: openrouter, cerebras, zai, openai, custom")
			m.cfg.MikroClaw.LLMProvider = m.readInput("Enter provider [%s]: ", m.cfg.MikroClaw.LLMProvider)
		case "2":
			m.cfg.MikroClaw.BaseURL = m.readInput("Enter base URL [%s]: ", m.cfg.MikroClaw.BaseURL)
		case "3":
			m.cfg.MikroClaw.Model = m.readInput("Enter model [%s]: ", m.cfg.MikroClaw.Model)
		case "4":
			key := m.readPassword("Enter API key (or press Enter to keep): ")
			if key != "" {
				m.cfg.MikroClaw.APIKey = key
			}
		case "5":
			key := m.readPassword("Enter Memu key (or press Enter to keep): ")
			if key != "" {
				m.cfg.MikroClaw.MemuKey = key
			}
		case "b", "B":
			return
		}
	}
}

func (m *MenuEditor) editBots() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("Bot Integrations")
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("  1) Telegram Bot")
		if m.cfg.MikroClaw.TelegramToken != "" {
			fmt.Println("     Status: [enabled]")
			fmt.Printf("     Allowlist: %s\n", m.cfg.MikroClaw.TelegramAllowlist)
		} else {
			fmt.Println("     Status: [disabled]")
		}
		fmt.Println()
		fmt.Println("  2) Discord Webhook")
		if m.cfg.MikroClaw.DiscordWebhook != "" {
			fmt.Println("     Status: [enabled]")
		} else {
			fmt.Println("     Status: [disabled]")
		}
		fmt.Println()
		fmt.Println("  3) Slack Webhook")
		if m.cfg.MikroClaw.SlackWebhook != "" {
			fmt.Println("     Status: [enabled]")
		} else {
			fmt.Println("     Status: [disabled]")
		}
		fmt.Println()
		fmt.Println("  B) Back to main menu")
		fmt.Println()
		fmt.Print("Select bot to configure: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			m.editTelegram()
		case "2":
			m.editDiscord()
		case "3":
			m.editSlack()
		case "b", "B":
			return
		}
	}
}

func (m *MenuEditor) editTelegram() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("Telegram Bot Configuration")
		fmt.Println("────────────────────────────────────────────────────────────────")
		if m.cfg.MikroClaw.TelegramToken != "" {
			fmt.Println("  1) Token: [********]")
		} else {
			fmt.Println("  1) Token: [not set]")
		}
		fmt.Println("  2) Allowlist:", m.cfg.MikroClaw.TelegramAllowlist)
		fmt.Println()
		fmt.Println("  D) Disable Telegram bot")
		fmt.Println("  B) Back")
		fmt.Println()
		fmt.Print("Select option: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			token := m.readPassword("Enter bot token (or press Enter to keep): ")
			if token != "" {
				m.cfg.MikroClaw.TelegramToken = token
				if m.cfg.MikroClaw.TelegramAllowlist == "" {
					m.cfg.MikroClaw.TelegramAllowlist = "*"
				}
			}
		case "2":
			m.cfg.MikroClaw.TelegramAllowlist = m.readInput("Enter allowlist (comma-separated usernames, or * for all) [%s]: ", m.cfg.MikroClaw.TelegramAllowlist)
		case "d", "D":
			m.cfg.MikroClaw.TelegramToken = ""
			m.cfg.MikroClaw.TelegramAllowlist = ""
			fmt.Println("Telegram bot disabled.")
			fmt.Println("Press Enter to continue...")
			m.readInput("")
			return
		case "b", "B":
			return
		}
	}
}

func (m *MenuEditor) editDiscord() {
	fmt.Println()
	fmt.Println("────────────────────────────────────────────────────────────────")
	fmt.Println("Discord Webhook Configuration")
	fmt.Println("────────────────────────────────────────────────────────────────")

	if m.cfg.MikroClaw.DiscordWebhook != "" {
		fmt.Println("Current webhook: [set]")
		change := m.readBool("Change webhook", false)
		if !change {
			return
		}
	}

	webhook := m.readPassword("Enter Discord webhook URL (or press Enter to disable): ")
	if webhook == "" {
		m.cfg.MikroClaw.DiscordWebhook = ""
		fmt.Println("Discord disabled.")
	} else {
		m.cfg.MikroClaw.DiscordWebhook = webhook
		fmt.Println("Discord webhook set.")
	}
	fmt.Println("Press Enter to continue...")
	m.readInput("")
}

func (m *MenuEditor) editSlack() {
	fmt.Println()
	fmt.Println("────────────────────────────────────────────────────────────────")
	fmt.Println("Slack Webhook Configuration")
	fmt.Println("────────────────────────────────────────────────────────────────")

	if m.cfg.MikroClaw.SlackWebhook != "" {
		fmt.Println("Current webhook: [set]")
		change := m.readBool("Change webhook", false)
		if !change {
			return
		}
	}

	webhook := m.readPassword("Enter Slack webhook URL (or press Enter to disable): ")
	if webhook == "" {
		m.cfg.MikroClaw.SlackWebhook = ""
		fmt.Println("Slack disabled.")
	} else {
		m.cfg.MikroClaw.SlackWebhook = webhook
		fmt.Println("Slack webhook set.")
	}
	fmt.Println("Press Enter to continue...")
	m.readInput("")
}

func (m *MenuEditor) editAdvanced() {
	for {
		fmt.Println()
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("Advanced Settings")
		fmt.Println("────────────────────────────────────────────────────────────────")
		fmt.Println("  1) Dry run mode:", m.cfg.Deployment.DryRun)
		fmt.Println("  2) Auto-fix VETH:", m.cfg.Deployment.AutoFixVETH)
		fmt.Println("  3) Force overwrite:", m.cfg.Deployment.ForceOverwrite)
		fmt.Println()
		fmt.Println("  B) Back to main menu")
		fmt.Println()
		fmt.Print("Select field to edit: ")

		choice := m.readInput("")
		switch choice {
		case "1":
			m.cfg.Deployment.DryRun = m.readBool("Dry run mode", m.cfg.Deployment.DryRun)
		case "2":
			m.cfg.Deployment.AutoFixVETH = m.readBool("Auto-fix VETH", m.cfg.Deployment.AutoFixVETH)
		case "3":
			m.cfg.Deployment.ForceOverwrite = m.readBool("Force overwrite", m.cfg.Deployment.ForceOverwrite)
		case "b", "B":
			return
		}
	}
}

// Helper methods

func (m *MenuEditor) readInput(prompt string, args ...interface{}) string {
	if prompt != "" {
		if len(args) > 0 {
			fmt.Printf(prompt, args...)
		} else {
			fmt.Print(prompt)
		}
	}
	input, _ := m.reader.ReadString('\n')
	return strings.TrimSpace(input)
}

func (m *MenuEditor) readInt(prompt string, defaultVal int) int {
	fmt.Printf(prompt, defaultVal)
	input := m.readInput("")
	if input == "" {
		return defaultVal
	}
	var result int
	if _, err := fmt.Sscanf(input, "%d", &result); err != nil {
		return defaultVal
	}
	return result
}

func (m *MenuEditor) readBool(prompt string, defaultVal bool) bool {
	defaultStr := "Y/n"
	if !defaultVal {
		defaultStr = "y/N"
	}
	fmt.Printf("%s [%s]: ", prompt, defaultStr)
	input := strings.ToLower(m.readInput(""))
	if input == "" {
		return defaultVal
	}
	return input == "y" || input == "yes"
}

func (m *MenuEditor) readPassword(prompt string) string {
	fmt.Print(prompt)
	// Use terminal for hidden password input
	bytePassword, err := term.ReadPassword(int(syscall.Stdin))
	fmt.Println()
	if err != nil {
		// Fallback to visible input if terminal doesn't support hidden input
		return m.readInput("")
	}
	return string(bytePassword)
}
