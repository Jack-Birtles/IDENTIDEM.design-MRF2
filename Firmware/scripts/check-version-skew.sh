#!/usr/bin/env bash
#
# Verify that FWVERSION is consistent across the five release-ritual files
# documented in CLAUDE.md. Run manually before tagging a release, or wire as a
# pre-commit hook (see comment at the bottom of this file).
#
# Exit status:
#   0 = all five files agree (or CHANGELOG carries an Unreleased section that
#       has yet to be renamed for the next bump)
#   1 = at least one file is out of sync; review the FAIL lines printed above

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

source_of_truth_file="Firmware/include/mrfconstants.h"
fwversion=$(grep -E '^#define FWVERSION ' "$source_of_truth_file" \
            | sed -E 's/.*"([^"]+)".*/\1/')

if [[ -z "$fwversion" ]]; then
  echo "FAIL: could not parse FWVERSION from $source_of_truth_file" >&2
  exit 1
fi
echo "FWVERSION (source of truth): $fwversion"

# Each expected_location is "<file>::<grep_pattern>". The grep_pattern is a
# POSIX extended regex run against the file; if it doesn't match, the file is
# considered out of sync.
expected_locations=(
  "Firmware/README.md::\\*\\*Version\\*\\*: ${fwversion}"
  "Documentation/user-manual/USER_MANUAL.md::\\*\\*Firmware version:\\*\\* ${fwversion}"
  "Documentation/user-manual/images/config-ui.svg::MRF ${fwversion}"
)

failed=0
for spec in "${expected_locations[@]}"; do
  file="${spec%%::*}"
  pattern="${spec#*::}"
  if [[ ! -f "$file" ]]; then
    echo "FAIL: ${file} does not exist" >&2
    failed=1
    continue
  fi
  if ! grep -Eq "$pattern" "$file"; then
    echo "FAIL: ${file} is missing the expected marker for ${fwversion}" >&2
    echo "       (looked for /${pattern}/)" >&2
    failed=1
  fi
done

# CHANGELOG is allowed to be in one of two valid states:
#   1) a "## <FWVERSION> - YYYY-MM-DD" section already exists (post-release)
#   2) an "## Unreleased" section exists (pre-release working state)
# Anything else means the CHANGELOG has drifted out of sync with FWVERSION.
changelog="Firmware/CHANGELOG.md"
if grep -Eq "^## ${fwversion} " "$changelog"; then
  : # versioned section present, all good
elif grep -Eq "^## Unreleased" "$changelog"; then
  echo "NOTE: ${changelog} has an 'Unreleased' section; rename it to" \
       "'## ${fwversion} - YYYY-MM-DD' before tagging." >&2
else
  echo "FAIL: ${changelog} has neither a '## ${fwversion}' section nor an" \
       "'## Unreleased' section" >&2
  failed=1
fi

if [[ "$failed" -eq 0 ]]; then
  echo "OK: ${fwversion} is consistent across all release-ritual files."
fi
exit "$failed"

# ---------------------------------------------------------------------------
# Wiring this as a pre-commit hook:
#   ln -s ../../Firmware/scripts/check-version-skew.sh .git/hooks/pre-commit
# (Or chain it into an existing pre-commit hook script.)
# Skip the hook for a specific commit with `git commit --no-verify` when you
# are intentionally landing a partial version bump.
# ---------------------------------------------------------------------------
