#!/usr/bin/env bash
# Regenerates docs/vendor/esp-web-tools/install-button.js from npm.
# Usage: scripts/vendor-esp-web-tools.sh [version]   (default 10.2.1)
set -euo pipefail

VERSION="${1:-10.2.1}"
ESBUILD_VERSION="0.25.6"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/docs/vendor/esp-web-tools"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

cd "$WORK_DIR"
npm init -y >/dev/null
npm install --no-audit --no-fund "esp-web-tools@$VERSION" "esbuild@$ESBUILD_VERSION" >/dev/null

npx esbuild node_modules/esp-web-tools/dist/web/install-button.js \
  --bundle --format=esm --minify --legal-comments=none \
  --banner:js="/* esp-web-tools $VERSION (https://github.com/esphome/esp-web-tools), Apache-2.0. Bundled with esbuild for self-hosting; see docs/vendor/esp-web-tools/README.md to regenerate. */" \
  --outfile=install-button.js

mkdir -p "$OUT_DIR"
cp install-button.js "$OUT_DIR/install-button.js"
cp node_modules/esp-web-tools/LICENSE "$OUT_DIR/LICENSE"

echo "Vendored esp-web-tools $VERSION into $OUT_DIR"
echo "Verify the updater end to end before merging."
