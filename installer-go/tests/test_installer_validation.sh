#!/bin/sh
set -eu

SCRIPT="./install/mikroclaw-install.sh"

out_file="/tmp/mikroclaw-installer-test.out"

set +e
"$SCRIPT" --target routeros --ip "1.2.3.4;touch /tmp/pwn" --user "admin" --pass "x" >"$out_file" 2>&1
rc=$?
set -e

if [ "$rc" -eq 0 ]; then
  echo "FAIL: malicious IP accepted"
  exit 1
fi

if grep -q "Invalid router IP" "$out_file"; then
  echo "ALL PASS"
  exit 0
fi

echo "FAIL: expected invalid IP validation message"
cat "$out_file"
exit 1
