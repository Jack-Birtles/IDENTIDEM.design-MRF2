# Updating the boards in KiCad — LiDAR LDO (Stage 2)

> **CURRENT — Stage 2 · implementation guide.** KiCad capture steps for the design in [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md). Start at the [errata index](README.md).

Step-by-step capture of the dedicated LiDAR regulator design. The design itself is locked in [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md); this is the click-by-click for the KiCad GUI. The target circuit is drawn in [images/lidar-ldo-schematic.svg](images/lidar-ldo-schematic.svg) — keep it open while you work.

Two projects change:

- **Breakout** — `PCBs/Breakout/KiCAD/MRF-Pro-v7.5-breakout.kicad_sch` (the real work: U1, Cin, two new nets).
- **Main board** — `PCBs/Main PCB/KiCAD/MRF-Pro-v7.5.kicad_sch` (one wire: Feather BAT → FPC pin 6).

`kicad-cli` cannot author a schematic — it only runs ERC and exports. So every step below is in the GUI. Use `kicad-cli sch erc` afterwards to check.

> Before you start: commit or stash the working tree so the change is its own diff, and note that the breakout project keeps a `.history/` and a `_restore_backup_*` folder — don't edit those copies.

---

## A. Breakout schematic

Open `MRF-Pro-v7.5-breakout.kicad_sch` in the KiCad Schematic Editor (Eeschema).

### A1. Place U1 (the LDO)

1. Press `A` (Add Symbol). Search the part:
   - If you have a TLV75533 symbol in a library, use it.
   - Otherwise use `Regulator_Linear:AP2112K-3.3` — its SOT-23-5 pinout is **identical** to the TLV75533 DBV (`1=IN, 2=GND, 3=EN, 4=NC, 5=OUT`), so it drops in pin-for-pin. **Rename the value to `TLV75533`.**
2. Place it near J7, set reference to **U1**.
3. **Pinout (confirmed against TI datasheet SBVS320D, Table 4-1):** `1=IN, 2=GND, 3=EN, 4=NC, 5=OUT`. <https://www.ti.com/lit/ds/symlink/tlv755p.pdf>
4. Assign the footprint **`Package_TO_SOT_SMD:SOT-23-5`** (Footprint field or in the editor's symbol properties).

### A2. Place Cin (input decoupling)

1. Press `A`, add `Device:C` twice. References **Cin1** (or next free C ref) and the second cap.
   - Values: **1 µF** (0402) and **10 µF** (0805, ≥10 V).
   - Footprints: `Capacitor_SMD:C_0402_1005Metric` and `Capacitor_SMD:C_0805_2012Metric`.
2. Place both between the VBAT input and GND, next to U1's IN pin.

> The existing C1/C2/C3 stay as-is in the netlist — you are **re-targeting** them onto the output net, not adding new ones. See A5.

### A3. New net: VBAT (FPC pin 6 → U1 IN)

1. The 8-pin FPC is **J5** (`Connector:Conn_01x08_Pin`). Today pin 6 is on the `3.3V` net.
2. **Detach pin 6 from 3.3V:** delete the wire/label tying J5 pin 6 to the `3.3V` net.
3. Draw a wire from J5 pin 6 and attach a **net label `VBAT`** (press `L`).
4. Label U1's **IN** pin `VBAT` as well, and tie both Cin caps' top plates to `VBAT`, bottoms to `GND`.

Result: `J5 pin 6 — Cin — U1 IN`, all on `VBAT`.

### A4. EN slaved to switched 3.3 V (FPC pin 1)

1. J5 pin 1 stays on the existing `3.3V` net (switched Feather rail). Do **not** change pin 1.
2. Draw a wire from U1's **EN** pin and label it `3.3V` (same net name as pin 1 — KiCad joins same-named labels, no wire needed across the sheet).
3. This is what makes the camera power switch also cut the LDO. Do **not** tie EN to IN/VBAT.

### A5. New net: 3V3_LIDAR (U1 OUT → C1/C2/C3 → J7 pins 1,2)

J7 is the 6-pin DTS6012M connector. Per the [sensor datasheet](datasheets/DTS6012M-Docdt22-Rev1.2.pdf): **pin 1 = 3V3_LASER, pin 2 = 3V3** (both supply, currently on `3.3V`), pins 3,4 = UART, **pin 5 = INT (interface-mode select)**, pin 6 = GND.

There are two output nets joined by **FB1** (a 0 Ω jumper by default; populate a ferrite later only if scoping shows laser noise on the logic supply):

- **`3V3_LIDAR`** — the LDO output node: U1 OUT, the stability caps C2 (1 µF) + C3 (100 nF), J7 pin 2 (logic), and FB1's input. Keep C2/C3 here so the regulator always sees its Cout.
- **`3V3_LASER`** — past FB1: C1 (47 µF bulk) and J7 pin 1 (laser).

1. Delete J7 pin 1 and pin 2 connections from `3.3V`.
2. Label U1's **OUT** pin, C2/C3 hot plates, and **J7 pin 2** all `3V3_LIDAR` (cap bottoms to `GND`).
3. Place **FB1** (`Device:FerriteBead` or a 0 Ω `Device:R`): input on `3V3_LIDAR`, output on `3V3_LASER`. Default value **0 Ω / DNP-ferrite**.
4. Label FB1's output side, **C1** (47 µF) hot plate, and **J7 pin 1** all `3V3_LASER` (C1 bottom to `GND`).
5. In layout: C2/C3 hard against U1 OUT; **C1 hard against J7 pin 1**; FB1 between them. Add probe/test points on `3V3_LIDAR` and `VBAT`.
6. **Do not touch J7 pin 5 (INT).** It is the UART/I²C mode select, read at power-on; it must stay tied low for UART. Pins 3,4 (UART) and pin 6 (GND) are unchanged.

Result: `U1 OUT — C2‖C3 — [3V3_LIDAR] — FB1 — [3V3_LASER] — C1`, with J7 pin 2 on `3V3_LIDAR` and pin 1 on `3V3_LASER`. At FB1 = 0 Ω the two are one clean rail, fully isolated from the system `3.3V`; UART, INT, and GND untouched.

### A6. Sanity check before ERC

Confirm on the canvas (matches the SVG):

- U1 IN → `VBAT`; J5 pin 6 → `VBAT`; Cin on `VBAT`/`GND`.
- U1 EN → `3.3V`; J5 pin 1 still → `3.3V`; J6 (Stemma QT) still → `3.3V`.
- U1 OUT → `3V3_LIDAR`; C2/C3 → `3V3_LIDAR`; J7 pin 2 → `3V3_LIDAR`; FB1 between `3V3_LIDAR` and `3V3_LASER`; C1 + J7 pin 1 → `3V3_LASER`.
- J7 pins 3,4 still UART (from J5 pins 2,3); J7 pin 5 (INT) still tied low; J7 pin 6 + J5 pins 4,5 still `GND`; J5 pins 7,8 still I²C → J6.

Run **Inspect → Electrical Rules Checker**. Expect a few "power pin not driven" warnings unless a `PWR_FLAG` sits on `VBAT`, `3V3_LIDAR`, and `3V3_LASER` — add a `power:PWR_FLAG` to each new net to clear those.

---

## B. Main board schematic

Open `MRF-Pro-v7.5.kicad_sch`.

1. The Feather ESP32-S3 is modeled as **generic header connectors**, and its **BAT** pin is currently unconnected. This change is additive — no new connector.
2. Find the header pin that corresponds to **BAT** on the Adafruit Feather ESP32-S3 pinout. Match the exact position from Adafruit's pinout diagram — don't assume, the generic symbol won't label it BAT for you.
3. Draw a wire from that BAT pin and label it **`VBAT`**.
4. Find the main-board side of the FPC that goes to the breakout, and label its **pin 6** conductor **`VBAT`** (it was `3.3V` — detach it from `3.3V` here too, exactly mirroring the breakout side).
5. FPC pin 1 stays on the Feather `3.3V`. No change to the power switch / Feather EN wiring.

Run ERC on the main board too.

---

## C. Verify (CLI)

From each project's KiCad dir (the macOS binary lives inside `KiCad.app/Contents/MacOS/`):

```bash
kicad-cli sch erc MRF-Pro-v7.5-breakout.kicad_sch
kicad-cli sch erc "MRF-Pro-v7.5.kicad_sch"
```

Netlist must show:

- U1 IN on `VBAT`
- U1 OUT + C1 + C2 + C3 + J7 pins 1,2 on `3V3_LIDAR`
- U1 EN on `3.3V` (FPC pin 1)
- J6 still on `3.3V`

---

## D. PCB layout (after schematic ERC passes)

Open each `.kicad_pcb`, **Tools → Update PCB from Schematic** to pull in U1 + Cin and the renamed nets, then:

- Place **U1 next to J7** so `3V3_LIDAR` is made at the sensor; keep C1/C2/C3 within a few mm of J7.
- Keep Cin tight to U1's IN pin.
- Pour/route `3V3_LIDAR` and `GND` generously near the sensor; stitch GND vias.
- Run **DRC**, then regenerate fab outputs.

Full layout intent is in [lidar-stage1-breakout-layout.md](lidar-stage1-breakout-layout.md).

---

## Done-when

- ERC clean on both boards (only intentional `PWR_FLAG`-resolved power warnings).
- Netlist matches section C.
- DRC clean after layout.
- Field verification per [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md#verification): scope J7 VCC under laser load, confirm cold-start boot, off-state battery current ~µA.
