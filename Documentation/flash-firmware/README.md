# Flashing the MRF2 Firmware

This guide walks through building and flashing the firmware on **macOS, Linux, and Windows**. It assumes you have the repo checked out and want to flash the `Firmware/` project.

Two paths are covered:

- **VS Code + PlatformIO IDE** (recommended for most builders): sections 1 through 7.
- **Command line (PlatformIO Core)**: the CLI section near the end.

If you would rather not install anything, use the browser-based updater instead:
`https://update.mrf2.com/`

The MRF2 board is an Adafruit Feather ESP32-S3. It uses the ESP32-S3's **native USB**, so there is **no CP210x/CH340 USB-to-serial chip and no vendor driver to install** on any operating system. The notes below cover the few things that do differ per OS: serial port permissions on Linux, COM-port behavior on Windows, and the bootloader recovery sequence.

## Prerequisites (all platforms)
- VS Code installed
- PlatformIO IDE extension installed
- A **USB-C data cable** (charge-only cables will not enumerate a serial port)
- MRF2 powered on

### Per-platform setup notes

#### macOS

- No driver needed. The board appears as `/dev/tty.usbmodem*`.

#### Windows (10/11)

- No driver needed. Windows installs the built-in USB CDC (serial) driver automatically the first time you plug the board in; it appears as a `COMx` port in Device Manager under "Ports (COM & LPT)".
- If the port never appears, see the Windows note in Troubleshooting.

#### Linux

- No driver needed, but your user must have permission to access the serial device, and `ModemManager` can interfere with uploads. Both are one-time fixes; see [Linux serial port access](#linux-serial-port-access) below before your first upload.

## 1) Install VS Code extensions
1. Open VS Code.
2. Open the Extensions view (`Cmd/Ctrl+Shift+X`).
3. Install **PlatformIO IDE**.
4. (Optional) Install **Espressif IDF**. PlatformIO does not require it, but it can be useful for ESP tooling and serial utilities.

## 2) Open the project in PlatformIO
1. Click the PlatformIO icon in the Activity Bar to open **PIO Home**.
2. Choose **Open Project**.
3. Select the `Firmware/` folder (the folder that contains `platformio.ini`).
4. Wait for PlatformIO to finish installing toolchains and dependencies.

## 3) Build the firmware
1. In the PlatformIO toolbar (bottom bar), click **Build** (checkmark icon).
2. Confirm the build completes without errors.

## 4) Connect and select the serial port
1. Switch on the MRF2 and connect it via USB-C.
2. In the bottom bar, click the **Serial Port** selector and choose **Auto** or the correct port.
   - macOS examples: `/dev/tty.usbmodem*`
   - Linux examples: `/dev/ttyACM0` (native USB) or `/dev/ttyUSB0`
   - Windows examples: `COM3`, `COM4`

## 5) Upload (flash)
1. Click **Upload** (right-arrow icon) in the PlatformIO toolbar.
2. Wait for the upload to finish.

If upload fails:
- Put the ESP32-S3 into bootloader mode: **hold BOOT**, tap **RESET**, then release BOOT and retry upload.
- Try a different USB-C cable or USB port.
- On Linux, confirm serial permissions (see below). On Windows, confirm the COM port appears in Device Manager.

## 6) Verify on device
1. Disconnect USB.
2. Power-cycle the MRF2.
3. The external display should show the firmware boot screen and version.

## 7) Serial monitor (optional)
1. Click **Monitor** (plug icon) in the PlatformIO toolbar.
2. Use **115200 baud** (matches `SERIAL_BAUD_RATE` in `Firmware/include/mrfconstants.h`).

---

## Linux serial port access

On most distributions the serial device is owned by the `dialout` group (Debian/Ubuntu) or `uucp` group (Arch/Fedora). If you are not a member, uploads and the monitor fail with "permission denied".

1. Find your distro's serial group and add yourself to it:
   ```bash
   # Debian / Ubuntu / Raspberry Pi OS
   sudo usermod -aG dialout "$USER"

   # Arch / Fedora / openSUSE
   sudo usermod -aG uucp "$USER"
   ```
2. **Log out and back in** (or reboot) for the group change to take effect.
3. Verify the device shows up after plugging in the board:
   ```bash
   ls -l /dev/ttyACM* /dev/ttyUSB*
   ```

**ModemManager grabbing the port:** on desktop Linux, `ModemManager` may probe the board the moment it enumerates and cause upload timeouts. If uploads intermittently fail right after plugging in, either wait a few seconds before uploading, or stop ModemManager:
```bash
sudo systemctl stop ModemManager     # for this session
sudo systemctl disable ModemManager  # permanently, if you do not use cellular modems
```

PlatformIO can also install the official udev rules for you if the port is not detected. See `https://docs.platformio.org/en/latest/core/installation/udev-rules.html`.

---

## Command line (PlatformIO Core)

Use this if you prefer the terminal or are flashing on a headless machine. All commands assume your working directory is `Firmware/` (or pass `-d Firmware` from the repo root).

### Install PlatformIO Core

PlatformIO Core needs Python 3.6+.

**macOS / Linux**
```bash
python3 -m pip install --user platformio
# then make sure ~/.local/bin is on your PATH, e.g. for bash/zsh:
export PATH="$HOME/.local/bin:$PATH"
```

**Windows (PowerShell)**
```powershell
py -m pip install --user platformio
# pio.exe lands in %APPDATA%\Python\PythonXY\Scripts; add it to PATH if `pio` is not found
```

Confirm the install:
```bash
pio --version
```

(Alternatively, install the **PlatformIO IDE** extension in VS Code and use its bundled `pio` from the PlatformIO Core CLI terminal, with no separate install needed.)

### Build, flash, monitor

```bash
# Build the firmware for the ESP32-S3
pio run -e adafruit_feather_esp32s3

# Flash a connected board (auto-detects the port)
pio run -e adafruit_feather_esp32s3 --target upload

# Open the serial monitor (115200 baud)
pio run -e adafruit_feather_esp32s3 --target monitor
```

To target a specific port, append `--upload-port`:
```bash
# Linux
pio run -e adafruit_feather_esp32s3 --target upload --upload-port /dev/ttyACM0
# Windows
pio run -e adafruit_feather_esp32s3 --target upload --upload-port COM4
# macOS
pio run -e adafruit_feather_esp32s3 --target upload --upload-port /dev/tty.usbmodem1101
```

List detected ports with `pio device list`.

### Host-side tests (no hardware required)

These run on your computer and do not touch the board, so they work the same on all three platforms:
```bash
pio test -e native_core_tests
```

---

## Troubleshooting
- **No serial port listed (all platforms)**: try a different cable (must be a data cable) or a different USB port.
- **No COM port on Windows**: open Device Manager and watch "Ports (COM & LPT)" while plugging the board in. If it appears under "Other devices" with a warning, unplug, wait, and replug; if it still fails, try another USB port or cable. No vendor driver is required for this board.
- **Permission denied on Linux**: add your user to the serial group and re-login (see [Linux serial port access](#linux-serial-port-access)).
- **Upload starts but fails to connect**: hold **BOOT**, tap **RESET**, release **BOOT**, then retry the upload.
- **Build errors on first run**: let PlatformIO finish downloading toolchains, then retry.
- **Upload times out intermittently on Linux**: stop `ModemManager` (see above).

For the firmware architecture and configuration reference, see `Firmware/README.md`.
