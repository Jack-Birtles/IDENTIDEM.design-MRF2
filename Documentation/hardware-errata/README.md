# LiDAR power errata — index

Hardware errata for the DTS6012M LiDAR power supply on the MRF-Pro breakout (connector J7). The work runs in **stages**; filenames carry the stage so the current document is obvious at a glance.

## Which doc do I follow?

**→ [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md) — the CURRENT plan.** A dedicated TLV75533 LDO for the LiDAR, fed from VBAT. Then its build guide, [lidar-stage2-ldo-kicad-guide.md](lidar-stage2-ldo-kicad-guide.md).

Everything else is background, supporting detail, or a superseded earlier stage — see the table.

## Documents

| Document | Stage | Status | Purpose |
|----------|-------|--------|---------|
| [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md) | 2 | **CURRENT** | The design to build: dedicated LDO, net/connector changes, components, verification. |
| [lidar-stage2-ldo-kicad-guide.md](lidar-stage2-ldo-kicad-guide.md) | 2 | **CURRENT** | KiCad GUI click-by-click to capture the Stage-2 schematic. |
| [lidar-stage1-decoupling.md](lidar-stage1-decoupling.md) | 1 | Superseded for the respin; still valid as **background + hand-solder stopgap** | Root-cause analysis and the solder-in decoupling fix that shipped first. |
| [lidar-stage1-breakout-layout.md](lidar-stage1-breakout-layout.md) | 1 | Partial | PCB layout for the Stage-1 decoupling caps only (see note below). |
| [lidar-field-test.md](lidar-field-test.md) | all | Reference | Tester protocol for comparable before/after range data. Unprefixed because it spans every stage; also linked from the firmware and the user manual. |

Supporting assets: [datasheets/](datasheets/) (TI TLV755P committed; vendor-confidential DTS6012M kept local), [images/](images/) (schematic + diagrams).

## Naming convention

- `lidar-stageN-*.md` — belongs to stage N of the fix. Higher stage number = later, more complete approach; a later stage supersedes earlier ones for the board respin.
- `lidar-field-test.md` — no stage prefix: it is used across all stages.
- The repository git history is the version record; filenames carry only the **stage**, not a revision number.

## Reading order for a newcomer

1. [lidar-stage1-decoupling.md](lidar-stage1-decoupling.md) — what's wrong and why (root cause).
2. [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md) — the current fix.
3. [lidar-stage2-ldo-kicad-guide.md](lidar-stage2-ldo-kicad-guide.md) — how to capture it in KiCad.
4. [lidar-field-test.md](lidar-field-test.md) — how to verify range before/after.

## Known gap

[lidar-stage1-breakout-layout.md](lidar-stage1-breakout-layout.md) predates the LDO and covers only the C1–C3 decoupling placement. The Stage-2 LDO adds U1, Cin, and FB1; interim placement guidance lives in the Layout notes of [lidar-stage2-ldo-design.md](lidar-stage2-ldo-design.md). A dedicated Stage-2 layout doc is not yet written.
