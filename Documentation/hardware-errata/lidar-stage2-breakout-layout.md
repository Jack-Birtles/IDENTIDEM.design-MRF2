# Breakout layout spec — LiDAR LDO (Stage 2)

> **CURRENT — Stage 2 · breakout PCB layout.** The board half of the dedicated-LDO fix in [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md); capture steps in [lidar-stage2-ldo-kicad-guide.md](lidar-stage2-ldo-kicad-guide.md). Supersedes the Stage-1 decoupling-only layout. Start at the [errata index](README.md).

This is the PCB-layout half of the LiDAR power fix on the breakout
(`PCBs/v2.0/Breakout/KiCAD/MRF-Pro-v8-breakout.kicad_pcb`). The schematic is
captured and ERC-clean; this document is the placement/routing intent for the
KiCad PCB editor (layout is GUI work — `kicad-cli` only runs DRC and exports).

Background and the "why" are in [lidar-stage1-decoupling.md](lidar-stage1-decoupling.md)
(root cause) and [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md) (the design).

## What the schematic contains (as-built)

The breakout now carries a dedicated TLV75533 LDO fed from `VBAT`, with two
output rails split by a 0 Ω bead, and an EN pulldown:

| Ref | Value | Footprint | Net(s) | Role |
|-----|-------|-----------|--------|------|
| U1 | TLV75533 (3.3 V, 500 mA) | `Package_TO_SOT_SMD:SOT-23-5` | IN=`VBAT`, EN=`3.3V`, OUT=`3V3_LIDAR`, GND | The regulator |
| Cin1 | 1 µF | `Capacitor_SMD:C_0402_1005Metric` | `VBAT`/GND | Input decoupling at U1 IN |
| Cin2 | 10 µF | `Capacitor_SMD:C_0805_2012Metric` | `VBAT`/GND | Input bulk at U1 IN |
| C2 | 1 µF | `Capacitor_SMD:C_0402_1005Metric` | `3V3_LIDAR`/GND | LDO stability cap at U1 OUT |
| C3 | 100 nF | `Capacitor_SMD:C_0402_1005Metric` | `3V3_LIDAR`/GND | HF bypass at U1 OUT |
| C1 | 47 µF | bulk | `3V3_LASER`/GND | Laser-pulse reservoir at J7 pin 1 |
| FB1 | 0 Ω (ferrite option) | `0805` | `3V3_LIDAR`↔`3V3_LASER` | Splits the two output rails |
| R1 | 100 kΩ | `Resistor_SMD:R_0402_1005Metric` | `3.3V`/GND | EN pulldown — deterministic shutdown |

Rails: `VBAT` (battery in, always present) → U1 → `3V3_LIDAR` (logic, J7 pin 2)
→ FB1 → `3V3_LASER` (laser, J7 pin 1). The switched `3.3V` on FPC pin 1 drives
U1 EN, the Stemma QT (J6), and R1.

## Connector facts that drive placement

From the as-built netlist:

- **J7 (DTS6012M, 6-pin):** pin 1 = `3V3_LASER`, pin 2 = `3V3_LIDAR` (logic) —
  two separate post-LDO rails split by FB1. Pins 3/4 = UART (to FPC pins 2/3).
  **Pin 5 = INT mode-select tied to GND** (low = UART; do not disturb). Pin 6 = GND.
- **J5 (FPC, 8-pin):** pin 1 = switched `3.3V` (U1 EN + J6 + R1), pin 6 = `VBAT`
  (U1 IN), pins 4/5 = GND, pins 2/3 = UART → J7, pins 7/8 = I²C → J6.
- **J6 (Stemma QT):** on the switched `3.3V` / I²C; carries the I²C bus to the
  rotary encoder and the rest of the peripherals.

## Layout steps

1. **Update PCB from Schematic** (F8) to pull in U1, Cin1/Cin2, FB1, **R1**, and
   the renamed nets (`VBAT`, `3V3_LIDAR`, `3V3_LASER`).
2. **Make the clean rail at the sensor** — proximity is everything (lead/track
   inductance defeats decoupling):
   - **U1 right next to J7**, so `3V3_LIDAR` is generated at the load.
   - **C2 (1 µF) + C3 (100 nF) hard against U1 OUT** (the LDO's required Cout, on
     the near side of FB1) and close to J7 pin 2.
   - **C1 (47 µF bulk) hard against J7 pin 1 (`3V3_LASER`)** — the pulsed laser
     load, on the far side of FB1.
   - **Cin1 + Cin2 tight to U1 IN** on `VBAT`.
   - **FB1 between** the U1-OUT node and the C1/laser node.
   - **R1 near U1 EN** (anywhere on the `3.3V` net is electrically fine).
   - Mount on the back of the board directly under J7 if the front is congested.
3. **Widen the supply copper.** Keep the `VBAT` run into U1 and the
   `3V3_LIDAR`/`3V3_LASER` runs out of it short and wide (≥0.5–0.8 mm or filled
   zones). The `VBAT` ribbon path is a regulator *input*, so series drop there is
   harmless as long as it stays above dropout — but keep the on-breakout legs tidy.
4. **Stitch the grounds.** Several **GND vias** near U1, the caps, and J7 to tie
   front/back copper and give the laser pulse a low-inductance return.
5. **Test points.** Add probe points (or 0 Ω-pad pads) on **`3V3_LIDAR`** and
   **`VBAT`** so the scope verification can be done without tacking onto J7.
6. **Refill zones**, then **DRC** until clean.
7. **Regenerate fab outputs** (Gerbers, drill, BOM, position files). The new
   parts (U1, Cin1, Cin2, FB1, R1) carry LCSC fields for the JLCPCB BOM/CPL.

## Sign-off

Scope the LiDAR VCC at J7, AC-coupled, during a far / bright-light reading:
pulse-synchronous ripple should drop to a few tens of mV and far readings should
hold. Then confirm the [design's verification list](lidar-stage2-ldo-design.md#verification):
`3V3_LIDAR` collapses to 0 V at power-off (deterministic via R1), off-state
battery draw ~µA, cold-start boots first try, and a before/after range
improvement using the [field test protocol](lidar-field-test.md).
