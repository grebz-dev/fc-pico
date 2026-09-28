<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-20 Native sprite status bar, face, HUD and menus

| | |
|---|---|
| **Lane** | A/B/C -- host tests, Mesen co-simulation, NES-001 |
| **Size** | XL, staged as P3-U1..U6 |
| **Depends on** | Current working video/input baseline; I-15 dynamic S2 for final dynamic acceptance; P3-T2 save/load for transition coverage |
| **Work plan tasks** | [P3-U1..U6](../plan/10-workplan.md#p3-u1-sprite-ui-feasibility-and-layout) |

## Status (2026-09-27)

**partial**. The v4 native text row and live health pass on NES-001 at
`proto=4`, count 15122. A 35-sprite native status candidate with tall health
and armor, four background ammo counts, weapon/key indicators and a real 3×3
Doomguy face passes Mesen and timing checks; [HR-10](../HARDWARE-REQUESTS.md)
awaits its physical run. The full 42-face sheet is generated, with seven
representative faces resident pending expression paging. The earlier sprite
probe's `A` and diamond disappeared intermittently across power cycles, so
physical atlas persistence remains open. Full menu/foreground migration,
reverse recovery and dynamic S2 acceptance also remain open.

## Goal and specification

Replace background-converted foreground UI with sharper native sprite text,
symbols, status indicators, Doomguy face and menus. Keep the world stream and
working controller input. [12 -- Sprite UI](../plan/12-sprite-ui.md) defines the
source map, feasibility decisions, asset/palette rules, frame handoff, transport,
Mesen scenarios and hardware acceptance. This is required M3-UI work, superseding
optional P5-T3.

## Execution and ownership

This is a tracking issue, not an independent parallel ownership claim over files
already listed under I-14..I-17. Implement the stages sequentially and coordinate
those existing issues when shared paths change:

1. **U1:** inventory, native layouts, sprite/CHR/RAM/flash and byte/cycle budgets;
   decide the presentation interface and transport. Preserve baseline artifacts.
2. **U2:** generated tiles and a minimal sprite probe; verify physical pattern
   storage and fetch selection before final atlas allocation.
3. **U3:** negotiated bus/boot-ROM updates, OAM/palette staging, atomic commit,
   bounded timing, upgrade/recovery. No fixed-bank changes.
4. **U4:** migrate status/face, HUD, menus and remaining scoped foreground UI;
   remove matching legacy draws. Engine changes belong in the engine submodule,
   bus/asset/test changes in this repository; record both revisions when landed.
5. **U5:** fixed and dynamic Mesen UI fixtures, independent composite reference,
   controller-driven scenes, regression suite and reproducible CI artifacts.
6. **U6:** NES-001 legibility/controller/timing/soak validation of the tested build.

Likely implementation touchpoints are listed in plan 12, not modified by this
planning task. Do not edit `tutorial_project/` or the permanent fix bank.

## Acceptance

- Every required UI element is accounted for; all face states use the engine's
  existing selection. Labels baked into STBAR are included in migration.
- Native glyphs/icons and face are readable, without duplicate dithered UI,
  default flicker or silent missing information. Layouts fit real sprite limits.
- Asset and memory budgets pass; worst-case NMI includes DMA parity, PPU writes,
  UI transport, controller and audio work. The calibrated stream remains stable.
- Host rendering/transport tests and proposed U0/U1/U2 pass, with exact UI crops,
  OAM/CHR/palette dumps, input/state logs and generation/timing checks. Existing
  S0/D0/D1/S1 regressions remain green; dynamic S2 is not waived.
- Upgrade/recovery and physical NES-001 checks pass with recorded hashes and
  observations. Any background UI exception is an explicit accepted scope change.

U0's setup-time probe command is in
[the Mesen guide](../sim/mesen2/DOOM-FRAME.md). Future implementation must add
runtime U0 and exact U1/U2 scenario commands.
Record commands/results in [PROGRESS](../PROGRESS.md), physical observations in
[HARDWARE-LOG](../HARDWARE-LOG.md), and actionable candidate checks in
[HARDWARE-REQUESTS](../HARDWARE-REQUESTS.md). Do not mark done on planning alone.
