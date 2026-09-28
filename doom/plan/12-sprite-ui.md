# 12 -- Sprite UI and legibility

## Status and objective (2026-09-25)

**U0 setup-time sprite probe passes in Mesen and partly on NES-001; production UI remains open.** Doom loads, displays and
accepts controller input on NES-001. Preserve that milestone while replacing the
background-converted status text, symbols, indicators, Doomguy face and menu/HUD
content with sharp, native-resolution sprite representations. This is required
playability work, promoted from optional P5-T3 polish to **M3-UI**, a gate within M3.
The world remains a streamed background. Decorative panels and full-screen artwork
may remain background content; foreground UI must not silently retain its old,
dithered representation when declared migrated.

This document is the specification; [10-workplan.md](10-workplan.md) schedules
P3-U1..U6. [I-20](../issues/I-20-sprite-ui.md) tracks the work. Runtime formats,
final asset layouts and U1/U2 scenarios remain future work.

## Visual follow-up after HR-12 (2026-09-28)

HR-12 passed on NES-001: the native red text and gray status backing are more
legible, the menu backdrop is transparent, and pickup/ouch faces work. The next
art pass should keep the working v4 transport and menu transparency while:

1. Replacing the flat gray status backing with a native-resolution styled panel
   that recalls Doom's original status bar. Use a constrained gray/metal palette,
   borders and section separators, keeping the numbers, 3×3 face, weapons and
   ammo legible. Test contrast on real game backgrounds and respect the existing
   background tile/palette budget.
2. Adding contrast to red native text, preferably a dark outline or shadow in
   the same sprite tile/palette rather than extra overlapping sprites. Check
   OAM occupancy and the eight-sprites-per-line limit in gameplay and menus.
3. Building a deterministic, source-attributed sprite sheet for the Doom menu
   logo and rendering it natively where OAM and scanline capacity permit. Check
   visible width and sprite CHR residency before choosing a logo size; preserve
   the transparent game picture behind the menu.
4. Sharpening other static or non-3D UI elements (menu titles, cursor, options,
   episode choices and status decoration) when they can bypass world conversion
   without hiding controls or duplicating the original draw. Use the actual
   shareware episode names instead of generic `Episode 1/2/3` labels.

Record generated art dimensions, tile ranges, scanline occupancy and palette
choices; gate each change with Mesen cosimulation of title, main/episode/options
menus and gameplay. Keep HR-12 as physical evidence for the previous candidate;
new visual changes need their own hardware request and run.

## Findings from the current source

| Source | Current behavior and consequence |
|--------|----------------------------------|
| `rp2040-doom/src/fcpico/i_video_fcpico.c`: `compose_frame`, `draw_patch_row` | Composes base rows and vpatch overlays into 320-pixel indexed lines, then calls `fcvideo_line_sink` and `fcvideo_frame_end`. Overlay identity is lost after composition; extract UI before this seam, not by recognizing pixels afterward. |
| `rp2040-doom/src/doom/st_stuff.c`: `ST_drawWidgets`, `ST_Drawer` | Emits STBAR, ammo/max ammo, health/armor percentages, weapon ownership, keys and face patches; existing game logic already selects the correct face. Preserve that selection and timing. |
| `rp2040-doom/src/doom/hu_stuff.c`, `hu_lib.c`, `m_menu.c`, `st_lib.c` | Own HUD strings, menu labels/cursor and widget drawing. Inventory these producers and their vpatch handles, including dynamic text and translated colors. Do not classify every vpatch as UI: other screens also use patches. |
| `port/video/fcvideo.c` | Decimation, vertical scaling, block-palette selection and dithering currently affect the composed UI as well as the world. Native NES UI assets must bypass these transformations. |
| `rp2040-doom/src/fcpico/{host_main,device_video_fcpico}.c`, `fcpico_video_sink.h` | Host and device frame sinks are the existing adapters. Extend their shared frame handoff to carry immutable UI state with the matching background frame. |
| `fcbus/fcbus_protocol.h`, `fcbus_core.{c,h}` | v2 has 128 mailbox bytes: commands, 15 APU pairs, 16 BG palette bytes and 64 attribute bytes. There is no sprite palette or UI-state transport. |
| `bootrom/src/PG_main.asm` | Setup parks all OAM entries at Y=$EF. PPUCTRL=$88 uses sprite patterns at $1000; PPUMASK=$1E already enables sprites. NMI receives the mailbox and writes BG palette/attributes, then sends the heartbeat and restores $0801/scroll. |
| `fcbus/fcppu.pio`, `sim/mesen2/Mesen2/Core/NES/Mappers/Homebrew/FcPico.h` | Firmware advances on CS1-selected strobes; the calibrated mapper selects $0800-$0FFF. Sprite patterns at $1000 are outside that stream window. The mapper provides 8 KB CHR RAM, which is a model assumption, not proof of the board's independent writable capacity. |
| `sim/mesen2/DOOM-FRAME.md`, `tests/bootrom/` | Strict S0, fixed D0/D1 and S1 reflash are established gates. D1 measured 1614 critical / 2021 total NMI cycles; dynamic S2 is still open. |

The permanent font installer at `tutorial_project/BOOTROM_FIX/PG_main.asm` and
`SysBootRom.asm` is useful evidence for local pattern storage, but must remain
unchanged. Verify physical address mirroring/capacity before allocating an atlas.
Do not infer sprite support from the tutorial's software `Canvas` sprite routines.

## Feasibility gates before committing the layout

1. **Pattern storage:** map writable, non-streamed pattern addresses, mirroring,
   system-font ownership and reset/reflash behavior. Start with 8x8 sprites at
   $1000, preserving current PPUCTRL/PPUMASK and background nametables. Prove a
   small tile upload and visible sprite in Mesen, then on NES-001. If $1000-$1FFF
   is independently writable, it offers at most 256 16-byte tiles; reserve actual
   aliases/font space first. Do not widen CS1 emulation to make a test pass.
2. **Sprite capacity:** enforce 64 OAM entries total and eight sprites per scanline
   with Mesen's sprite-limit removal disabled. Eight 8-pixel-wide sprites cover
   at most 64 opaque horizontal pixels on a line. The current wide status bar,
   long HUD messages and menu rows cannot simply become one sprite per glyph.
   8x16 sprites do not increase that horizontal limit and change pattern addressing.
3. **Timing:** the existing v2 NMI cannot just gain a full OAM DMA. Its measured
   1614/2021 cycles plus a 513/514-cycle DMA already exceed the 1900/2200 design
   limits, before UI reception, sprite palette writes or OAM construction. Measure
   both DMA parities, all valid flags and maximum APU traffic. Count CPU reads,
   transfers and register restoration, not just DMA cost.
4. **Memory and assets:** budget console RAM, erasable PRG bank, RP2350 static RAM,
   zone headroom, flash before WHX, and CHR tiles for every screen. `$0200-$02FF`
   is a candidate aligned OAM shadow page, subject to a fixed-bank RAM audit;
   do not use fixed scratch, controller, reflash or mailbox locations.

Deliver a pixel layout and scanline occupancy report for every UI mode before
migrating widgets. Prefer compact packed glyph runs, vertically separated groups,
a smaller face, wrapping and paged menus to flicker or missing information. A
packed run can hold multiple narrow glyphs in each tile but still obeys the
64-pixel coverage bound. A 24x24 face alone needs nine 8x8 entries and three
slots on each of its 24 scanlines; compare it with a 16x16 face in mockups.

The 2026-09-25 scope decision permits native-resolution background tiles for
long text and menu rows. Keep the compact status, resolved face, indicators,
cursor and icons on sprites where capacity permits. Native text must use local
font patterns and nametable cells, bypass the world converter, and preserve the
stream's calibrated fetch address/count. This requires a new layout and timing
proof: writing font tile IDs over the current `$80` stream nametable changes
which PPU fetches hit CS1. Do not ship the old dithered foreground as this
exception. Do not use
unlimited-sprite emulator settings, disappearing indicators, alternating-frame
text or raster OAM multiplexing as the default solution.

## Proposed rendering seam

Create a small UI module shared by host and device. Its interface consumes a
bounded, immutable presentation snapshot and returns a bounded render package
plus diagnostics. The implementation owns layout, glyph packing, tile allocation,
OAM ordering, clipping, palette mapping and capacity checks. Engine adapters
supply semantic values and already-resolved face/menu selections; tests use the
same interface. Keep Doom simulation, RNG, face logic and controller mapping in
the engine. Do not clone them in the 6502 ROM.

A snapshot needs a presentation generation/frame ID, active screen/mode, visible
widget values, resolved face identity, strings, menu selection, palette effect
and transition state. First compare a semantic snapshot with classified draw
commands against the actual producers; settle the minimal representation in U1.
Copy bounded values at the engine handoff: no pointers into mutable overlay lists
may outlive their frame, and no malloc is allowed in core-1 conversion/IRQ work.

The render package includes OAM intent, referenced tiles/atlas generation, sprite
palette and diagnostics (total/per-line occupancy, clipping, unsupported assets,
update bytes and estimated/measured cost). Host dumps and device publication
must consume identical packages. Publish the background, UI state and palette as
one presentation; retain the last complete presentation on a missed deadline.
Do not swap new OAM against old tile contents. A partial multi-frame upload stays
invisible until its generation is complete.

Suppress the corresponding legacy draws only when a UI item is represented by
the new package. Include static labels baked into STBAR: suppressing number/face
patches alone leaves duplicated or blurry labels. Preserve a clean background
panel without foreground UI. A diagnostic legacy mode remains useful for A/B
capture and regression isolation during rollout.

## Asset and palette plan

- Author or derive native 256-wide-screen 2bpp art, with integer pixel placement,
  hard edges and no world dithering. Use deterministic conversion and manifests
  recording source WAD/WHX identity, license, dimensions, palette and tile IDs.
  Reuse the existing resource/build pipeline; do not redistribute new WAD-derived
  assets without retaining the project's licensing constraints.
- Include digits, percent sign, punctuation, HUD/menu alphabet, keys/skulls,
  weapon ownership states, cursor, sliders, pause/save indicators and all face
  states selected by the shareware engine (pain levels, looks, grin, damage,
  rampage, god/invulnerability and death). Verify selections from the engine;
  do not approximate them with a new animation timer.
- Use four sprite subpalettes, with three visible colors and transparent index
  zero per sprite. Handle $3F10/$14/$18/$1C palette aliases so sprite uploads
  cannot change the BG backdrop accidentally. Explicitly choose and test readable
  damage/bonus/radiation behavior rather than inheriting world quantization.
- Prove atlas fit. Prefer resident common glyphs/icons and bounded face tiles;
  stage screen-specific banks while rendering is disabled at a deliberate screen
  transition, or use measured bounded updates. No routine full-atlas upload in NMI.
  A face change must not blank the screen. Use generated tile/flash budget reports.
- Define placements in the native 256x240 coordinate system, with a conservative
  TV-safe inset; reconcile with the current world at lines 8..231. Native glyphs
  are not vertically stretched with the 200-to-224-line world. Record the chosen
  status area/view geometry and confirm it on a physical display.

## Transport, boot ROM and publication

Prefer persistent non-streamed sprite tiles plus compact UI updates; raw full OAM
transport is a comparison case, not the assumed protocol. Compare bounded OAM
changes with a compact descriptor decoded into the OAM shadow outside critical
PPU work. A compact 6502 decoder may expand positions/tile IDs, but must not know
Doom gameplay rules. Quantify worst-case menu replacement and simultaneous face,
health, key and palette changes, not just steady-state numbers.

Keep v1/v2 behavior and constants as regressions. Define a negotiated successor
only after a byte/cycle table proves it fits. Do not silently reuse live v2 APU,
command or attribute bytes. Coordinate the version with the existing audio task's
planned "v3"; protocol version and ROM build stamp are separate decisions.
Generate C/assembly/Python constants from `fcbus_protocol.h` as today.

The design must specify fixed transfer lengths/padding, bounds, validity/version,
frame generation, reset behavior and invalid/incomplete update handling. Prefer
a constant read count per negotiated mode even when data is unchanged; skipping
PPU writes can save cycles without skipping stream reads. If mailbox length is
N and rendering selection is unchanged, the expected zero-based report is
15426 + N, including the established dummy-read/bias convention. Re-derive this
with traces; never replace the v2 count 15554 globally.

Fit OAM preparation, DMA, BG attributes, both palettes, controller heartbeat and
APU work into a measured schedule. Consider bounded dirty PPU writes, staging
outside NMI and multi-heartbeat preparation with atomic commit. Preserve controller
polling even when a UI upload waits. Reserve time for future audio/DMC stalls;
any changed APU cap must be documented with the audio plan. Retain the $0801 and
scroll restoration and no-$2007-after-heartbeat rule unless a new schedule is
independently proven. An over-budget path delays a complete presentation rather
than tearing it or silently extending vblank limits.

Initialize hidden OAM, then enable only a complete atlas/palette/UI generation.
Clear stale entries on menu close, reset, load, death and fullscreen changes.
Define wipe behavior explicitly: use a coherent old/new presentation or hide the
UI during the wipe, then restore the matching final state. Saving must use
resident data/code under the existing flash-programming restrictions. Test both
upgrade and recovery through the unchanged permanent fix bank; bump the erasable
ROM stamp when its behavior changes.

## Validation and artifacts

**U0 now has a setup-time sprite probe runner** in
`sim/mesen2/DOOM-FRAME.md`; its runtime transport and NES-001 checks remain
open. U1 and U2 are still proposed scenarios. Mesen runs the actual NES CPU/PPU with a native host cartridge model;
it does not validate ARM instructions or physical PIO/DMA timing.

| Layer | Required checks and retained artifacts |
|-------|---------------------------------------|
| Host UI/asset tests | Known snapshot to tiles/OAM/palette; exact glyph masks, transparency, clipping, digit limits, all face IDs, max strings and menu replacement; deterministic generation and resource budgets. Capacity overflow fails visibly in diagnostics. |
| Independent image reference | Extend the BG-only decoder with NES sprite compositing (OAM order, per-line selection, Y convention, priority, flips, palette aliases and transparency). Compare expected masks and labeled UI crops as well as full frames; whole-screen PSNR can hide unreadable text. |
| Bus/boot-ROM tests | Negotiation, fixed length/count, malformed/unknown/truncated update, repeated frame, timeout/reset, deferred commit and generation consistency. py65 tests cover full/empty/dirty cases, both DMA parities, max audio and OAM/PPU write deadlines. Keep fix-bank byte equality and ROM reproducibility checks. |
| U0: sprite transport | Known glyph, icon and face tile grid over the calibrated stream. Assert OAM/CHR/sprite palette contents, BG equality, eight-per-line enforcement, stable heartbeat/count and no new DMA stops/resyncs. Deliberately test a ninth sprite to prove the harness enforces the hardware limit. |
| U1: fixed Doom UI | Extend D1-style replay to a versioned fixture containing BG stream, UI payload, atlas and hashes. Dump final image, UI crops, OAM, sprite palette, CHR references, mailbox, generation and NMI timing. Compare against an independent expected composition, not only a golden produced by the same renderer. |
| U2: dynamic Doom UI | Extend the pending S2 engine integration and S3 controller scenarios; drive real menu/input paths and log engine state alongside UI generation. Cover damage, pickups, ammo/weapon/key changes, every face fixture, long messages, menus/sliders/save names, automap, pause, fullscreen, wipe, save/load/reset and repeated frames. Run 3000-frame deterministic playback plus a stress sequence that changes all UI fields. |
| Mesen regression | Strict S0 including debugger-peek invariance, D0/D1 and S1 remain green for legacy modes. New protocol gets equivalent upgrade/recovery, count, table and timing gates. Disable sprite-limit removal/overclock enhancements; archive emulator commit/configuration. Require PPU writes complete before rendering resumes and NMI timing within the agreed measured limits. |
| Device and hardware | Build/layout gate, RAM/zone/flash margins, conversion and UI packing/publication timings, frame/drop counts and input response. NES-001 captures of the same UI scenes at normal viewing size, sprite flicker/overflow, overscan, palette flashes, controller use, cold boot/reset and 30-minute soak. Retain the later one-hour release soak. |

Reuse current build and validation entry points (commands in the
[build guide](08-build.md), [testing plan](09-testing-ci.md) and
[D0/D1 guide](../sim/mesen2/DOOM-FRAME.md)). Expand `tests/fcvideo`, `tests/fcbus`,
`tests/bootrom`, `tests/cartmodel`, `tests/mesen2` and a proposed `tests/ui` suite
only where behavior changes. Host dumps must carry enough UI state to replay
without the engine. Add scenario/fixture support to `sim/cartmodel`, the Mesen
runner/Lua and mapper only where hardware behavior requires it. Do not grant the
mapper storage or sprite capabilities the board has not demonstrated.

Archive source/submodule revisions, ROM/UF2/WHX/atlas hashes, fixture inputs,
commands, counters, cycle reports, screenshots and reference diffs in each run.
Update the cosim workflow template and activate its CI lane when reproducible;
local passes and remote CI passes are separate evidence. Add hardware requests
when a tested candidate exists, then put observed results in HARDWARE-LOG and
PROGRESS. This planning change does not claim any new hardware or emulator pass.

## Completion gate

M3-UI requires all scoped foreground UI to have an accounted-for representation,
reviewed native-resolution screenshots with sharper readable text, no duplicate
legacy text/face, no missing information or default flicker, valid tile/OAM/cycle
budgets, passing host and Mesen gates, and a matching physical NES-001 playtest.
Any background-rendered exception must be explicitly recorded and accepted.
The UI must remain synchronized with gameplay and controller-driven menus during
slow conversion and transitions. The existing <=8 ms converter target, dynamic
S2 and broader M1/M2/M3 gates remain open until independently satisfied.
