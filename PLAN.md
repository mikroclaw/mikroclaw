# MikroClaw GitHub CI Fix Plan

## Problem
The GitHub workflow (`.github/workflows/build.yml`) is configured for the OLD C-based project but we have a NEW Go-based installer in `installer-go/` subdirectory.

## Required Changes

### 1. Update .github/workflows/build.yml
Replace C-based builds with Go builds:
- Use `go build` instead of `make`
- Build in `installer-go/` subdirectory
- Add cross-platform builds (linux/darwin/windows, amd64/arm64)
- Add Go linting and formatting checks
- Remove C dependencies (mbedtls, libcurl)

### 2. Files to Modify
- `.github/workflows/build.yml` - Complete rewrite for Go

### 3. Build Commands
```bash
cd installer-go
go build -o mikroclaw-install ./cmd/mikroclaw-install/
go test ./...
```

### 4. Test Commands
```bash
cd installer-go
go test -v ./...
```

### 5. Acceptance Criteria
- [ ] GitHub Actions builds Go project successfully
- [ ] Tests pass in CI
- [ ] Binary artifacts uploaded for all platforms
- [ ] Code formatting enforced
