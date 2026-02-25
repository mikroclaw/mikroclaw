# Security Policy

## Supported Versions

| Version | Supported |
|---|---|
| 0.3.x | Yes |
| < 0.3.0 | No |

## Reporting a Vulnerability

Report security issues privately to the maintainers before public disclosure.

Recommended report contents:

- Affected version/commit
- Reproduction steps
- Impact assessment
- Suggested mitigation

Target response timeline:

- Initial response: 72 hours
- Triage decision: 7 days

## Security Architecture Summary

- Gateway auth pairing (`POST /pair`) issuing short-lived bearer token
- Optional `PAIRING_REQUIRED=1` protection for non-health endpoints
- Per-IP rate limiting + lockout via `rate_limit.c`
- Sender allowlists for Telegram/Discord/Slack
- File tool workspace enforcement with forbidden path list
- Symlink escape prevention in path validation
- Tool command allowlist (`ALLOWED_SHELL_CMDS`)
- Optional RouterOS firewall auto-rule management
- Encrypted secret helper (`ENCRYPTED:v1:` format)

## Security Testing

Run:

```bash
make clean && make
./scripts/verify-release.sh
```

Manual checks:

```bash
grep -rn "TODO\|FIXME\|HACK" src/
grep -rn "sk-\|api_key.*=.*[a-zA-Z]" src/
```
