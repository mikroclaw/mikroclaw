package preflight

import (
	"context"
	"testing"
)

// MockRouterOSClient is a mock implementation for testing
type MockRouterOSClient struct {
	version         string
	architecture    string
	containerConfig map[string]string
	vethInterfaces  []string
	registryError   error
}

func (m *MockRouterOSClient) GetVersion(ctx context.Context) (string, error) {
	return m.version, nil
}

func (m *MockRouterOSClient) GetArchitecture(ctx context.Context) (string, error) {
	return m.architecture, nil
}

func (m *MockRouterOSClient) GetContainerConfig(ctx context.Context) (map[string]string, error) {
	return m.containerConfig, nil
}

func (m *MockRouterOSClient) GetVETHInterfaces(ctx context.Context) ([]string, error) {
	return m.vethInterfaces, nil
}

func (m *MockRouterOSClient) TestRegistryConnectivity(ctx context.Context, registry string) error {
	return m.registryError
}

func TestCheckRouterOSVersion(t *testing.T) {
	tests := []struct {
		name           string
		version        string
		expectPassed   bool
		expectWarning  bool
		expectMessage  string
	}{
		{
			name:          "stable version 7.16",
			version:       "7.16",
			expectPassed:  true,
			expectMessage: "RouterOS v7.16 is stable for containers",
		},
		{
			name:          "problematic version 7.14",
			version:       "7.14",
			expectPassed:  false,
			expectMessage: "RouterOS v7.14 has critical container bugs",
		},
		{
			name:          "v7.21 with VETH changes",
			version:       "7.21",
			expectPassed:  true,
			expectWarning: true,
			expectMessage: "VETH breaking changes",
		},
		{
			name:          "old but supported 6.40",
			version:       "6.40",
			expectPassed:  true,
			expectMessage: "supports containers",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := &MockRouterOSClient{version: tt.version}
			checker := NewChecker(mock)

			result := checker.checkRouterOSVersion(context.Background())

			if result.Passed != tt.expectPassed {
				t.Errorf("Expected Passed=%v, got %v", tt.expectPassed, result.Passed)
			}
			if result.Warning != tt.expectWarning {
				t.Errorf("Expected Warning=%v, got %v", tt.expectWarning, result.Warning)
			}
		})
	}
}

func TestCheckArchitecture(t *testing.T) {
	tests := []struct {
		name          string
		architecture  string
		expectPassed  bool
		expectWarning bool
	}{
		{
			name:         "ARM64",
			architecture: "arm64",
			expectPassed: true,
		},
		{
			name:          "x86_64 CHR",
			architecture:  "x86_64",
			expectPassed:  true,
			expectWarning: true,
		},
		{
			name:          "ARM32",
			architecture:  "arm",
			expectPassed:  true,
			expectWarning: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := &MockRouterOSClient{architecture: tt.architecture}
			checker := NewChecker(mock)

			result := checker.checkArchitecture(context.Background())

			if result.Passed != tt.expectPassed {
				t.Errorf("Expected Passed=%v, got %v", tt.expectPassed, result.Passed)
			}
			if result.Warning != tt.expectWarning {
				t.Errorf("Expected Warning=%v, got %v", tt.expectWarning, result.Warning)
			}
		})
	}
}

func TestCheckContainerRuntime(t *testing.T) {
	tests := []struct {
		name         string
		config       map[string]string
		expectPassed bool
	}{
		{
			name:         "enabled",
			config:       map[string]string{"enabled": "true"},
			expectPassed: true,
		},
		{
			name:         "disabled",
			config:       map[string]string{"enabled": "false"},
			expectPassed: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := &MockRouterOSClient{containerConfig: tt.config}
			checker := NewChecker(mock)

			result := checker.checkContainerRuntime(context.Background())

			if result.Passed != tt.expectPassed {
				t.Errorf("Expected Passed=%v, got %v", tt.expectPassed, result.Passed)
			}
		})
	}
}

func TestParseVersion(t *testing.T) {
	tests := []struct {
		input    string
		expected float64
	}{
		{"7.21", 7.21},
		{"RouterOS v7.21.3", 7.21},
		{"v6.40.5 (stable)", 6.40},
		{"7.14beta", 7.14},
	}

	for _, tt := range tests {
		t.Run(tt.input, func(t *testing.T) {
			result := parseVersion(tt.input)
			if result != tt.expected {
				t.Errorf("parseVersion(%q) = %v, want %v", tt.input, result, tt.expected)
			}
		})
	}
}
