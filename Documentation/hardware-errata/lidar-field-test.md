# LiDAR field test protocol

> **Reference · all stages.** Diagnostic protocol for gathering comparable before/after range data for any of the fixes. Start at the [errata index](README.md).

Thank you for helping diagnose the LiDAR range issue. The goal is to get consistent, comparable numbers from every tester so we can tell apart three possible causes (the focus-distance plausibility gate, the sensor's frame rate / integration time, and a board power-supply problem). Please follow the steps exactly and record every value, including "no reading" — a blank tells us nothing, but "no reading" is real data.

Everything you need is on the new **Setup > LiDAR > Diagnostics** screen. Aim the camera, hold steady, and read the live values off that screen.

## 1. Fill in once

| Field | Your value |
|---|---|
| Tester name / unit ID | |
| Firmware version (Health screen) | |
| Sensor FW hex (Health screen) | |
| Hardware: stock, or which cap mod fitted? (none / P1 caps / P3 source cap / P4 flying wire) | |
| LiDAR offset setting (Setup > LiDAR > Offset; default 400 mm) | |
| Unit role: reference (known-good) or test | |
| Date | |

**About the offset:** the Raw value on the Diagnostics screen already includes your offset setting, so two cameras with different offsets are not comparable. If you have changed the offset, either note it above or set it back to 400 mm for the test.

## 2. Setup rules (so runs are comparable)

- Battery above 50% (a low cell can cause its own brownout).
- After waking the camera, wait ~10 seconds before the first reading.
- For each reading, aim the reticle at the centre of the target, hold steady ~2 seconds, and record the value once it stops changing.
- For each reading, note the lens focus distance you set (this matters — see the focus rule below).
- Check the **Age** field before writing anything down. It should read a few tens of milliseconds; if it is climbing (seconds), the sensor has stopped sending frames and the other values are stale — write "stale" in Notes instead of the numbers.

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

## 4. Near-range calibration sweep (tape measure needed)

The firmware applies a correction curve below 1.5 m that was derived from a single measured point, and we need real pairs to replace it. This sweep is the data that fixes it — it needs only one target and one room.

Indoors, against the white wall (**W**), measure the true distance from the **front of the lens** to the wall with a tape measure. Lens focus set roughly at the target each time. Record:

| True (tape) | Raw (mm) | Disp | Intensity | Quality | Notes |
|---|---|---|---|---|---|
| 0.3 m | | | | | |
| 0.5 m | | | | | |
| 0.75 m | | | | | |
| 1.0 m | | | | | |
| 1.25 m | | | | | |
| 1.5 m | | | | | |

Accuracy of the tape measurement matters here — please measure to the centimetre, don't pace it out.

## 5. Beam-miss check (subject close, background far)

This verifies the focus-distance plausibility gate. Stand a person (or a chair) at **1 m**, with a wall or other large surface **3–5 m** behind them. Lens focused at the subject (~1 m).

1. Aim the reticle at the subject's centre. Record Disp / Quality / Held.
2. Deliberately aim just past their shoulder so the beam hits the background. Record what Disp does over ~3 seconds (jumps to the wall instantly / shows `Held:` then releases / sticks on the subject), plus Quality and Held.
3. Aim back at the subject. Record how quickly the reading returns.

| Step | Disp | Quality | Held | What it did (own words) |
|---|---|---|---|---|
| 1 — on subject | | | | |
| 2 — past shoulder | | | | |
| 3 — back on subject | | | | |

## 6. Frame-rate sweep (if we send you alternate firmware)

If you have been given firmware builds at different frame rates, repeat just the **W** and **P** columns at each rate. Confirm the rate on the Diagnostics screen (`fps act`) before each run.

| Rate (fps act) | Best range on W | Best range on P | Notes |
|---|---|---|---|
| 50 | | | |
| 30 | | | |
| 15 | | | |

## 7. Hardware A/B (only if you can solder)

If you fit the cap mod from the [decoupling errata](lidar-stage1-decoupling.md): run the full Section 3 matrix once **before** fitting, then again **after**, on the same day with the same targets. Note which mod you fitted in the Section 1 header.

## 8. Reference unit comparison (if you have a known-good camera)

If you have a unit that already ranges well (e.g. one that reads to ~6 m indoors), run it through **Section 3 with the exact same targets, distances and lighting** as a suspect unit, and mark it `reference` in the Section 1 header. Its Diagnostics numbers become the target the others should hit.

What the comparison tells us, reading the two unitsʼ telemetry side by side at the **same target / range / lighting**:

- Reference shows clearly higher **Intensity / Quality** at the same distance → the difference is genuinely in the optics/sensor (a real unit-to-unit or batch difference).
- Both show similar raw **Intensity** but the suspect still reads short → the loss is downstream of the sensor (supply droop / wiring), not the sensor itself — the case the decoupling and regulator fixes target.
- Compare the **Sensor FW hex** (Health screen) on both — a different value points at a different sensor firmware/calibration batch.

Keep conditions identical; an indoor 6 m reading is far easier than anything outdoors, so only compare like with like.

## 9. Free-text observations

For each lighting condition, describe what the readout *did*, in your own words — for example: instant lock, climbs slowly, jumps around, sticks on one number, shows `...`, shows `Held`. This often tells us as much as the numbers.

---

Send the completed tables back as text or a photo of your notes. Thank you.
