# 14 -- Enemy and pickup sprites

Date: 2026-10-04. Status: P3-W1..W4 partial. Review chose RP2350 LUT/DDA
scaling. Protocol v5, the `20DOOM-05-9031` ROM, engine admission/masking, per-tile
dynamic palettes and the full shareware atlas pass end-to-end Mesen and host
gates. On hardware ([HR-17](../HARDWARE-REQUESTS.md) first pass) colours are good
but frame rate dropped with enemies on screen. The second candidate cuts the
transport cost per picture in Mesen from 6.89 to 4.99 heartbeats (5.62 with
modelled conversion, versus 3.01 streamed): anchored scale-stepped tiles that
repeat, packets that outrank HUD DMAs, and uploads overlapping conversion. The
HUD now shares BG palette 2 with the world. P3-W5 and physical acceptance remain.
See [probe results](14b-world-sprite-probe-results.md) and
[PROGRESS](../PROGRESS.md).

## Objective and starting design

Make enemies and item pickups clearer by drawing their billboard art as native
NES sprites over the streamed 3D background. Retain Doom's animation, viewing
angle, patch origins, perspective, lighting and geometry occlusion. Start with
one pickup and one enemy, then expand to the shareware actor inventory. Weapons,
projectiles, decorations and corpses must be classified explicitly; they are
not automatically included because they share Doom's software-sprite renderer.

The initial candidate is **RP2350-generated, scaled and visibility-masked 2bpp
tiles**, front-priority OAM, and coherent background/CHR/OAM publication. Compare
resident size variants against runtime LUT scaling. Keep streamed rendering as
an explicit capacity fallback: the NES cannot promise hardware sprites for
every actor in an arbitrary crowded or close-up view. Quantify coverage before
choosing how much of the game can migrate. No actor may silently disappear.

This extends [12](12-sprite-ui.md)'s streamed-world assumption for selected actors.
UI acceptance, converter timing, audio deadlines and fixed-bank preservation
remain requirements. Mapper extensions are a separate experiment, conditional
on capabilities demonstrated on the physical FC PICO cartridge.

## Source baseline and integration seams

- [r_things.c](../rp2040-doom/src/doom/r_things.c): `R_ProjectSprite` already
  chooses frame, rotation/flip, patch, position, scale and light. Export actor
  intent here, before it becomes pixels. `R_DrawPSprite` handles the held weapon
  separately. Preserve fullbright, translation and shadow/fuzz cases, initially
  keeping unsupported effects in the streamed path.
- [CMakeLists.txt](../rp2040-doom/src/CMakeLists.txt): the Pico configuration sets
  `NO_DRAWSEGS`, `NO_VISSPRITES`, `NO_MASKED_FLOOR_CLIP` and `PD_SCALE_SORT`.
  `R_ProjectSprite` calls `R_DrawSpriteEarly`; copying classic `R_DrawSprite`'s
  drawseg clipping loop alone will not provide visibility in this build.
- [pd_render.cpp](../rp2040-doom/src/pd_render.cpp) resolves overlapping column
  lists by scale and clips/splits their visible intervals. Investigate exporting
  actor ownership and visible spans from this resolution, including planes and
  masked midtextures. A single nearest-wall depth per X is insufficient for
  door openings, upper/lower walls and fences with transparent holes.
- [fcvideo.c](../port/video/fcvideo.c) maps source X using
  `(x / 4) * 5 + (x & 3)` and maps 200 source rows into 224 output rows with an
  eight-line top margin. Use the same mapping for actor bounds and masks;
  account for native status overlays and view-size changes. Do not independently
  round the sprite and wall edge and create a one-pixel seam.
- [build_native_status_probe.py](../tools/build_native_status_probe.py) generates
  the current 9016 UI ROM from the earlier transport/text probes. Budget against
  that generated ROM, not just the older base `bootrom/src/PG_main.asm`.

## Automatic WAD-to-sheet pipeline

Proposed tool: `tools/build_world_sprite_art.py`, with a source WAD, supported
actor manifest and output directory as explicit inputs. Deliver source-resolution
RGBA sheets, indexed sheets, tile data and a machine-readable manifest.

1. Reuse or factor the validated IWAD/PWAD directory reader in
   [audio/wadextract.py](../tools/audio/wadextract.py), preserving directory order
   for sprite namespaces and last-lump-wins behavior. Read `PLAYPAL`, sprite
   marker ranges and named patches; validate dimensions, signed origins, column
   offsets, post lengths and terminators before decoding. Empty posts are
   transparency, not Doom palette index zero. This project targets shareware;
   unsupported archive/patch variants should report a clear error.
2. Derive enemy/pickup families and animation frames from the engine's
   [info.c](../rp2040-doom/src/doom/info.c) state/mobj tables and sprite definitions,
   with a reviewed manifest for scope. Parse names such as `TROOA1` and paired
   `TROOA2A8`: paired names share a patch with a mirrored view; rotation zero
   represents all views. Check missing frames/rotations and preserve death,
   attack, pain and animated-pickup states that belong to supported actors.
3. Preserve left/top offsets and a stable world/foot anchor across frames.
   Transparent cropping records its offset; it must not move the actor. Pack
   deterministic sheets for editing and inspection, retaining original patches
   alongside reduced candidates. Map WAD lump identity to WHX/runtime patch
   identity during generation; the engine uses compressed drawable data, so
   sheet order must not become an accidental runtime index.
4. Quantize opaque art to three visible colors per sprite subpalette, reserving
   index zero for transparency. Evaluate shared enemy/pickup palettes alongside
   all four existing UI palettes, light levels and damage/bonus flashes. Prefer
   recognizable silhouettes and stable colors over fine world-style dithering.
   Any outline must fit the actor's bounds/budget and must also be occluded.
5. Generate candidate scale bands, tile placements and flip metadata. Deduplicate
   exact tiles under flips only where placement flags preserve the original art;
   masked instances need separate tiles when visibility differs. Omit wholly
   transparent tiles. Publish bytes before/after deduplication, per-frame OAM,
   peak scanline occupancy and projected scale range.
6. Record WAD/content hashes, lump/frame/rotation mappings, palette, tool options,
   offsets and output hashes. Require reproducible regeneration. Existing
   [face](../tools/build_doomguy_faces.py) and
   [menu](../tools/build_doom_menu_art.py) tools provide CHR/sheet conventions;
   their WHX vpatch decoders are not raw WAD sprite-patch decoders. Follow
   [LICENSES.md](../LICENSES.md) for generated art and source attribution.

## LUT scaling study

[scaling-wip](../reference/scaling-wip/README.md) supplies a useful primitive,
not a complete perspective renderer. Its
[generator](../reference/scaling-wip/tools/mkscaletable.py) emits five 256-byte
tables mapping an 8-bit row to widths 8, 7, 6, 5 and 4. Both bitplanes use the
same lookup; vertical shrinking skips rows with a DDA.
[scaling.s](../reference/scaling-wip/src/scaling.s) expects interleaved Game Boy
source planes and repacks output for NES CHR. Adapt formats explicitly rather
than feeding ordinary NES planar tiles directly into that routine.

The reference estimates 74 CPU cycles per row / 4736 cycles for eight tiles of
scaling work and advertises eight tile uploads per vblank, or about 15 fps for a
32x64 character. Scaling work occurs outside the upload window; this is not an
FC PICO timing measurement or a promise that the current NMI has room for it.

Compare three candidates: precomputed size bands in flash; RP2350 LUT/DDA
scaling plus a bounded cache; 6502 reference scaling as a measured control.
Prefer RP2350 computation initially, leaving NES cycles for reception and PPU
writes. For wider art, concatenate/shift packed slivers across destination tile
boundaries: shrinking each eight-pixel source tile into its own eight-pixel OAM
cell leaves gaps and does not reduce horizontal occupancy. Preserve sampling
phase across slivers, support scales below half-size through additional levels,
and use a separate enlargement/near-view fallback; the reference only shrinks.

Cache unmasked tiles by patch/frame, rotation/flip, size and palette treatment.
Generate instance-specific masked tiles after scaling. Measure camera-motion
cache churn, scale-band popping, foot-anchor jitter, mask cost, flash/XIP traffic
and worst-case CPU/RAM cost. A moving corner changes the mask even if the actor
frame and scale stay cached.

## Budget against the current implementation

The 9016 generator reserves 35 status OAM entries, 28 menu entries and one cursor.
Gameplay actually uses up to 25 status sprites (16 face, six arms, three keys),
with ten reserved status entries hidden; menu slots are normally hidden. With
that partition retained, world allocation starts at **29 entries**, not 64.
Repacking hidden status slots could reach 39 alongside those 25 active entries,
but requires a unified allocator and mode-transition tests. Paused/title menus
reuse status slots for a 32-sprite logo and menu content; initially keep the
world in the stream while menus are open.

The current face is at visible Y=200..231. Its four columns, three arms and one
key can fill all eight scanline slots. World sprites must clip at the native
status boundary or compete in the same combined occupancy calculation. Empty
pixels in an allocated tile do not release its scanline slot. The NES supports
64 entries total and eight evaluated sprites per scanline; 8x16 halves entries
for suitable art but does not increase horizontal capacity and changes sprite
size globally for the UI too.

Conservative solid-rectangle costs, before empty-tile removal or deduplication:

| Native actor size | 8x8 entries | Tiles / CHR bytes | Slots per covered line | Consequence with 29 world entries |
| --- | ---: | ---: | ---: | --- |
| 16x16 pickup | 4 | 4 / 64 | 2 | Seven fit by total OAM; only four on a common line without UI |
| 24x32 enemy | 12 | 12 / 192 | 3 | Two fit by total OAM; overlapping rows use six slots |
| 32x64 enemy | 32 | 32 / 512 | 4 | One already exceeds the retained world partition |
| 48x64 enemy | 48 | 48 / 768 | 6 | Exceeds even the 39-entry repartition; needs UI changes or fallback |
| 64x64 near enemy | 64 | 64 / 1024 | 8 | Exhausts the entire OAM array before UI |

These are calculated bounds, not measured gameplay occupancy. Count actor
instances separately from reusable pattern tiles. Two instances sharing art
still need distinct OAM placements and may need distinct occlusion masks.

Current [native_status_art.py](../tools/native_status_art.py) assets contain
119 deduplicated face tiles in a padded 128-tile / 2048-byte upload at
`$1800..$1FFF`, three key tiles at `$12E0`, six arms tiles at `$1320`, and
32 logo tiles at `$1600`. System font and other inherited art also occupy
the sprite table. Nine padding tiles are not a useful general world atlas;
produce an exact live CHR ownership map before reserving any supposedly free
region. Setup-time residency proves neither continuous upload bandwidth nor
arbitrary bank switching. All four UI subpalettes have existing uses.

The feasibility report must give current and proposed worst cases for each
mode: combined OAM/scanlines, resident/cache/double-buffered CHR, palettes,
RP2350 masks/cache/zone headroom, console staging RAM, flash before WHX, mailbox
bytes, 6502 receive/construction/PPU cycles, OAM DMA parity, converter and total
frame time. For example, refreshing 32 unique tiles costs 512 CHR bytes before
descriptors, palette or OAM work; at 20 updates/s that is 10,240 CHR bytes/s.
Average bandwidth alone cannot establish that a burst fits vblank.

## Replacement and visibility contract

Choose admission before final background rendering: supported actors receive
sprites only if complete art, palettes, tile storage, line capacity and upload
deadlines fit. Prefer stable actor IDs and hysteresis over arbitrary OAM rotation.
Keep critical UI stable; rank visible threats and nearby pickups explicitly.
On overflow or unsupported effects, render the whole affected actor through the
legacy stream for that generation. Report fallback counts and coverage.

The accepted actor's old framebuffer draw must be suppressed, while geometry
behind it must still be drawn. Simply skipping rasterization after PD insertion
can leave holes because the actor has already clipped away background columns.
Prototype a geometry-only visibility/background pass plus actor mask queries,
or modify PD ownership/resolution to retain underlying geometry for accepted
actors. Compare cost and memory before selecting the seam. Actor masks must also
respect nearer streamed actors, the held weapon, masked fences, planes and UI;
front-priority OAM will otherwise incorrectly cover them.

Use the geometry-visible actor mask in native output coordinates. For each
scaled tile row, clear both planes where the actor is hidden:
`plane0 &= visible_bits; plane1 &= visible_bits`. This yields transparent sprite
pixels at corners, doors and partial top/bottom edges without altering the
background's palette-zero pixels. Drop fully masked tiles; partial tiles keep
their normal OAM evaluation cost. Preserve masks separately from reusable art.
For overlapping hardware actors, near-first OAM ordering works only with an
allocator that already enforces scanline capacity; export resolved ownership
where mixed streamed/hardware actors require finer masking.

Publish background, actor descriptors, masked CHR, UI, palettes and OAM with one
generation. Stage uploads without overwriting currently referenced tiles; switch
only when complete. If storage/bandwidth cannot support that, repeat the complete
previous generation or produce a fresh all-streamed fallback. Never repeat actor
OAM against a newer camera/background. Clear actor slots on pickup removal,
death/state changes, menu/automap entry, wipe, save/load, reset and timeout.

## Traditional and homebrew mapper comparisons

The source-backed comparison is in
[14a-world-sprite-occlusion-research.md](14a-world-sprite-occlusion-research.md).
Evaluate these alternatives against the baseline above:

- Behind-background sprite priority: test the classic nonzero BG foreground
  mask, but world color zero already has visual meaning. Ordinary priority
  cannot choose geometry by depth and would hide an actor behind unrelated
  opaque background. A converter-owned mask would constrain world colors and
  must preserve different actor depths; quantify those costs.
- Occluder sprites and tile-aligned splits: useful limited probes, but occluder
  sprites consume the same OAM/line capacity, cannot generally reproduce a
  textured streamed wall, and cannot provide arbitrary pixel clipping by
  placement alone. Compare coarse cropping with CHR bit masking at real edges.
- Traditional CHR banking / raster IRQ mappers: explore resident scale banks
  and vertical-band reuse. These can help storage or scheduling, but do not
  add per-pixel depth, change the eight-sprite limit, or provide arbitrary
  horizontal clipping. Record actual bank granularity and raster costs.
- Homebrew sprite-fetch substitution: investigate whether a mapper can serve
  scaled/masked sprite bytes from cartridge-side buffers with a coherent latch.
  Establish fetch/address/instance identification, repeated-tile behavior,
  bus response deadline, pre-render/dummy fetch handling and independent BG
  stream counts. FC PICO's present selected BG stream is not evidence of this
  capability. Model extensions in Mesen must be marked experimental until
  matching electrical hardware exists and passes a physical probe.

## Deliverables and acceptance

[10-workplan.md](10-workplan.md) assigns P3-W1..W5: budget/visibility study;
deterministic extraction and scaling; bounded sprite transport probe; mixed
rendering and masking; dynamic regression and hardware acceptance.

Require independent decoded-mask/image comparisons and consecutive-frame Mesen
captures with sprite-limit removal disabled. Include one pickup and every enemy
view/animation; half-hidden corners on both sides; upper/lower door edges; floor
steps; grates with holes; two actor depths; moving occluders; offscreen bounds;
near-camera large art; crowded views; HUD overlap; fullbright/shadow effects;
pickup disappearance; menu/automap/wipe/save/load transitions; stalled uploads.
Retain strict S0, D0/D1, S1, dynamic S2/UI checks and combined audio/NMI gates.

Compare identical camera/tic frames with the current stream for silhouette and
recognition, clipping correctness, total rendering/conversion/upload time, fps,
fallback rate and palette stability. Establish useful migration coverage at the
20 fps playability target and existing converter budget before broad adoption.
Run NES-001 timing/visual checks and a 30-minute soak after Mesen passes. A single
clear enemy demonstration is evidence for the probe, not completion of migration.

## Implementation notes

- 2026-10-02: add deterministic imp/stimpack extraction and resident-band, host
  LUT/DDA and NES 6502 LUT cosimulation samples. Correct the atlas assumption:
  the probe borrows gameplay-inactive menu glyph tiles at `$1380..$15FF`; these
  require restoration before menus. Setup residency is not runtime upload proof.
  P3-W1/W2 remain partial and W3/W4/W5 open; see the linked results and limits.
- 2026-10-03: implement the runtime after review (RP2350 LUT/DDA). Corrections:
  per-frame CHR cannot use the probe's menu-glyph borrowing; 53 tiles unused by
  every HUD/menu/logo layout form the pool, so no restore exists. Sprite palette
  writes belong to the commit packet, not OAM DMA (2213 > 2200 cycles). Admitted
  actors are withheld from PD insertion and masked by resolved front-span depth,
  not by owner sidecars. Native updates measure about 8.8 fps under motion;
  bandwidth, not occlusion, is the limiting factor for the 20 fps target.
- 2026-10-03 (colour review): one 3-colour palette per family cannot carry Doom
  actors. Atlas v2 keeps 15 local colours per family; frames load per-family
  candidate palettes into sprite palettes 0/2 (only into slots the displayed
  generation does not use) and each 8x8 tile picks the best loaded palette.
  The WHX reservation shrinks to 2 MB so the 408 KB atlas fits at `0x10280000`.
- 2026-10-03 (hardware/performance): HR-17 confirms good colours on hardware but
  lower fps with enemies. Cosimulation isolates the transport: 6.89 heartbeats per
  native picture versus 1.02 streamed; 55% of heartbeats are CHR uploads with only
  6% tile reuse. Next options, in expected payoff order: make tiles repeat
  (native-space, translation-invariant sampling and quantised scale bands); raise
  CHR per packet (five tiles via the PAL bytes, cycle budget permitting); cap new
  tiles per generation to trade native coverage for fps; overlap conversion with
  upload (needs a third 17 KB stream buffer, beyond current RAM headroom).
