# Contributing to MikroClaw

Last updated: 2026-02-25

Thanks for contributing.

## Development Setup

```bash
git clone https://github.com/your-org/mikroclaw.git
cd mikroclaw
```

Install build requirements (Alpine example):

```bash
apk add gcc make musl-dev curl-dev mbedtls-dev
```

## Build

```bash
make clean && make
```

Build outputs:

- `mikroclaw` (default dynamic binary)
- `mikroclaw-static-musl` (`make static-musl`)
- `mikrotik-docker` (`make mikrotik-docker`)

## Verify Changes

Run full release verification before opening a PR:

```bash
./scripts/verify-release.sh
```

This script builds the project and runs the test suite in `tests/`.

## Project Structure

```text
mikroclaw/
  src/         C runtime source
  src/channels/ channel adapters and allowlist handling
  src/tasks/   async task handlers
  docs/        project documentation
  tests/       C test programs
  scripts/     build and release scripts
  vendor/      bundled third-party code
```

## Coding Guidelines

- Language: C99
- Return convention: `0` success, negative values for errors
- Prefer explicit bounds checks and fixed-size buffers
- Keep line length reasonable and favor readable control flow
- Follow existing naming/structure conventions in nearby files

## Pull Requests

1. Keep changes focused (single purpose per PR).
2. Update docs when behavior/config/API changes.
3. Include verification evidence (`make`, `./scripts/verify-release.sh`).
4. Note security-impacting changes explicitly.

## Security Reports

For vulnerabilities, follow `SECURITY.md` and report privately to maintainers first.

## License

By contributing, you agree your contributions are licensed under the MIT License.
