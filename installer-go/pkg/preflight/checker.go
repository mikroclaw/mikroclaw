package preflight

import (
	"context"
	"fmt"
	"regexp"
	"strings"
	"time"
)

// CheckResult represents the result of a pre-flight check
type CheckResult struct {
	Name      string
	Passed    bool
	Warning   bool
	Message   string
	Details   map[string]interface{}
	Timestamp time.Time
}

// Checker performs pre-flight checks before deployment
type Checker struct {
	client  RouterOSClient
	results []CheckResult
}

// RouterOSClient defines the interface for RouterOS operations
type RouterOSClient interface {
	GetVersion(ctx context.Context) (string, error)
	GetArchitecture(ctx context.Context) (string, error)
	GetContainerConfig(ctx context.Context) (map[string]string, error)
	GetVETHInterfaces(ctx context.Context) ([]string, error)
	TestRegistryConnectivity(ctx context.Context, registry string) error
}

// NewChecker creates a new pre-flight checker
func NewChecker(client RouterOSClient) *Checker {
	return &Checker{
		client:  client,
		results: make([]CheckResult, 0),
	}
}

// RunAllChecks executes all pre-flight checks
func (c *Checker) RunAllChecks(ctx context.Context) ([]CheckResult, error) {
	checks := []func(context.Context) CheckResult{
		c.checkRouterOSVersion,
		c.checkArchitecture,
		c.checkContainerRuntime,
		c.checkVETHConfiguration,
		c.checkResources,
	}

	for _, check := range checks {
		result := check(ctx)
		c.results = append(c.results, result)

		// Stop on critical failures
		if !result.Passed && !result.Warning {
			break
		}
	}

	return c.results, nil
}

// checkRouterOSVersion validates RouterOS version compatibility
func (c *Checker) checkRouterOSVersion(ctx context.Context) CheckResult {
	version, err := c.client.GetVersion(ctx)
	if err != nil {
		return CheckResult{
			Name:      "RouterOS Version",
			Passed:    false,
			Message:   fmt.Sprintf("Failed to get RouterOS version: %v", err),
			Timestamp: time.Now(),
		}
	}

	result := CheckResult{
		Name:      "RouterOS Version",
		Details:   map[string]interface{}{"version": version},
		Timestamp: time.Now(),
	}

	// Parse version number
	versionNum := parseVersion(version)

	// Check for problematic versions
	switch {
	case versionNum >= 7.14 && versionNum <= 7.15:
		result.Passed = false
		result.Warning = false
		result.Message = fmt.Sprintf("RouterOS v%.2f has critical container bugs. Upgrade to v7.16+ or downgrade to v7.13", versionNum)
	case versionNum >= 7.20 && versionNum <= 7.21:
		result.Passed = true
		result.Warning = true
		result.Message = fmt.Sprintf("RouterOS v%.2f has VETH breaking changes. Manual configuration may be required.", versionNum)
	case versionNum >= 7.16 && versionNum < 7.20:
		result.Passed = true
		result.Message = fmt.Sprintf("RouterOS v%.2f is stable for containers", versionNum)
	case versionNum >= 6.40:
		result.Passed = true
		result.Message = fmt.Sprintf("RouterOS v%.2f supports containers (REST API)", versionNum)
	default:
		result.Passed = false
		result.Message = fmt.Sprintf("RouterOS v%.2f may not support containers. Minimum recommended: v6.40", versionNum)
	}

	return result
}

// checkArchitecture validates device architecture
func (c *Checker) checkArchitecture(ctx context.Context) CheckResult {
	arch, err := c.client.GetArchitecture(ctx)
	if err != nil {
		return CheckResult{
			Name:      "Architecture",
			Passed:    false,
			Message:   fmt.Sprintf("Failed to get architecture: %v", err),
			Timestamp: time.Now(),
		}
	}

	result := CheckResult{
		Name:      "Architecture",
		Details:   map[string]interface{}{"architecture": arch},
		Timestamp: time.Now(),
	}

	switch strings.ToLower(arch) {
	case "arm64":
		result.Passed = true
		result.Message = "ARM64 architecture - Full container support"
	case "x86_64", "amd64":
		result.Passed = true
		result.Warning = true
		result.Message = "x86_64 architecture - CHR detected. CHR does not support containers!"
	case "arm":
		result.Passed = true
		result.Warning = true
		result.Message = "ARM32 architecture - Limited to ARMv5 containers"
	default:
		result.Passed = false
		result.Message = fmt.Sprintf("Architecture '%s' may not support containers", arch)
	}

	return result
}

// checkContainerRuntime verifies container runtime is enabled
func (c *Checker) checkContainerRuntime(ctx context.Context) CheckResult {
	config, err := c.client.GetContainerConfig(ctx)
	if err != nil {
		return CheckResult{
			Name:      "Container Runtime",
			Passed:    false,
			Message:   fmt.Sprintf("Failed to get container config: %v", err),
			Timestamp: time.Now(),
		}
	}

	details := make(map[string]interface{})
	for k, v := range config {
		details[k] = v
	}

	result := CheckResult{
		Name:      "Container Runtime",
		Details:   details,
		Timestamp: time.Now(),
	}

	enabled, ok := config["enabled"]
	if !ok || enabled != "true" {
		result.Passed = false
		result.Message = "Container runtime is not enabled. Run: /container/config/set enabled=yes"
		return result
	}

	result.Passed = true
	result.Message = "Container runtime is enabled"

	// Check registry configuration
	if registry, ok := config["registry-url"]; ok && registry != "" {
		result.Details["registry"] = registry
	}

	return result
}

// checkVETHConfiguration validates VETH setup (important for v7.20+)
func (c *Checker) checkVETHConfiguration(ctx context.Context) CheckResult {
	veths, err := c.client.GetVETHInterfaces(ctx)
	if err != nil {
		return CheckResult{
			Name:      "VETH Configuration",
			Passed:    false,
			Message:   fmt.Sprintf("Failed to get VETH interfaces: %v", err),
			Timestamp: time.Now(),
		}
	}

	result := CheckResult{
		Name:      "VETH Configuration",
		Details:   map[string]interface{}{"veth_count": len(veths)},
		Timestamp: time.Now(),
	}

	if len(veths) == 0 {
		result.Passed = true
		result.Warning = true
		result.Message = "No VETH interfaces found. For RouterOS v7.20+, VETH configuration is required for container networking."
		return result
	}

	result.Passed = true
	result.Message = fmt.Sprintf("Found %d VETH interface(s)", len(veths))
	return result
}

// checkResources validates available resources
func (c *Checker) checkResources(ctx context.Context) CheckResult {
	// This would check RAM, storage, etc.
	// For now, assume we can't query this easily via API
	return CheckResult{
		Name:      "Resources",
		Passed:    true,
		Message:   "Resource check skipped (manual verification recommended)",
		Timestamp: time.Now(),
	}
}

// GetResults returns all check results
func (c *Checker) GetResults() []CheckResult {
	return c.results
}

// HasFailures returns true if any check failed
func (c *Checker) HasFailures() bool {
	for _, r := range c.results {
		if !r.Passed && !r.Warning {
			return true
		}
	}
	return false
}

// HasWarnings returns true if any check has warnings
func (c *Checker) HasWarnings() bool {
	for _, r := range c.results {
		if r.Warning {
			return true
		}
	}
	return false
}

// parseVersion extracts version number from RouterOS version string
func parseVersion(version string) float64 {
	// Match patterns like "7.21", "v7.21", "RouterOS v7.21"
	re := regexp.MustCompile(`(\d+)\.(\d+)`)
	matches := re.FindStringSubmatch(version)
	if len(matches) >= 2 {
		var major, minor int
		if _, err := fmt.Sscanf(matches[0], "%d.%d", &major, &minor); err != nil {
			return 0.0
		}
		return float64(major) + float64(minor)/100.0
	}
	return 0.0
}
