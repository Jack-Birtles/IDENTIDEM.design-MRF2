# LiDAR field test protocol

Thank you for helping diagnose the LiDAR range issue. The goal is to get consistent, comparable numbers from every tester so we can tell apart three possible causes (the focus-distance plausibility gate, the sensor's frame rate / integration time, and a board power-supply problem). Please follow the steps exactly and record every value, including "no reading" — a blank tells us nothing, but "no reading" is real data.

Everything you need is on the new **Setup > LiDAR > Diagnostics** screen. Aim the camera, hold steady, and read the live values off that screen.

## 1. Fill in once

| Field | Your value |
|---|---|
| Tester name / unit ID | |
| Firmware version (Health screen) | |
| Sensor FW hex (Health screen) | |
| Hardware: stock, or which cap mod fitted? (none / P1 caps / P3 source cap / P4 flying wire) | |
| Date | |

## 2. Setup rules (so runs are comparable)

- Battery above 50% (a low cell can cause its own brownout).
- After waking the camera, wait ~10 seconds before the first reading.
- For each reading, aim the reticle at the centre of the target, hold steady ~2 seconds, and record the value once it stops changing.
- For each reading, note the lens focus distance you set (this matters — see the focus rule below).

## 3. Main test matrix

Repeat the whole table in **three lighting conditions**: (A) indoors under artificial light, (B) outdoors in open shade, (C) outdoors in direct sun.

Targets:
- **W** = a white / light matte wall
- **P** = a person or a mid-grey object
- **D** = a dark / low-reflectivity object

Ranges: **0.3 m, 1 m, 2.5 m, 5 m, 8 m, 12 m.**

**Focus rule:** for the 5 m, 8 m and 12 m rows, take each reading **twice** — once with the lens focused *at* the target, and once with the lens focused *near (minimum)*. Label them "focus far" / "focus near". (This is the only way to separate the focus-distance gate from a genuine sensor limit.)

For every cell record, from the Diagnostics screen:

| Target | Range | Lighting | Focus | Raw (mm) | Disp | Intensity | SunBase | SNR | Quality | Held | err | Notes |
|--------|-------|----------|-------|----------|------|-----------|---------|-----|---------|------|-----|-------|
| W | 0.3m | A | — | | | | | | | | | |
| W | 1m | A | — | | | | | | | | | |
| W | 2.5m | A | — | | | | | | | | | |
| W | 5m | A | far | | | | | | | | | |
| W | 5m | A | near | | | | | | | | | |
| W | 8m | A | far | | | | | | | | | |
| W | 8m | A | near | | | | | | | | | |
| W | 12m | A | far | | | | | | | | | |
| W | 12m | A | near | | | | | | | | | |

…then repeat the same nine rows for target **P** and target **D**, and repeat the whole block for lighting **B** and **C**. Copy the table as many times as you need. If there is no reading at all, write "none" in the Disp column and still record Intensity / SunBase / Quality.

## 4. Frame-rate sweep (if we send you alternate firmware)

If you have been given firmware builds at different frame rates, repeat just the **W** and **P** columns at each rate. Confirm the rate on the Diagnostics screen (`fps act`) before each run.

| Rate (fps act) | Best range on W | Best range on P | Notes |
|---|---|---|---|
| 50 | | | |
| 30 | | | |
| 15 | | | |

## 5. Hardware A/B (only if you can solder)

If you fit the cap mod from the [decoupling errata](lidar-decoupling.md): run the full Section 3 matrix once **before** fitting, then again **after**, on the same day with the same targets. Note which mod you fitted in the Section 1 header.

## 6. Free-text observations

For each lighting condition, describe what the readout *did*, in your own words — for example: instant lock, climbs slowly, jumps around, sticks on one number, shows `...`, shows `Held`. This often tells us as much as the numbers.

---

Send the completed tables back as text or a photo of your notes. Thank you.
