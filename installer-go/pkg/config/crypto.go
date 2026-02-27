package config

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
	"io"

	"golang.org/x/crypto/argon2"
	"golang.org/x/crypto/chacha20poly1305"
)

// EncryptedConfig wraps an encrypted config with metadata
type EncryptedConfig struct {
	Version    int    `json:"version"`
	Salt       string `json:"salt"`
	Nonce      string `json:"nonce"`
	Ciphertext string `json:"ciphertext"`
}

const (
	argon2Time    = 3
	argon2Memory  = 64 * 1024
	argon2Threads = 4
	argon2KeyLen  = 32
)

// deriveKey derives an encryption key from password using Argon2id
func deriveKey(password string, salt []byte) []byte {
	return argon2.IDKey([]byte(password), salt, argon2Time, argon2Memory, argon2Threads, argon2KeyLen)
}

// EncryptConfig encrypts config JSON with a password
func EncryptConfig(plaintext []byte, password string) ([]byte, error) {
	// Generate salt
	salt := make([]byte, 16)
	if _, err := io.ReadFull(rand.Reader, salt); err != nil {
		return nil, fmt.Errorf("generating salt: %w", err)
	}

	// Derive key
	key := deriveKey(password, salt)

	// Create AEAD
	aead, err := chacha20poly1305.New(key)
	if err != nil {
		return nil, fmt.Errorf("creating cipher: %w", err)
	}

	// Generate nonce
	nonce := make([]byte, aead.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return nil, fmt.Errorf("generating nonce: %w", err)
	}

	// Encrypt
	ciphertext := aead.Seal(nonce, nonce, plaintext, nil)

	// Format: base64(salt) + ":" + base64(ciphertext)
	result := base64.StdEncoding.EncodeToString(salt) + ":" + base64.StdEncoding.EncodeToString(ciphertext)
	return []byte(result), nil
}

// DecryptConfig decrypts config JSON with a password
func DecryptConfig(encrypted []byte, password string) ([]byte, error) {
	// Parse format
	parts := splitString(string(encrypted), ':')
	if len(parts) != 2 {
		return nil, fmt.Errorf("invalid encrypted format")
	}

	salt, err := base64.StdEncoding.DecodeString(parts[0])
	if err != nil {
		return nil, fmt.Errorf("decoding salt: %w", err)
	}

	ciphertext, err := base64.StdEncoding.DecodeString(parts[1])
	if err != nil {
		return nil, fmt.Errorf("decoding ciphertext: %w", err)
	}

	// Derive key
	key := deriveKey(password, salt)

	// Create AEAD
	aead, err := chacha20poly1305.New(key)
	if err != nil {
		return nil, fmt.Errorf("creating cipher: %w", err)
	}

	// Extract nonce and decrypt
	if len(ciphertext) < aead.NonceSize() {
		return nil, fmt.Errorf("ciphertext too short")
	}

	nonce, ciphertext := ciphertext[:aead.NonceSize()], ciphertext[aead.NonceSize():]
	plaintext, err := aead.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return nil, fmt.Errorf("decrypting (wrong password?): %w", err)
	}

	return plaintext, nil
}

// IsEncrypted checks if data appears to be encrypted
func IsEncrypted(data []byte) bool {
	// Check if it matches our format: base64:base64
	parts := splitString(string(data), ':')
	if len(parts) != 2 {
		return false
	}
	// Try to decode both parts
	if _, err := base64.StdEncoding.DecodeString(parts[0]); err != nil {
		return false
	}
	if _, err := base64.StdEncoding.DecodeString(parts[1]); err != nil {
		return false
	}
	return true
}

// splitString is a helper that splits on first occurrence of sep
func splitString(s string, sep byte) []string {
	for i := 0; i < len(s); i++ {
		if s[i] == sep {
			return []string{s[:i], s[i+1:]}
		}
	}
	return []string{s}
}
