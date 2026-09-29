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
   the transparent game picture behind the menu. Preserve the source `M_DOOM`
   silhouette and blue/gold treatment in the reduced sheet; a generic redraw
   does not meet the visual goal.
4. Sharpening other static or non-3D UI elements (menu titles, cursor, options,
   episode choices and status decoration) when they can bypass world conversion
   without hiding controls or duplicating the original draw. Use the actual
   shareware episode names instead of generic `Episode 1/2/3` labels.

Record generated art dimensions, tile ranges, scanline occupancy and palette
choices; gate each change with Mesen cosimulation of title, main/episode/options
menus and gameplay. Keep HR-12 as physical evidence for the previous candidate;
new visual changes need their own hardware request and run.

### HR-13 physical correction (2026-09-28)

The paused-menu photo of the 9010 candidate shows status values over its upper
border, Doomguy over the lower divider and no sprite logo. The last finding
follows from the title-only 9010 OAM layout, so the 9011 follow-up adds a
source-derived 48×24 logo to the paused main menu. Its 18 entries are funded
by packing the six menu labels into 14 sprites and moving the static `H`/`R`
labels into the native status background (four OAM entries saved). The paused
main menu now consumes exactly 64 entries; retain the 64×32 source-derived
title logo when the status is absent. Move the numeric rows and 3×3 face
inside the panel bounds. Gate this exact-capacity layout in Mesen and on
NES-001, including a cold boot and pause after gameplay begins.

### HR-14 physical correction and Mesen iteration (2026-09-28)

The 9011 physical run exposed a retained logo corner outside menus and an
incomplete 3×3 face. Mesen reproduced both: an inactive menu packet with a
retained menu ID still entered the logo path, and health/armor plus the face
put nine sprites on each of the first two face rows. Keep an explicit
menu-active gate and check all 240 scanlines with the NES eight-sprite limit
enabled. Reposition the face and weapon/key rows in a central panel niche so
all nine face tiles remain visible during gameplay.

On the paused main menu, prioritize full-size labels and a larger, three-color
source-derived logo. Preserve health/armor values; the face, weapons and keys
may yield their OAM slots until the menu closes. Spell `READ ME` in full.
Restore Doom's original difficulty-name patches instead of substituting
abbreviations; long original names exceed the sprite-per-scanline bound and
need a separate native background-text treatment to become fully sharp.
Expose native-size PNG sheets and a deterministic PNG-to-CHR import so a human
can edit the logo, faces and paired episode letters. Keep this iteration in
Mesen and host-engine captures, with no UF2 build until the visual layout is
accepted for physical testing.

## Concrete status bar and sprite multiplexing study (2026-09-28)

**Planning only; production implementation and UF2 generation are not authorized
by this planning request.** The user asks to explore cycling, paging and ordering
before settling on background text. Research is in
[12b-sprite-multiplexing-research.md](12b-sprite-multiplexing-research.md).

### Visual target

Use `doom/StatusBar.png` as the supplied reference. Recreate gray concrete with
subtle mottling, recessed divisions and quiet areas under text, replacing the
bright metallic rails. Preserve this left-to-right order: AMMO, HEALTH, ARMS,
Doomguy, ARMOR, vertical keys, four-row ammunition inventory. Large ammo/health/
armor values are red with in-art dark contrast; health/armor use trailing `%`.
Small labels are white; arms and inventory counts preferably yellow, with white
inventory counts acceptable if required by palette constraints. Menus remain
transparent and difficulty patches retain their accepted current treatment.

Initial width allocation at 256 pixels: ammo 32, health 48, arms 32, face 32,
armor 40, keys 8, inventory 64. Test both a 32-pixel (four tile rows) bar and the
existing 48-pixel region. The former matches the requested compactness; the
latter gives label baselines and TV-safe padding. Neither height is final until
the native-resolution preview establishes legibility. A 32-pixel face needs
additional exterior space if an outer border is desired in a 32-pixel-high bar.

Use a 32x32, 4x4-tile face canvas, re-extracted from source patches with correct
proportions and a black backing; transparent canvas pixels reveal black. Preserve
idle, damage, pickup, god and death selection from engine state. Canonicalize
identical tiles under horizontal/vertical flips and keep per-placement flags;
never force symmetry onto lighting, injuries or directional expressions. Flips
save CHR, not OAM entries or per-line capacity. Eleven uncompressed faces alone
need 176 tiles, exceeding the current 128-tile resident region. Measure exact
savings and atlas allocation before selecting residency or bounded uploads.

### What the proposed techniques can and cannot buy

- **8x16 sprites:** a four-tile-high face needs eight OAM entries rather than
  sixteen; each line still uses four of the eight sprite slots. Tall numeric
  glyphs similarly need fewer entries. Larger vertical sprites do not widen the
  horizontal budget and transparent rows still participate in evaluation. Sprite
  size is a PPU-wide setting, so audit menus, logo, keys, paired tile alignment,
  flips and table selection. Odd tile indices select the $1000 table in this
  mode; do not accidentally fetch patterns from the streamed background table.
- **Packed glyph runs:** two small glyphs can share an 8-pixel tile when their
  advances permit. Tight crop/repacking and removal of wholly empty tiles can
  reduce real demand. Transparent pixels inside a present sprite do not release
  its slot. The inventory's 64-pixel-wide four rows are already estimated as
  packed runs, not one sprite per character. Compare readable source-derived
  fonts and actual masks; the numerical example below is not a universal lower
  bound on every possible font.
- **OAM rotation / temporal pages:** distribute omissions over successive PPU
  frames instead of permanently losing rightmost tiles. Schedule whole glyphs
  or field segments deliberately; naive rotation can show only half a numeral
  or face. All pages still obey eight per scanline and 64 total. This provides
  temporal visibility, not a complete image in each refresh. Fixed priorities
  can keep critical fields stable only by making other fields less frequent.
- **CHR paging / flipping / deduplication:** expand the available art and face
  repertoire; they do not create additional sprite shifters. The cartridge has
  no demonstrated arbitrary bank-switching or scanline IRQ interface. Prefer
  resident deduplicated art; any CHR upload path needs measured deadlines and
  coherent face replacement. Swapping CHR tiles at frame boundaries does not
  leave previous sprite pixels displayed underneath the new frame.
- **Draw order and priority:** select which eight eligible sprites win and how
  their pixels overlap the background. Behind-background priority, transparent
  masks and offscreen-X tricks cannot give a ninth sprite a rendering slot.
  A native BG layer can supply stable outlines/labels while sprites supply
  color or emphasis; that is a deliberate hybrid, not extra sprite capacity.
- **Mid-frame OAM reuse:** can replace the set for a later vertical band, helping
  the 64-total limit for tall scenes. It cannot add a ninth sprite on the same
  scanline. OAM writes during rendering are unsafe; forced blanking plus a full
  513/514-cycle DMA alone takes roughly 4.5 NTSC scanlines, before setup and
  recovery. This is costly inside a four-row bar and disrupts the calibrated
  stream. Exclude from the normal implementation unless a separate timing
  experiment establishes a useful, artifact-free benefit.

### Reproducible capacity calculation

`python3 doom/tools/check_sprite_layout.py doom/plan/sprite-ui-dense-feasibility.json`
checks deliberately overfull sprite-only sketches and a stable hybrid allocation.
A nonzero exit is expected for the two overfull sketches. These are conservative
rectangle allocations, not final artwork or Mesen results.

| Sketch | 8x8 entries | Peak per scanline | Overfull lines |
|---|---:|---:|---:|
| Dense original order, 48-pixel allocation | 89 | 27 | 40 |
| Same content in four tile rows | 89 | 27 | 32 |
| Continuous face + arms + keys, other fields native BG | 25 | 8 | 0 |

The dense peak comprises 11 large-number columns, four face columns, three arms
columns, one key and eight inventory columns. Equal four-page cycling can divide
27 entries into legal pages at a nominal 60 Hz refresh, giving each page 15 Hz
visibility and 25% duty. Pinning four face columns leaves four slots for the other
23 columns: at least six equal pages, or 10 Hz and about 17% duty. These are
optimistic equal-page examples, not a proven schedule; field grouping can cost
more. Irregular fair rotation changes the duty distribution but not capacity.
At nominal 50 Hz, four/six-page visibility is 12.5/8.3 Hz respectively.

The current UI_APPLY returns early for unchanged generations, and OAM DMA occurs
only when a new shadow is published. Text-update frames may defer OAM entirely.
Any flicker experiment must advance pages at PPU refresh cadence independently
of the roughly 25 Hz converted picture cadence and repeated UI generations.
Measure DMA cadence, scheduling fairness and worst-case NMI budget rather than
assuming every heartbeat can commit a new page. No production change is made by
this study.

### Proposed experiments before choosing the renderer

1. Build a source-art layout/atlas report for 8x8 and 8x16 variants, including
   exact flip deduplication, tile-table ownership, palette regions, OAM and all
   scanlines. Compare 32- and 48-pixel bar heights and packed-font legibility.
2. In an isolated Mesen probe, compare (A) stable native BG values/labels with
   sprite face/arms/keys, (B) a narrow two-page treatment for secondary fields
   with important values stable, and (C) the full sprite-only four-or-more-page
   layout as an explicit quality comparison. Candidate B still needs a valid
   allocation; it is not assumed to fit all requested content.
3. Include unchanged UI generations and slow/repeated picture frames. Disable
   sprite-limit removal. Capture consecutive individual frames, effective field
   duty, skipped-page counts and a real-time clip. Include page transitions
   during health/ammo/face changes. Do not present a temporal union or averaged
   PNG as an image the hardware displays in a single frame.
4. Provide full-screen PNGs, nearest-neighbor HUD crops and the source reference,
   plus the clip for any cycling candidate. A static PNG approves artwork and
   placement only; flicker needs motion inspection. Let the visual comparison
   determine whether any secondary-field cycling is acceptable. Do not silently
   adopt flicker for health/ammo or animation-sensitive face tiles.

### Implementation sequence after design review

Capture selected-weapon ammo and actual per-type maxima from the engine; support
backpacks, values over 100%, empty ammo for melee weapons, slots 2..7 in a 3x2
arms grid and card/skull semantics. Keep BULL/SHEL/RCKT/CELL in display order,
which differs from the current internal ammo-array order.

For a hybrid, compose text directly at native resolution, bypassing world
scaling/dithering. Audit 16x16 BG attribute boundaries; inventory begins at x=192
so a dedicated palette is plausible. Audit the existing omitted 28-tile native
text row and its calibrated read count before replacing its mechanism. Keep
background and sprite state coherent by generation and preserve legacy modes
and the permanent fix bank. The approved renderer choice determines transport
changes, not the other way around.

Validate native background texture, percentages, max capacities, all face states,
weapons/keys, death, automap, pause/menu open-close, stale menu IDs and reset in
host tests and Mesen. Maintain the accepted simplified paused HUD and transparent
menus; explicitly clear vacated OAM entries. Check cycles, CHR, read counts and
palette aliases. Expose editable concrete/font/key/face PNGs. Deliver the actual
Mesen output PNG (and cycling clip if relevant) for review before generating any
UF2. Research and capacity calculations do not establish physical flicker quality
or physical bus timing; hardware validation remains a later gate.

## Background-first follow-up and 8x16 evaluation (2026-09-29)

The 9013 candidate moves the cement panel, current ammo, health/armor
percentages, white labels, and four current/max inventory rows into the native
background conversion path. It uses the semantic UI snapshot before conversion,
so those glyphs bypass Doom's 320x200 scaling and dithering. The 28-tile v4
omitted row stays static and reserves a black face backing. The face is a 28x32
source-derived image in a 32x32 canvas, with tile sharing and PPU flip flags;
the arms grid and key icons remain sprites. This keeps the busiest scanline at
eight sprites. The supplied `StatusBar.png` is the visual reference. The
candidate is for physical validation; the Mesen output alone does not prove
color, overscan or readability on the user's display.

The next native graphics pass should treat **static content as background
composition** where possible. Build a host-side native overlay stage after
the Doom picture is scaled but before it is encoded for the PPU. It may combine
glyph pixels with the world pixels in each streamed pattern, preserving a
visible world under the menu without requiring a black menu rectangle. First
prototype main menu labels, then episode/options/save strings, then the logo.
Keep the Doom source patch silhouette for logo artwork. An ordinary NES
background glyph replaces its tile's world image; it cannot be transparent
over the world by itself. Pixel composition into the streamed tile is required
for the user's transparent-menu preference. Prove this in Mesen against a
moving world frame and assess readability at actual size.

Background palettes are shared by 16x16 attribute regions. Measure menu text
contrast and world-color loss where native red/white glyphs demand a reserved
palette. Do not assume a palette change can affect only glyph pixels. For
status content, retain a fixed concrete palette and generation-matched semantic
snapshot. Keep text out of the v4 skipped picture row unless its fixed native
tile path is changed and the calibrated transfer count is recalculated.

Evaluate 8x16 sprites on a separate Mesen branch after the background overlay
removes menu glyph pressure. Each 8x16 entry covers twice the vertical area
of an 8x8 entry, but the eight-per-scanline bound is unchanged. PPUCTRL sprite
size is global, so all remaining sprites, including menu logo, face and keys,
must be repacked as aligned tile pairs. Verify pattern-table selection from
tile bit zero, OAM counts, every status scanline, CHR residency, and a real
face/menu transition before selecting the mode. Retain 8x8 if the global
repacking costs more than it saves. Consider putting even the face into the
background only when a black square can be updated atomically with its state
and without lengthening the read schedule or causing one-frame expression lag.

Compare screenshot crops and 30 consecutive Mesen frames for text contrast,
face changes, transparent menus, and zero leftover sprites after menu close.
Capture NMI cycles, v4 read count, and 64-entry/8-per-line limits. Generate a
review PNG and motion clip before any successor hardware build. Maintain the
9013 UF2 as a tested checkpoint while this redesign is explored.

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
