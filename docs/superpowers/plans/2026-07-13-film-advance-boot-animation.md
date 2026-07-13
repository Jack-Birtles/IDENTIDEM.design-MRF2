# Film-Advance Boot Animation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the static external-display boot splash with a silly film-advance animation that feeds the `MRF X.Y.Z` version in from the right, and ship it as firmware 10.6.1.

**Architecture:** A new pure-logic module computes per-frame geometry (eased text x-offset, wrapped sprocket offset) and is host-unit-tested. The draw loop in `main.cpp`'s existing `showBootScreenOnExternalDisplay()` calls that logic and renders rectangles + text to the 128x32 SSD1306, keeping the hardware code thin. Timing constants live in `mrfconstants.h`.

**Tech Stack:** C++ (Arduino/ESP32-S3), Adafruit_SSD1306 for the external OLED, PlatformIO, Unity host tests (`native_core_tests`).

## Global Constraints

- External display is 128 wide x 32 tall, monochrome (`SCREEN_WIDTH` = 128, `SCREEN_HEIGHT_EXT` = 32).
- Pure-logic modules must be host-compilable: no Arduino/Adafruit types in `boot_animation_logic.*`.
- No `Co-Authored-By: Claude` or any AI trailer in commits. Human-style commit subjects, no AI tells, no emoji.
- Do NOT add `setFrameRate`-style side effects; this change touches display only.
- No prefs / NVS schema change; do not bump `PREFS_SCHEMA_VERSION`.
- Preserve the `hardware.externalDisplay` guard and the `u8g2_ext.begin(display_ext)` handoff at the end of the boot screen.
- Total animation (moving frames + end hold) must stay ~1000-1200 ms so boot feels no slower.
- Release version is exactly `10.6.1`; date `2026-07-13`.

---

### Task 1: Pure boot-animation geometry logic + host tests

**Files:**
- Create: `Firmware/include/boot_animation_logic.h`
- Create: `Firmware/src/boot_animation_logic.cpp`
- Test: `Firmware/test/test_core_logic_regression/test_main.cpp` (add include + cases)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth);`
  - `int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx);`

- [ ] **Step 1: Write the failing tests**

Add near the other logic includes at the top of `Firmware/test/test_core_logic_regression/test_main.cpp`:

```cpp
#include "boot_animation_logic.h"
```

And with the other `#include "../../src/*.cpp"` lines:

```cpp
#include "../../src/boot_animation_logic.cpp"
```

Add these test functions (place them alongside the existing `test_*` functions):

```cpp
void test_boot_text_starts_offscreen_right()
{
  // Frame 0: the version text begins fully off the right edge of the display.
  TEST_ASSERT_TRUE(bootTextXForFrame(0, 16, 128, 120) >= 128);
}

void test_boot_text_lands_centered_on_final_frame()
{
  const int totalFrames = 16;
  const int displayWidth = 128;
  const int textWidth = 120;
  int expectedCentered = (displayWidth - textWidth) / 2; // 4
  TEST_ASSERT_EQUAL_INT(expectedCentered,
                        bootTextXForFrame(totalFrames - 1, totalFrames, displayWidth, textWidth));
}

void test_boot_text_is_monotonically_non_increasing()
{
  const int totalFrames = 16;
  int prev = bootTextXForFrame(0, totalFrames, 128, 120);
  for (int f = 1; f < totalFrames; f++)
  {
    int x = bootTextXForFrame(f, totalFrames, 128, 120);
    TEST_ASSERT_TRUE(x <= prev); // film never jitters back to the right
    prev = x;
  }
}

void test_boot_text_single_frame_guard()
{
  // Degenerate totalFrames must not divide by zero; returns the centered x.
  TEST_ASSERT_EQUAL_INT((128 - 120) / 2, bootTextXForFrame(0, 1, 128, 120));
}

void test_boot_sprocket_offset_within_spacing_and_advances()
{
  const int step = 3;
  const int spacing = 12;
  for (int f = 0; f < 40; f++)
  {
    int off = bootSprocketOffsetForFrame(f, step, spacing);
    TEST_ASSERT_TRUE(off >= 0 && off < spacing);
    TEST_ASSERT_EQUAL_INT((f * step) % spacing, off);
  }
}

void test_boot_sprocket_offset_spacing_guard()
{
  // Non-positive spacing must not modulo-by-zero.
  TEST_ASSERT_EQUAL_INT(0, bootSprocketOffsetForFrame(5, 3, 0));
}
```

Register them in the Unity `RUN_TEST(...)` block (inside `main()` / the runner where the other `RUN_TEST` calls live):

```cpp
RUN_TEST(test_boot_text_starts_offscreen_right);
RUN_TEST(test_boot_text_lands_centered_on_final_frame);
RUN_TEST(test_boot_text_is_monotonically_non_increasing);
RUN_TEST(test_boot_text_single_frame_guard);
RUN_TEST(test_boot_sprocket_offset_within_spacing_and_advances);
RUN_TEST(test_boot_sprocket_offset_spacing_guard);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native_core_tests -f test_core_logic_regression`
Expected: FAIL to compile / link with an error about `boot_animation_logic.h` not found or `bootTextXForFrame` undefined.

- [ ] **Step 3: Write the header**

Create `Firmware/include/boot_animation_logic.h`:

```cpp
#ifndef BOOT_ANIMATION_LOGIC_H
#define BOOT_ANIMATION_LOGIC_H

// Pure geometry for the external-display film-advance boot animation.
// No Arduino/Adafruit types so it can be unit-tested on the host.

// Eased left-edge x (px) of the version text for a given animation frame.
// The film feeds in from the right: frame 0 starts fully off-screen
// (x == displayWidth) and the sequence eases out to the centered x on the
// final frame (frame == totalFrames - 1). Monotonically non-increasing.
// frame is clamped to [0, totalFrames - 1]; totalFrames <= 1 returns centered.
int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth);

// Wrapped x-offset (0..spacingPx-1) of the sprocket perforations for a frame,
// advancing by stepPx each frame so the film reads as moving left.
// spacingPx <= 0 returns 0.
int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx);

#endif // BOOT_ANIMATION_LOGIC_H
```

- [ ] **Step 4: Write the implementation**

Create `Firmware/src/boot_animation_logic.cpp`:

```cpp
#include "boot_animation_logic.h"

int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth)
{
  const int centered = (displayWidth - textWidth) / 2;
  if (totalFrames <= 1)
  {
    return centered;
  }
  if (frame < 0)
  {
    frame = 0;
  }
  if (frame > totalFrames - 1)
  {
    frame = totalFrames - 1;
  }

  const long last = totalFrames - 1;
  const int start = displayWidth;            // fully off the right edge
  const long span = (long)(centered - start); // negative: text moves left

  // Ease-out: eased = 1 - (1 - p)^2, with p = frame / last.
  // easedNumerator / (last*last) == eased, kept in integer math.
  const long remaining = last - frame;
  const long easedNumerator = last * last - remaining * remaining;

  return start + (int)(span * easedNumerator / (last * last));
}

int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx)
{
  if (spacingPx <= 0)
  {
    return 0;
  }
  long off = ((long)frame * stepPx) % spacingPx;
  if (off < 0)
  {
    off += spacingPx;
  }
  return (int)off;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native_core_tests -f test_core_logic_regression`
Expected: PASS — all existing tests plus the six new ones.

- [ ] **Step 6: Commit**

```bash
git add Firmware/include/boot_animation_logic.h Firmware/src/boot_animation_logic.cpp Firmware/test/test_core_logic_regression/test_main.cpp
git commit -m "Add boot animation geometry logic with host tests"
```

---

### Task 2: Constants + film-advance draw loop in main.cpp

**Files:**
- Modify: `Firmware/include/mrfconstants.h` (add animation constants; retune `DISPLAY_BOOT_SCREEN_MS`)
- Modify: `Firmware/src/main.cpp` (include header; replace body of `showBootScreenOnExternalDisplay()`)

**Interfaces:**
- Consumes: `bootTextXForFrame`, `bootSprocketOffsetForFrame` from Task 1.
- Produces: animated boot screen; no new public symbols.

- [ ] **Step 1: Add constants**

In `Firmware/include/mrfconstants.h`, next to the existing `DISPLAY_BOOT_SCREEN_MS` / `DISPLAY_BOOT_TEXT_SIZE` lines (around line 68-77), change the hold and add the animation constants:

```cpp
const unsigned long DISPLAY_BOOT_SCREEN_MS = 400;      // Post-landing hold after the film settles.
const int DISPLAY_BOOT_ANIM_FRAMES = 16;               // Number of animated film-advance frames.
const unsigned long DISPLAY_BOOT_ANIM_FRAME_MS = 40;   // Delay per animation frame (16*40 = 640 ms).
const int DISPLAY_BOOT_SPROCKET_SPACING = 12;          // Px between sprocket-hole centres.
const int DISPLAY_BOOT_SPROCKET_STEP = 3;              // Px the perforations shift per frame.
const int DISPLAY_BOOT_SPROCKET_W = 6;                 // Sprocket-hole width in px.
const int DISPLAY_BOOT_SPROCKET_H = 4;                 // Sprocket-hole height (top and bottom bands).
```

(Total ~640 ms moving + 400 ms hold = ~1040 ms, within the 1000-1200 ms budget.)

- [ ] **Step 2: Include the logic header in main.cpp**

In `Firmware/src/main.cpp`, add to the include block (after `#include "mrfconstants.h"`):

```cpp
#include "boot_animation_logic.h"
```

- [ ] **Step 3: Replace the body of showBootScreenOnExternalDisplay()**

Replace the current function body (the version-sizing loop through the final clear) so it keeps the largest-fits-width sizing, then animates. The full function becomes:

```cpp
void showBootScreenOnExternalDisplay()
{
  if (!hardware.externalDisplay)
  {
    return;
  }

  display_ext.clearDisplay();
  display_ext.setTextColor(SSD1306_WHITE);

  char bootText[24];
  snprintf(bootText, sizeof(bootText), "MRF %s", FWVERSION);

  // Pick the largest text size (up to the preferred one) that fits the display
  // width, so a longer version string (e.g. 10.6.10) can't wrap on the narrow
  // external screen.
  int16_t bootTextX1 = 0;
  int16_t bootTextY1 = 0;
  uint16_t bootTextWidth = 0;
  uint16_t bootTextHeight = 0;
  for (int bootTextSize = DISPLAY_BOOT_TEXT_SIZE; bootTextSize >= 1; bootTextSize--)
  {
    display_ext.setTextSize(static_cast<uint8_t>(bootTextSize));
    display_ext.getTextBounds(bootText, 0, 0, &bootTextX1, &bootTextY1, &bootTextWidth, &bootTextHeight);
    if (static_cast<int16_t>(bootTextWidth) <= display_ext.width())
    {
      break;
    }
  }

  const int16_t bootCursorY =
      ((display_ext.height() - static_cast<int16_t>(bootTextHeight)) / 2) - bootTextY1;

  // Film-advance animation: perforations tick left along the top and bottom
  // edges while the version rides in from the right and eases to centre.
  for (int frame = 0; frame < DISPLAY_BOOT_ANIM_FRAMES; frame++)
  {
    display_ext.clearDisplay();

    const int sprocketOffset =
        bootSprocketOffsetForFrame(frame, DISPLAY_BOOT_SPROCKET_STEP, DISPLAY_BOOT_SPROCKET_SPACING);
    for (int x = sprocketOffset - DISPLAY_BOOT_SPROCKET_SPACING; x < display_ext.width();
         x += DISPLAY_BOOT_SPROCKET_SPACING)
    {
      display_ext.fillRect(x, 0, DISPLAY_BOOT_SPROCKET_W, DISPLAY_BOOT_SPROCKET_H, SSD1306_WHITE);
      display_ext.fillRect(x, display_ext.height() - DISPLAY_BOOT_SPROCKET_H,
                           DISPLAY_BOOT_SPROCKET_W, DISPLAY_BOOT_SPROCKET_H, SSD1306_WHITE);
    }

    const int textX = bootTextXForFrame(frame, DISPLAY_BOOT_ANIM_FRAMES,
                                        display_ext.width(), static_cast<int>(bootTextWidth));
    display_ext.setCursor(static_cast<int16_t>(textX) - bootTextX1, bootCursorY);
    display_ext.print(bootText);

    display_ext.display();
    delay(DISPLAY_BOOT_ANIM_FRAME_MS);
  }

  // Hold the settled frame, then clear and hand the panel to u8g2_ext as before.
  delay(DISPLAY_BOOT_SCREEN_MS);

  u8g2_ext.begin(display_ext);
  display_ext.clearDisplay();
  display_ext.display();
}
```

- [ ] **Step 4: Build the firmware**

Run: `pio run -e adafruit_feather_esp32s3`
Expected: SUCCESS, no new `-Wall -Wextra` warnings from the changed code.

- [ ] **Step 5: Re-run host tests (regression guard)**

Run: `pio test -e native_core_tests`
Expected: PASS (unchanged; confirms the constants edit didn't break anything).

- [ ] **Step 6: On-device verification**

Flash a board (`pio run -e adafruit_feather_esp32s3 --target upload`) and watch the external display at power-on: perforations tick left, `MRF 10.6.0`/current version rides in from the right and settles centred, boot feels no slower than before. (Version string updates to 10.6.1 in Task 3.)

- [ ] **Step 7: Commit**

```bash
git add Firmware/include/mrfconstants.h Firmware/src/main.cpp
git commit -m "Animate external boot splash as a film-advance sequence"
```

---

### Task 3: Version bump to 10.6.1 + docs + release

**Files:**
- Modify: `Firmware/include/mrfconstants.h:9`
- Modify: `Firmware/README.md:3`
- Modify: `Firmware/CHANGELOG.md` (new top section)
- Modify: `Documentation/user-manual/USER_MANUAL.md:3`
- Modify: `Documentation/user-manual/images/config-ui.svg:15`

**Interfaces:**
- Consumes: the feature from Tasks 1-2.
- Produces: a tagged, released 10.6.1.

- [ ] **Step 1: Bump the firmware version constant**

In `Firmware/include/mrfconstants.h` line 9:

```cpp
#define FWVERSION "10.6.1"                   // Version shown in UI and release metadata.
```

- [ ] **Step 2: Bump README**

In `Firmware/README.md` line 3:

```markdown
**Version**: 10.6.1
```

- [ ] **Step 3: Bump user manual**

In `Documentation/user-manual/USER_MANUAL.md` line 3:

```markdown
**Firmware version:** 10.6.1
```

- [ ] **Step 4: Update the on-screen version label in the SVG**

In `Documentation/user-manual/images/config-ui.svg` line 15, change the version at the end of the text node:

```xml
  <text x="3" y="121" font-family="monospace" font-size="6" fill="#ffffff"> IDENTIDEM.design MRF 10.6.1</text>
```

- [ ] **Step 5: Add the CHANGELOG section**

In `Firmware/CHANGELOG.md`, insert this new section immediately above `## 10.6.0 - 2026-07-13`:

```markdown
## 10.6.1 - 2026-07-13

### External display boot animation

- Replaced the static `MRF X.Y.Z` boot splash on the external OLED with a film-advance animation: sprocket perforations tick along the top and bottom edges while the version rides in from the right and settles centred, like loading a fresh roll. The per-frame geometry lives in a new host-tested `boot_animation_logic` module; the draw loop stays thin. Boot timing is unchanged (~1 s). Verify boot timing and legibility on device.
```

- [ ] **Step 6: Verify the version appears consistently**

Run: `grep -rn "10.6.1" Firmware/include/mrfconstants.h Firmware/README.md Firmware/CHANGELOG.md Documentation/user-manual/USER_MANUAL.md Documentation/user-manual/images/config-ui.svg`
Expected: one match in each of the five files.

- [ ] **Step 7: Commit**

```bash
git add Firmware/include/mrfconstants.h Firmware/README.md Firmware/CHANGELOG.md Documentation/user-manual/USER_MANUAL.md Documentation/user-manual/images/config-ui.svg
git commit -m "Release firmware v10.6.1"
```

- [ ] **Step 8: Tag and release (after merge to main)**

These run once the branch is merged. Confirm with the maintainer before pushing.

```bash
git tag -a v10.6.1 <merge-commit> -m "Firmware v10.6.1"
git push origin v10.6.1
gh release create v10.6.1 --title "Firmware v10.6.1" --notes-file <(sed -n '/## 10.6.1/,/## 10.6.0/p' Firmware/CHANGELOG.md | sed '$d')
```

Tags are annotated, not signed. No binary assets — the updater workflow builds from `main`.

---

## Self-Review

**Spec coverage:**
- Animation on external display, film-advance, replaces static splash → Task 2 draw loop. Yes
- Perforations present whole animation, version eases in from right to centre → Task 1 logic + Task 2 loop. Yes
- Timing ~1000-1200 ms → Task 2 constants (640 + 400 ms). Yes
- Logic/hardware split, pure host-tested module → Task 1. Yes
- Test invariants (starts off-screen, monotonic, lands centred, sprocket wrap) → Task 1 tests. Yes
- No prefs/schema change, preserve guard + u8g2_ext handoff → Task 2 function preserves both. Yes
- Release 10.6.1: five version files + changelog + tag/release → Task 3. Yes
- Out of scope (main display, toggle, sound) → not present. Yes

**Placeholder scan:** No TBD/TODO; all steps have concrete code and commands. The `<merge-commit>` in Step 8 is a genuine post-merge value the maintainer supplies, not a plan gap.

**Type consistency:** `bootTextXForFrame` / `bootSprocketOffsetForFrame` signatures identical across header (Task 1), tests (Task 1), and call sites (Task 2). Constant names (`DISPLAY_BOOT_ANIM_FRAMES`, `DISPLAY_BOOT_ANIM_FRAME_MS`, `DISPLAY_BOOT_SPROCKET_SPACING`, `DISPLAY_BOOT_SPROCKET_STEP`, `DISPLAY_BOOT_SPROCKET_W`, `DISPLAY_BOOT_SPROCKET_H`, `DISPLAY_BOOT_SCREEN_MS`) match between Task 2 Step 1 and Step 3.
