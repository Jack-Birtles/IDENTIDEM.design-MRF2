# MRF2 Browser Firmware Updater (GitHub Pages)

This repo includes a static firmware updater site in `docs/` that uses Web Serial + ESP Web Tools to flash MRF2 firmware in-browser.

## Public URL

`https://update.mrf2.com/`

## UI snapshot

![Web updater UI](../user-manual/images/web-updater-ui.svg)

## How it works

1. The GitHub Actions workflow `.github/workflows/firmware-updater-pages.yml` builds firmware from `Firmware/` using PlatformIO.
2. The workflow copies all required ESP32-S3 images into each version directory:
   - `bootloader.bin` @ `0x0000` (Adafruit TinyUF2 bootloader)
   - `partitions.bin` @ `0x8000`
   - `boot_app0.bin` @ `0xE000`
   - `tinyuf2.bin` @ `0x2D0000`
   - `firmware.bin` @ `0x10000`
3. The workflow also builds tagged historical releases and publishes them under `firmware/versions/<version>/`.
4. The workflow generates:
   - `firmware/latest/manifest.json`
   - `firmware/versions/<version>/manifest.json` for each published version
   - `firmware/versions.json` catalog (used by the UI dropdown)
5. The static app in `docs/` loads `firmware/versions.json`, defaults the selector to the latest entry, and points ESP Web Tools to the selected manifest.
6. The release notes panel shows notes for the selected version and the immediately previous version, with a link to the full changelog.

## Settings preservation

Camera settings are stored in NVS (ESP32 non-volatile storage) at flash address `0x9000`. None of the five flashed images overlap that address, so all saved settings survive a web updater firmware upgrade. New settings introduced by a firmware update will simply load their defaults on first boot until changed.

The manifest sets `new_install_prompt_erase: false`, which prevents the flasher from erasing the device before writing. A full chip erase is never performed by the web updater.

If you need to reset all settings to factory defaults, use the factory reset option in the camera's settings menu, or reflash via PlatformIO as described in `Documentation/flash-firmware/README.md`.

## Browser requirements

- Desktop Chrome or Edge (Web Serial support)
- HTTPS context (GitHub Pages provides this)
- USB data cable

## Operational notes

- Workflow trigger is `push` to `main` (and `workflow_dispatch`).
- If the site shows `Not published yet`, run the workflow or merge to `main`.
- If `boot_app0.bin` path changes in PlatformIO packages, update the workflow copy path.
