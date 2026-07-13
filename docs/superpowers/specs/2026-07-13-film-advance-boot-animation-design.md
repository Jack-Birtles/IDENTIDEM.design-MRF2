# Film-advance boot animation (external display)

**Date:** 2026-07-13
**Target version:** 10.6.1 (patch bump + tag + GitHub release)
**Scope:** Firmware only. One new pure-logic module, one host unit test, a thin draw loop in `main.cpp`, a handful of constants, plus the standard release-file updates.

## Goal

Give the 128x32 external SSD1306 a silly-but-tasteful startup animation: a strip of
perforated film feeds in from the right and carries the `MRF X.Y.Z` version into
the centre, like loading a fresh roll. It fully replaces today's static splash and
must not make boot feel slower.

## Current behaviour (what we're replacing)

`showBootScreenOnExternalDisplay()` in `Firmware/src/main.cpp` prints `MRF X.Y.Z`
centred, holds it for `DISPLAY_BOOT_SCREEN_MS` (1000 ms) via a blocking `delay`,
then clears the display and hands the panel to `u8g2_ext`. It is called once in
`setup()` between `initializeMainDisplay()` and the LiDAR init step, guarded by
`hardware.externalDisplay`.

## Design

### The animation (128 wide x 32 tall, monochrome)

- **Sprocket perforations:** small white rectangles run along the top edge
  (y 0..3) and bottom edge (y 28..31) at a fixed horizontal spacing. Each frame
  their x positions shift left by a fixed step and wrap, so the film reads as
  advancing through the gate. Perforations are present for the whole animation.
- **Frame image (the "photo"):** the centre band (y ~8..24) carries the text
  `MRF X.Y.Z`. It starts fully off the right edge and eases left to centred.
- **Landing:** once the text is centred, perforations tick for a couple more
  frames, then a short hold, then the display clears exactly as today and control
  passes to `u8g2_ext`. Final visible state = version legibly centred (same info
  a builder relies on today).

### Timing

Whole sequence targets ~1000-1200 ms so perceived boot time is unchanged. The
existing `DISPLAY_BOOT_SCREEN_MS` is repurposed as the end-hold after landing (or
reduced accordingly); the moving portion is `frameCount * frameDelayMs`. Concrete
values live in constants (below) and are tuned so the total stays in budget.

### Logic / hardware split (matches repo convention)

New pure module `Firmware/src/boot_animation_logic.cpp` + `include/boot_animation_logic.h`,
host-testable, no Arduino/Adafruit types. It computes, given a frame index and the
display width, the per-frame geometry the draw loop needs. Proposed surface:

```cpp
// Eased horizontal offset (px) of the version text for a given frame.
// Monotonically non-increasing; starts >= displayWidth (off-screen right),
// ends at the centred x for the final animated frame.
int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth);

// Wrapped horizontal offset (0..spacing-1) of the sprocket holes for a frame.
int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx);
```

Easing: a simple integer ease-out (e.g. offset proportional to
`(totalFrames - frame)^2 / totalFrames^2 * startOffset`) so the film decelerates
into place. Exact curve chosen during implementation; the test pins the
invariants, not the precise pixels.

The draw loop in `main.cpp` stays thin: for each frame, clear, draw top+bottom
sprocket rects using `bootSprocketOffsetForFrame`, draw the version text at
`bootTextXForFrame`, `display_ext.display()`, `delay(frameDelayMs)`. Version
string is built once with the existing largest-fits-width sizing logic so long
versions (e.g. `10.6.10`) still fit.

### Constants (`Firmware/include/mrfconstants.h`)

- `DISPLAY_BOOT_ANIM_FRAMES` — number of animated frames (e.g. 16).
- `DISPLAY_BOOT_ANIM_FRAME_MS` — per-frame delay (e.g. 45 ms).
- `DISPLAY_BOOT_SPROCKET_SPACING` — px between perforation centres (e.g. 12).
- `DISPLAY_BOOT_SPROCKET_STEP` — px the perforations shift per frame (e.g. 3).
- `DISPLAY_BOOT_SPROCKET_W` / `_H` — perforation rect size.
- `DISPLAY_BOOT_SCREEN_MS` — retained, now the post-landing hold.

### What does NOT change

- No new boot step; the main-display progress bar is untouched.
- No prefs / NVS schema change (no `PREFS_SCHEMA_VERSION` bump).
- `hardware.externalDisplay` guard and the `u8g2_ext` handoff are preserved.
- Graceful no-op when the external display is absent (early return, as today).

## Testing

Add cases to `Firmware/test/test_core_logic_regression/test_main.cpp` (include the
new logic `.cpp` and header the same way the other logic modules are pulled in).
Assert the invariants:

- `bootTextXForFrame(0, ...)` >= displayWidth (text starts fully off-screen right).
- Sequence is monotonically non-increasing across frames (never jitters right).
- Final frame lands at the centred x: `(displayWidth - textWidth) / 2`.
- `bootSprocketOffsetForFrame` stays within `[0, spacing)` and advances by `step`
  each frame with correct wrap.

Run: `pio test -e native_core_tests -f test_core_logic_regression`.
Firmware build sanity: `pio run -e adafruit_feather_esp32s3`.
On-hardware check: flash and confirm the film feeds in, version lands centred,
boot feels no slower. Note "verify on device" in the CHANGELOG since timing is
eyeballed, not measured.

## Documentation & release (10.6.1)

Per the repo release workflow, update all five version files:

1. `Firmware/include/mrfconstants.h` -> `#define FWVERSION "10.6.1"`
2. `Firmware/README.md` -> `**Version**: 10.6.1`
3. `Firmware/CHANGELOG.md` -> new `## 10.6.1 - 2026-07-13` section describing the
   boot animation, with a "verify boot timing on device" note.
4. `Documentation/user-manual/USER_MANUAL.md` -> `**Firmware version:** 10.6.1`
5. `Documentation/user-manual/images/config-ui.svg` -> on-screen version label.

Then commit, push the branch, and after merge: annotated tag `v10.6.1`, GitHub
release with the changelog section as the body (no binary assets).

## Out of scope

- Any animation on the main 128x128 display.
- User-configurable / toggleable animation (YAGNI; it's a boot flourish).
- Sound, per-format variations, or logo bitmaps.
