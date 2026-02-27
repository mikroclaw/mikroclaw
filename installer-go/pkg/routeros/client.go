package routeros

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// Client represents a RouterOS API client
type Client struct {
	host       string
	username   string
	password   string
	port       int
	useTLS     bool
	httpClient *http.Client
}

// ClientOption configures the RouterOS client
type ClientOption func(*Client)

// WithPort sets the API port
func WithPort(port int) ClientOption {
	return func(c *Client) {
		c.port = port
	}
}

// WithTLS enables TLS/HTTPS
func WithTLS(useTLS bool) ClientOption {
	return func(c *Client) {
		c.useTLS = useTLS
	}
}

// WithTimeout sets the HTTP timeout
func WithTimeout(timeout time.Duration) ClientOption {
	return func(c *Client) {
		c.httpClient.Timeout = timeout
	}
}

// NewClient creates a new RouterOS API client
func NewClient(host, username, password string, opts ...ClientOption) *Client {
	client := &Client{
		host:     host,
		username: username,
		password: password,
		port:     443,
		useTLS:   true,
		httpClient: &http.Client{
			Timeout: 30 * time.Second,
			Transport: &http.Transport{
				TLSClientConfig: &tls.Config{
					InsecureSkipVerify: true, // RouterOS often uses self-signed certs
				},
			},
		},
	}

	for _, opt := range opts {
		opt(client)
	}

	return client
}

// baseURL returns the base URL for API requests
func (c *Client) baseURL() string {
	scheme := "http"
	if c.useTLS {
		scheme = "https"
	}
	return fmt.Sprintf("%s://%s:%d/rest", scheme, c.host, c.port)
}

// doRequest makes an authenticated HTTP request
func (c *Client) doRequest(ctx context.Context, method, path string, body io.Reader) (*http.Response, error) {
	url := c.baseURL() + path

	req, err := http.NewRequestWithContext(ctx, method, url, body)
	if err != nil {
		return nil, fmt.Errorf("creating request: %w", err)
	}

	req.SetBasicAuth(c.username, c.password)
	req.Header.Set("Content-Type", "application/json")

	return c.httpClient.Do(req)
}

// GetVersion retrieves the RouterOS version
func (c *Client) GetVersion(ctx context.Context) (string, error) {
	resp, err := c.doRequest(ctx, "GET", "/system/resource", nil)
	if err != nil {
		return "", fmt.Errorf("fetching version: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return "", fmt.Errorf("decoding response: %w", err)
	}

	version, ok := result["version"].(string)
	if !ok {
		return "", fmt.Errorf("version not found in response")
	}

	return version, nil
}

// GetArchitecture retrieves the device architecture
func (c *Client) GetArchitecture(ctx context.Context) (string, error) {
	resp, err := c.doRequest(ctx, "GET", "/system/resource", nil)
	if err != nil {
		return "", fmt.Errorf("fetching architecture: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return "", fmt.Errorf("decoding response: %w", err)
	}

	arch, ok := result["architecture-name"].(string)
	if !ok {
		// Fallback to cpu
		arch, _ = result["cpu"].(string)
	}

	return arch, nil
}

// GetContainerConfig retrieves container configuration
func (c *Client) GetContainerConfig(ctx context.Context) (map[string]string, error) {
	resp, err := c.doRequest(ctx, "GET", "/container/config", nil)
	if err != nil {
		return nil, fmt.Errorf("fetching container config: %w", err)
	}
	defer resp.Body.Close()

	// Container package might not be installed
	if resp.StatusCode == http.StatusNotFound {
		return map[string]string{"enabled": "false", "error": "container package not installed"}, nil
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decoding response: %w", err)
	}

	config := make(map[string]string)
	for k, v := range result {
		if s, ok := v.(string); ok {
			config[k] = s
		}
	}

	return config, nil
}

// GetVETHInterfaces retrieves VETH interfaces
func (c *Client) GetVETHInterfaces(ctx context.Context) ([]string, error) {
	resp, err := c.doRequest(ctx, "GET", "/interface/veth", nil)
	if err != nil {
		return nil, fmt.Errorf("fetching VETH interfaces: %w", err)
	}
	defer resp.Body.Close()

	// VETH might not exist if container package not installed
	if resp.StatusCode == http.StatusNotFound {
		return []string{}, nil
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}

	var result []map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		// Single result comes as object, not array
		var single map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&single); err != nil {
			return nil, fmt.Errorf("decoding response: %w", err)
		}
		result = []map[string]interface{}{single}
	}

	var interfaces []string
	for _, item := range result {
		if name, ok := item["name"].(string); ok {
			interfaces = append(interfaces, name)
		}
	}

	return interfaces, nil
}

// TestRegistryConnectivity tests connectivity to a container registry
func (c *Client) TestRegistryConnectivity(ctx context.Context, registry string) error {
	// Use /tool/fetch to test connectivity
	// This is a simplified check - in reality we'd try to fetch from the registry
	data := fmt.Sprintf(`{"url": "%s/v2/", "mode": "https"}`, registry)

	resp, err := c.doRequest(ctx, "POST", "/tool/fetch", strings.NewReader(data))
	if err != nil {
		return fmt.Errorf("testing registry: %w", err)
	}
	defer resp.Body.Close()

	// We're just checking if the command was accepted
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("registry test failed: %s", string(body))
	}

	return nil
}

// EnableContainerRuntime enables the container runtime
func (c *Client) EnableContainerRuntime(ctx context.Context) error {
	data := `{"enabled": "true"}`

	resp, err := c.doRequest(ctx, "POST", "/container/config/set", strings.NewReader(data))
	if err != nil {
		return fmt.Errorf("enabling container runtime: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("failed to enable: %s", string(body))
	}

	return nil
}

// ImportContainer imports a container from a tarball
func (c *Client) ImportContainer(ctx context.Context, filePath string) error {
	data := fmt.Sprintf(`{"file": "%s"}`, filePath)

	resp, err := c.doRequest(ctx, "POST", "/container/add", strings.NewReader(data))
	if err != nil {
		return fmt.Errorf("importing container: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("import failed: %s", string(body))
	}

	return nil
}

// StartContainer starts a container
func (c *Client) StartContainer(ctx context.Context, name string) error {
	// Get container number first
	containers, err := c.ListContainers(ctx)
	if err != nil {
		return err
	}

	var containerNum string
	for _, c := range containers {
		if c["name"] == name {
			containerNum = c[".id"]
			break
		}
	}

	if containerNum == "" {
		return fmt.Errorf("container not found: %s", name)
	}

	data := fmt.Sprintf(`{"numbers": "%s"}`, containerNum)

	resp, err := c.doRequest(ctx, "POST", "/container/start", strings.NewReader(data))
	if err != nil {
		return fmt.Errorf("starting container: %w", err)
	}
	defer resp.Body.Close()

	return nil
}

// ListContainers lists all containers
func (c *Client) ListContainers(ctx context.Context) ([]map[string]string, error) {
	resp, err := c.doRequest(ctx, "GET", "/container", nil)
	if err != nil {
		return nil, fmt.Errorf("listing containers: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}

	var result []map[string]string
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decoding response: %w", err)
	}

	return result, nil
}
