# Breakout respin: LiDAR decoupling layout spec

> **Stage 1 · decoupling layout spec.** Covers the C1–C3 decoupling placement only. The Stage-2 LDO adds U1/Cin/FB1 — its layout notes live in [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md) until a dedicated Stage-2 layout doc exists. Start at the [errata index](README.md).

This is the PCB-layout half of the LiDAR power fix. The schematic
(`PCBs/v2.0/Breakout/KiCAD/MRF-Pro-v8-breakout.kicad_sch`) already has the
decoupling capacitors C1–C3 added and ERC-clean. This document is the
step-by-step for finishing the board in the KiCad PCB editor, since layout
(placement, routing, pours, DRC, fab outputs) is GUI work.

Background and the "why" are in [lidar-stage1-decoupling.md](lidar-stage1-decoupling.md).

## What changed in the schematic

Three capacitors across the LiDAR 3.3 V rail to GND, all on the existing
`3.3V` / `GND` nets:

| Ref | Value | Footprint | Role |
|-----|-------|-----------|------|
| C1 | 47 µF | `Capacitor_SMD:C_1206_3216Metric` | Bulk reservoir for the laser pulse |
| C2 | 1 µF | `Capacitor_SMD:C_0603_1608Metric` | Mid-band |
| C3 | 100 nF | `Capacitor_SMD:C_0402_1005Metric` | High-frequency bypass |

The schematic is intentionally ahead of the PCB until you run **Update PCB
from Schematic** — this is the normal mid-respin state, not an error.

## Connector facts that drive placement

From the netlist:

- **J7 (LiDAR) pin 1 and pin 2 are the two DTS6012M supply pins**
  (`3V3_LASER` and `3V3`), both on the `3.3V` net. Pins 5 and 6 are GND.
  Pins 3/4 are the UART.
- The FPC already carries **3.3 V on two conductors (J5 pins 1 + 6)** and
  **GND on two (J5 pins 4 + 5)**, so the ribbon is already paralleled; the
  remaining series-impedance win is copper width and the short run to J7.

## Layout steps

1. **Update PCB from Schematic** (F8 in the PCB editor) to pull in C1–C3
   with the footprints above.
2. **Placement — proximity is everything** (lead/track inductance is what
   defeats decoupling):
   - Put **C3 (100 nF)** as close as physically possible to **J7 pin 1
     (laser supply)**, with the shortest possible GND return to pin 5/6.
   - Put **C2 (1 µF)** immediately next to C3.
   - Put **C1 (47 µF bulk)** right beside them, still within a few mm of J7.
   - Mount on the back of the board directly under J7 if the front is
     congested.
   - Optional but recommended: a second 100 nF tight to **J7 pin 2**
     (digital supply). If you add it, drop another `Device:C` 100 nF on the
     `3.3V` net in the schematic first so it round-trips.
3. **Widen the supply copper.** Take the `3.3V` and `GND` tracks feeding J7
   from 0.25 mm up to **≥0.5–0.8 mm**, or rely on filled zones with enough
   copper. Keep the J7 power run short and direct from the FPC landing.
4. **Stitch the grounds.** Add several **GND vias** near J7 and the caps to
   tie the front and back copper/zones together, giving the laser pulse a
   low-inductance return.
5. **Refill zones**, then **DRC** until clean.
6. **Regenerate fab outputs** (Gerbers, drill, BOM, position files). Add the
   three caps to the BOM/CPL.

## Sign-off

The electrical proof is unchanged from the errata: scope the LiDAR VCC at J7,
AC-coupled, during a far / bright-light reading. Pulse-synchronous ripple
should drop to a few tens of mV and far readings should hold. Confirm a
before/after range improvement with the
[field test protocol](lidar-field-test.md).

If the decoupling alone does not restore range, the next step is the
dedicated LiDAR regulator (a two-board change) — see the design notes on
beads issue `IDENTIDEM_design-MRF2-3z5`.
