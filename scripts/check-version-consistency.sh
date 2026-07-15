#!/usr/bin/env bash
# Checks that all six version-bearing files agree on FWVERSION.
# The release workflow in CLAUDE.md lists these six edits; this script is the
# CI guard so a missed edit fails the PR instead of shipping inconsistent docs.
set -euo pipefail

cd "$(dirname "$0")/.."

FW=$(sed -nE 's/^#define[[:space:]]+FWVERSION[[:space:]]+"([0-9]+(\.[0-9]+)*)".*/\1/p' Firmware/include/mrfconstants.h | head -n 1)
if [ -z "$FW" ]; then
  echo "FAIL: could not extract FWVERSION from Firmware/include/mrfconstants.h"
  exit 1
fi
FW_RE=${FW//./\\.}
echo "FWVERSION: $FW"

fail=0

check() {
  local file="$1" pattern="$2" desc="$3"
  if grep -qE "$pattern" "$file"; then
    echo "  ok: $desc"
  else
    echo "  FAIL: $desc — $file does not match '$pattern'"
    fail=1
  fi
}

check Firmware/README.md "^\*\*Version\*\*: $FW_RE$" "Firmware/README.md version line"
check Documentation/user-manual/USER_MANUAL.md "^\*\*Firmware version:\*\* $FW_RE$" "USER_MANUAL.md version line"
check Documentation/user-manual/images/config-ui.svg "$FW_RE" "config-ui.svg version label"
check Documentation/user-manual/images/health-ui.svg "FW: $FW_RE" "health-ui.svg FW label"

TOP_CHANGELOG=$(grep -m 1 -E '^## ' Firmware/CHANGELOG.md)
if printf '%s' "$TOP_CHANGELOG" | grep -qE "^## $FW_RE - [0-9]{4}-[0-9]{2}-[0-9]{2}$"; then
  echo "  ok: CHANGELOG.md top entry"
else
  echo "  FAIL: CHANGELOG.md top entry is '$TOP_CHANGELOG', expected '## $FW - YYYY-MM-DD'"
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "Version consistency check FAILED for $FW"
  exit 1
fi
echo "All version references consistent at $FW"
