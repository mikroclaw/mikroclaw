# MikroClaw Release Policy

## Artifact types

### `mikroclaw` (dynamic)
- Size target: below 50 KB
- Dependencies: system libc and system mbedTLS
- Build command: `make`

### `mikroclaw-static-musl`
- Size target: below 500 KB
- Dependencies: none (fully static)
- Build command: `./scripts/build-musl.sh`

### `mikroclaw-minimal`
- Size target: below 100 KB (dynamic) or below 400 KB (static)
- Dependencies: libc with custom mbedTLS static libraries
- Build command: `make mbedtls-minimal && make mikroclaw-minimal`

## Release checklist

1. Run `./scripts/verify-release.sh` and require all checks to pass.
2. Confirm binary sizes meet target constraints.
3. Validate target runtime environments.
4. Validate HTTPS connectivity to required external services.
5. Tag and publish release notes.

## Versioning

Semantic versioning:
- MAJOR: breaking changes
- MINOR: backward-compatible features
- PATCH: bug fixes and security updates

## Security updates

For critical security fixes, publish a patch release immediately and document impacted components and mitigation in release notes.
