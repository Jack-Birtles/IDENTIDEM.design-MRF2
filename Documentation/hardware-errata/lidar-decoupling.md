# Errata: LiDAR power decoupling (breakout board)

**Applies to:** MRF-Pro v7.5 breakout board (and earlier), DTS6012M LiDAR on connector J7.

**Status:** Hand-solder fix available now. A board respin is planned (see the end of this document).

## Symptom

- The LiDAR reads close objects fine but cannot range distant ones.
- Range is much worse outdoors, in open shade, or in direct sun.
- Hard to get a stable lock; readings jump or drop out at distance.
- A bench unit on a clean power supply behaves better than the same sensor in the camera.

## Root cause

The DTS6012M is a pulsed-laser time-of-flight sensor. Each laser pulse draws a short, sharp burst of current. On the current breakout the LiDAR's 3.3 V comes from the Feather's onboard regulator through a long, high-impedance path — across the main board, an FPC ribbon, a second FPC connector, thin (0.25 mm) copper, and the J7 JST connector — **with no decoupling capacitor anywhere near the sensor.** There is also no local capacitor on the Feather's regulator output for this load.

With no local charge reservoir, every laser pulse pulls its peak current through that whole impedance and the 3.3 V rail at the sensor sags during the exact moment it is trying to measure the return. Close, bright targets still work (little laser energy needed); distant or low-reflectivity targets and bright-ambient scenes — which need full laser power and maximum signal-to-noise — fail. The DTS6012M user manual itself notes the module has a dedicated `3V3_LASER` pin and advises ensuring sufficient supply current "or use an external 3.3 V regulator."

> Note: there is no DTS6012M current specification in this repository, so the capacitor values below are derived from a generic pulsed-laser ToF load model. Verify with the scope test before declaring the problem solved. This follows the project's "derived fix + field verification" convention.

## Fix (hand-solder, no board change)

Solder to the back of the breakout at/under J7, bridging the LiDAR 3.3 V and GND pads. **Keep all leads as short as possible — lead length adds inductance that defeats the purpose.**

Priority order:

1. **P1 — wideband decoupling at J7 (do this first, always).**
   A bulk capacitor in parallel with two smaller high-frequency ceramics, from LiDAR 3.3 V to GND:
   - **47–100 µF** bulk — a 47 µF/10 V tantalum, or a 100 µF 1206 X5R/X7R 6.3 V MLCC (size up because MLCCs lose 30–60 % of their value under DC bias).
   - **‖ 1 µF** 0603 ceramic.
   - **‖ 100 nF** 0402 ceramic.
   A single 10 µF cap is **not** enough: it derates to a few µF under bias and covers neither the bulk droop nor the high-frequency pulse edges.

2. **P2 — second HF cap on the laser pin.** If J7 breaks out `3V3_LASER` and `3V3` on separate pins, add another 1 µF + 100 nF on the laser pin specifically. If both module supply pins are tied together at the connector, P1 already covers it.

3. **P3 — stiffen the source.** Add 10 µF + 100 nF across the Feather 3V3↔GND on the main board, near where the FPC ribbon picks up 3.3 V.

4. **P4 — bypass the thin path (advanced).** Run short AWG 26–28 flying jumpers straight from the Feather 3V3 and GND pins to the J7 3.3 V/GND pads, paralleling the ribbon and JST. This is the single biggest improvement for the series-impedance half of the problem. Pair it with P1.

## Verifying the fix

Use an oscilloscope, not a multimeter — a DMM averages out the nanosecond transient and will look fine even when the rail is collapsing.

1. Probe the LiDAR VCC pin at J7, AC-coupled, ~20 mV/div.
2. Aim at a distant / low-reflectivity target in bright light and trigger a reading.
3. **Bad:** pulse-synchronous sag or ringing greater than ~100–150 mV, or the rail dipping toward 3.0 V.
4. **Good (after fix):** ripple shrinks to a few tens of mV and readings hold at distance.
5. Functional A/B: range a subject at 8–15 m before and after the fix; watch the **Setup > LiDAR > Diagnostics** screen — Intensity/Quality at distance should rise and the `err`/`Recov` counts should drop.

To collect comparable before/after data, use the [LiDAR field test protocol](lidar-field-test.md).

## Planned board respin

The next breakout revision should fix this properly rather than relying on hand-soldered caps. Tracked separately; the headline change is a **dedicated low-noise 3.3 V regulator for the LiDAR on the breakout** (fed from VBAT/VBUS, sized for the laser peak current, placed next to J7), plus designed-in decoupling on both supply pins, a wider/shorter supply path with multiple FPC power/ground conductors and ground stitching vias, and unpopulated UART series-termination footprints.
