# LiDAR dedicated regulator — design (Stage 2, gated)

**Status: design locked, not implemented.** Build this only if field testing shows the Stage 1 decoupling alone does not restore LiDAR range. Tracked on beads issue `IDENTIDEM_design-MRF2-3z5`. Background and root cause: [lidar-decoupling.md](lidar-decoupling.md).

## Goal

Give the DTS6012M a clean, stiff 3.3 V **at the sensor** that the pulsed laser cannot drag down, isolated from the ESP32-S3 / WiFi noise on the shared 3.3 V rail.

## Locked decisions

1. **LDO**, not a buck-boost.
2. **TI TLV75533** — chosen for its high PSRR / low noise. The input is the shared, noisy VBAT rail and the sensor is noise-sensitive, so rejection matters more than the marginal cost over an AP2112K.
3. **Keep the 8-pin FPC** — bring VBAT only; no power-enable GPIO line.

## Architecture — regulate at the point of load

```
Feather BAT pin ──(main board, new VBAT net)──> FPC pin 6 ──ribbon──> Breakout
                                                                        │ VBAT
                                                                  [Cin] │
                                                                   ┌────┴────┐
                                                                   │   U1    │ TLV75533, 3.3V
                                                                   │  EN─IN  │
                                                                   └────┬────┘
                                                                  3V3_LIDAR (new net)
                                                        C1 47µF ‖ C2 1µF ‖ C3 100nF
                                                                        │
                                                                  J7 pins 1,2 (LiDAR supply)
Feather 3V3 ──> FPC pin 1 ──> Stemma QT (J6) only
```

The regulator sits on the **breakout**, next to J7, so the clean rail is created at the sensor. The rough VBAT travels the lossy FPC/ribbon — voltage drop on a regulator *input* is harmless as long as it stays above dropout.

## Accepted limitation (low battery)

The input is the LiPo VBAT (3.0–4.2 V). Making 3.3 V, the LDO drops out as the battery approaches ~3.3 V and the output then follows VBAT down — the same regime where the Feather's own 3.3 V LDO already struggles. This is accepted:

- For most of the discharge curve the rail is clean and fully isolated.
- Even in dropout, the LDO still isolates the sensor from WiFi/switching noise and the output bulk caps still absorb the laser pulse, so it is never worse than today.

If low-battery LiDAR range later proves to matter in the field, the fallback is a buck-boost (e.g. TPS63xxx) holding 3.3 V at any VBAT, with output filtering and careful layout because it is a switcher next to a noise-sensitive receiver. Not now.

## Components (breakout)

| Ref | Part | Footprint | Notes |
|-----|------|-----------|-------|
| U1 | TLV75533 (fixed 3.3 V, ~500 mA) | `Package_TO_SOT_SMD:SOT-23-5` | Confirm pinout against the datasheet/symbol when implementing (DBV 5-pin typ: 1=IN, 2=GND, 3=EN, 4=NC, 5=OUT). Tie **EN→IN** (always on; no spare FPC conductor for a GPIO). |
| Cin | 1 µF (0402) + 10 µF (0805, ≥10 V) | — | Input decoupling on VBAT. |
| Cout | reuse C1 47 µF + C2 1 µF + C3 100 nF | existing | The Stage-1 caps become the LDO output network, moved onto `3V3_LIDAR`. TLV75533 is stable with this much ceramic Cout. |

Thermal is a non-issue: worst case ≈ 0.9 V × ~80 mA ≈ 70 mW in a SOT-23-5.

## Net and connector changes

**New nets (breakout):**
- `VBAT` — FPC J5 pin 6 → U1 IN (+ Cin).
- `3V3_LIDAR` — U1 OUT → J7 pins 1, 2 (+ C1/C2/C3). Detach J7 pins 1, 2 from the system `3.3V`.

**FPC (J5, 8-pin) reallocation** — no spare conductors, so repurpose one:

| Pin | Now | After | Feeds |
|-----|-----|-------|-------|
| 1 | 3.3V | 3.3V | Stemma QT (J6) only |
| 2 | UART | UART | J7 pin 3 |
| 3 | UART | UART | J7 pin 4 |
| 4 | GND | GND | — |
| 5 | GND | GND | — |
| 6 | 3.3V | **VBAT** | U1 IN |
| 7 | I²C | I²C | J6 |
| 8 | I²C | I²C | J6 |

**Main board:** wire the Feather **BAT** header pin to a new `VBAT` net → FPC pin 6. The Feather is modeled as generic header connectors with the BAT pin currently unconnected, so this is additive (no new connector). Match the exact header position to the Adafruit Feather ESP32-S3 pinout when implementing. FPC pin 1 stays on the Feather 3.3 V for the Stemma QT.

## Execution split

Schematic edits authored and ERC-validated headlessly with `kicad-cli`; PCB layout (placement, routing, pours, DRC, fab outputs) done in the KiCad GUI.

## Verification

- ERC both boards; netlist shows U1 IN on `VBAT`; U1 OUT + C1/C2/C3 + J7 pins 1,2 on `3V3_LIDAR`; J6 still on `3.3V`.
- Scope J7 VCC under laser load: ripple to tens of mV; confirm it holds clean until VBAT approaches ~3.5 V (dropout onset).
- Max-range bright-ambient field test versus the decoupling-only build.
