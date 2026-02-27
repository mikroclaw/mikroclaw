package config

import (
	"os"
	"testing"
)

func TestExpandEnv(t *testing.T) {
	// Set up test environment variables
	os.Setenv("TEST_HOST", "192.168.1.1")
	os.Setenv("TEST_PORT", "8729")
	os.Setenv("TEST_PASSWORD", "secret123")
	defer func() {
		os.Unsetenv("TEST_HOST")
		os.Unsetenv("TEST_PORT")
		os.Unsetenv("TEST_PASSWORD")
	}()

	tests := []struct {
		name     string
		input    string
		expected string
	}{
		{
			name:     "simple variable",
			input:    `{"host": "${TEST_HOST}"}`,
			expected: `{"host": "192.168.1.1"}`,
		},
		{
			name:     "multiple variables",
			input:    `{"host": "${TEST_HOST}", "port": ${TEST_PORT}}`,
			expected: `{"host": "192.168.1.1", "port": 8729}`,
		},
		{
			name:     "default value when set",
			input:    `{"host": "${TEST_HOST:-192.168.88.1}"}`,
			expected: `{"host": "192.168.1.1"}`,
		},
		{
			name:     "default value when unset",
			input:    `{"host": "${UNSET_VAR:-192.168.88.1}"}`,
			expected: `{"host": "192.168.88.1"}`,
		},
		{
			name:     "literal text when no env",
			input:    `{"name": "mikroclaw"}`,
			expected: `{"name": "mikroclaw"}`,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := ExpandEnv(tt.input)
			if result != tt.expected {
				t.Errorf("ExpandEnv(%q) = %q, want %q", tt.input, result, tt.expected)
			}
		})
	}
}

func TestValidate(t *testing.T) {
	tests := []struct {
		name      string
		config    Config
		wantError bool
		errorMsg  string
	}{
		{
			name: "valid config with port 7778 and TLS",
			config: Config{
				RouterOS: RouterOSConfig{
					Host:     "192.168.1.1",
					Port:     7778,
					Username: "admin",
					Password: "password123",
					UseTLS:   true,
				},
				Container: ContainerConfig{
					Name:  "mikroclaw",
					Image: "ghcr.io/openclaw/mikroclaw:latest",
				},
			},
			wantError: false,
		},
		{
			name: "port 7778 without TLS should fail",
			config: Config{
				RouterOS: RouterOSConfig{
					Host:     "192.168.1.1",
					Port:     7778,
					Username: "admin",
					Password: "password123",
					UseTLS:   false,
				},
				Container: ContainerConfig{
					Name:  "mikroclaw",
					Image: "ghcr.io/openclaw/mikroclaw:latest",
				},
			},
			wantError: true,
			errorMsg:  "port 7778 requires TLS",
		},
		{
			name: "missing host should fail",
			config: Config{
				RouterOS: RouterOSConfig{
					Host:     "",
					Port:     443,
					Username: "admin",
				},
				Container: ContainerConfig{
					Name:  "mikroclaw",
					Image: "ghcr.io/openclaw/mikroclaw:latest",
				},
			},
			wantError: true,
			errorMsg:  "host is required",
		},
		{
			name: "missing username should fail",
			config: Config{
				RouterOS: RouterOSConfig{
					Host:     "192.168.1.1",
					Port:     443,
					Username: "",
				},
				Container: ContainerConfig{
					Name:  "mikroclaw",
					Image: "ghcr.io/openclaw/mikroclaw:latest",
				},
			},
			wantError: true,
			errorMsg:  "username is required",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.config.Validate()
			if tt.wantError {
				if err == nil {
					t.Errorf("Expected error containing %q, got nil", tt.errorMsg)
				} else if tt.errorMsg != "" {
					if err.Error() != tt.errorMsg {
						// Allow partial match for error messages
						if len(err.Error()) < len(tt.errorMsg) || err.Error()[:len(tt.errorMsg)] != tt.errorMsg {
							// Just check it contains the expected text
							found := false
							for i := 0; i <= len(err.Error())-len(tt.errorMsg); i++ {
								if err.Error()[i:i+len(tt.errorMsg)] == tt.errorMsg {
									found = true
									break
								}
							}
							if !found {
								t.Errorf("Expected error containing %q, got %q", tt.errorMsg, err.Error())
							}
						}
					}
				}
			} else {
				if err != nil {
					t.Errorf("Expected no error, got %q", err.Error())
				}
			}
		})
	}
}
