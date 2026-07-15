# Vendored esp-web-tools

`install-button.js` is a self-contained esbuild bundle of
[esp-web-tools](https://github.com/esphome/esp-web-tools) 10.2.1
(`dist/web/install-button.js` entry point), licensed Apache-2.0 (see LICENSE).

It is vendored so update.mrf2.com serves no third-party script. The previous
unpkg `?module` load meant a CDN compromise or MITM could swap the code that
decides which bytes get flashed to builders' cameras.

To upgrade, run `scripts/vendor-esp-web-tools.sh <version>` from the repo root
and verify the updater end to end (connect a camera, flash, reboot) before
merging.
