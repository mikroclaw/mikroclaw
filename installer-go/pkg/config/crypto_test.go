package config

import (
	"testing"
)

func TestEncryptDecrypt(t *testing.T) {
	plaintext := []byte(`{"routeros": {"host": "192.168.1.1", "password": "secret123"}}`)
	password := "my-encryption-password"

	// Encrypt
	encrypted, err := EncryptConfig(plaintext, password)
	if err != nil {
		t.Fatalf("EncryptConfig failed: %v", err)
	}

	// Verify it looks encrypted (contains : separator and base64)
	if !IsEncrypted(encrypted) {
		t.Error("IsEncrypted returned false for encrypted data")
	}

	// Decrypt
	decrypted, err := DecryptConfig(encrypted, password)
	if err != nil {
		t.Fatalf("DecryptConfig failed: %v", err)
	}

	if string(decrypted) != string(plaintext) {
		t.Errorf("Decrypted text doesn't match original. Got %q, want %q", decrypted, plaintext)
	}
}

func TestDecryptWrongPassword(t *testing.T) {
	plaintext := []byte(`{"password": "secret"}`)
	password := "correct-password"

	encrypted, err := EncryptConfig(plaintext, password)
	if err != nil {
		t.Fatalf("EncryptConfig failed: %v", err)
	}

	// Try decrypt with wrong password
	_, err = DecryptConfig(encrypted, "wrong-password")
	if err == nil {
		t.Error("DecryptConfig should fail with wrong password")
	}
}

func TestIsEncryptedPlaintext(t *testing.T) {
	plaintext := []byte(`{"host": "192.168.1.1"}`)

	if IsEncrypted(plaintext) {
		t.Error("IsEncrypted returned true for plaintext JSON")
	}
}

func TestSplitString(t *testing.T) {
	tests := []struct {
		input    string
		sep      byte
		expected []string
	}{
		{"hello:world", ':', []string{"hello", "world"}},
		{"no-separator", ':', []string{"no-separator"}},
		{"a:b:c", ':', []string{"a", "b:c"}},
		{"", ':', []string{""}},
	}

	for _, tt := range tests {
		result := splitString(tt.input, tt.sep)
		if len(result) != len(tt.expected) {
			t.Errorf("splitString(%q, %q) returned %d parts, expected %d",
				tt.input, string(tt.sep), len(result), len(tt.expected))
			continue
		}
		for i := range result {
			if result[i] != tt.expected[i] {
				t.Errorf("splitString(%q, %q)[%d] = %q, expected %q",
					tt.input, string(tt.sep), i, result[i], tt.expected[i])
			}
		}
	}
}
