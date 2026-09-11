# 04 -- Video pipeline

From RP2040 Doom's 320x200 palette-indexed frame to the bytes the PPU pulls off the bus.

## Output format (what the console can display)

- 256x240 background, 8x8 tiles, no scrolling, no sprites used.
- Per pixel: 2 bits selecting one of four entries of a *sub-palette*.
- Per 16x16 block: 2 bits selecting one of four sub-palettes (the attribute table, 64 bytes at
  `$23C0`; each byte covers a 32x32 area as four 2-bit quadrants).
- Sub-palette entry 0 is always the shared backdrop colour (`$3F00`); entries 1-3 are free.
  So at most **13 distinct colours** on screen, chosen from the PPU's 64 (about 54 unique).
- The pattern data itself is what `fcbus` streams (`docs/pages/graphics.md`,
  `rp_system::convVram()`): per scanline, 34 16-bit words, low byte = bitplane 0, high byte =
  bitplane 1, pixel 0 of each 8-pixel run in bit 7. Line 0 starts at word 31. Words 32-33 of a
  line hold the first 16 pixels of the *next* line (that is what `convVram()` reads there;
  they are the PPU's next-line prefetch). **Replicate the layout byte for byte; do not
  rationalise it.** `fcbus` owns the buffer and exposes `fcbus_stream_word(line, tile)`.

## Input (what RP2040 Doom produces)

See 01, "Engine facts". The 3D view is in `frame_buffer[display_frame_index]` (320x168, 8-bit
`PLAYPAL` indices); everything else -- status bar, menu, HUD messages, intermission and title
screens, the melt wipe, the ENDOOM text screen -- is composed **per scanline** by
`fill_scanlines()` in `src/pico/i_video.c` from "vpatch" overlay lists, writing 16-bit RGB into
`scanvideo` buffers. There is no full 320x200 8-bit frame anywhere.

## The pipeline

```
 RP2040 Doom                          fcvideo (core 1, LOW_PRIO_IRQ)                     fcbus
 frame_buffer[i] (320x168x8) --+
 vpatch overlays (menus, HUD)  +--> A: compose 8-bit line (320)  --> B: decimate 320->256 (nearest)
 wipe tables / single screens -+        ^                                     |
                                        |                                     v
 PLAYPAL + palette number ------> E: 14 NES palette sets           C: per-block palette cost
                                        |                                     |
                                        v                                     v
                                  mailbox: 16 palette bytes        D: dither + pack 2bpp  --> stream[back]
                                  mailbox: 64 attribute bytes <----+
```

### Stage A -- 8-bit composition (engine side, `src/fcpico/i_video_fcpico.c`)

Port `i_video.c` with these substitutions:

| Original | fcpico |
|----------|--------|
| `scanvideo_*`, `video_doom.pio`, timing structs | removed |
| `uint32_t *dest` scanline of `uint16_t` pixels | `uint8_t *dest8` of 320 palette indices |
| `palette_convert_scanline()` (8->16 via interp) | `memcpy` (or nothing: compose in place) |
| `draw_vpatch()` writing `palette[pal[v]]` | writes `pal[v]` (the index). `shared_pal[]` becomes `uint8_t`. The `stbar` XIP-streaming DMA optimisation is dropped (the RP2350 build already moves its cache to main RAM; measure first, re-add only if the status bar row costs > 0.3 ms). |
| `scanline_func_wipe` reading `palette[...]` | index copy |
| `render_text_mode_scanline` (80-column VGA text for ENDOOM) | not ported; the fcpico target sets `NO_USE_ENDDOOM=1`. `I_Quit` -> watchdog reboot (see 05). |
| `new_frame_stuff()` / `new_frame_init_overlays_palette_and_wipe()` | kept: frame flip, overlay list rebuild, wipe advance. The `palette[]` rebuild becomes "select NES palette set `next_pal`" (stage E). Must stay `__no_inline_not_in_flash_func` because it runs while flash is being programmed during saves. |
| `fill_scanlines()` driven by `scanvideo` buffer callbacks | `fcvideo_convert_frame()` driven by the `fcbus` heartbeat via `LOW_PRIO_IRQ` |
| `I_InitGraphics` launching `core1()` with `scanvideo_setup` | launches `core1()` that installs the bus ISR (`fcbus_attach_irq(core1)`), the low-priority conversion IRQ, then runs `pd_core1_loop()` forever |
| `interp_in_use` save/restore | kept; the converter does not use the interpolators, but `draw_vpatch`/wipe code paths are the same as before |

Rules inherited from the original that must survive: no `malloc` on core 1 after init; the
`render_frame_ready`/`display_frame_freed` semaphores define frame ownership; during
`VIDEO_TYPE_SAVING` do not touch flash-resident code from the IRQ.

Output of stage A: one 320-byte line at a time (`line8[2]`, double-buffered so stage B can
overlap), for lines 0..199, plus the current `next_pal`.

### Stage B -- horizontal decimation 320 -> 256

Nearest-neighbour: keep source columns `x` where `x % 5 != 4` (drop every fifth). A 256-entry
`src_x[256]` table is the whole implementation. Vertical: 200 lines placed at console lines
16..215 (top letterbox 16 lines so the Doom frame starts on an attribute-block boundary; bottom
letterbox 24 lines). Aspect: the result is ~10% wider than Doom's intended 4:3; acceptable for
M1, revisit in Phase 5 (options: box filter in RGB space before quantisation; 200->219 line
duplication).

### Stage C -- sub-palette selection per 16x16 block

16 block columns x 13 block rows cover the Doom frame (the 13th row is half height). For each
block and each sub-palette `p` (0..3), cost = sum over the block's pixels of
`err[p][idx]`, where `err` is a precomputed `uint8_t[4][256]`: the residual error of the best
*dither pair* for Doom colour `idx` under sub-palette `p` (stage D's LUT is built from the same
search, so C and D agree). Choose the minimum; apply **hysteresis**: keep the previous frame's
choice unless another palette is cheaper by more than `HYST_PCT` (default 12%). Write the 2-bit
choice into the attribute shadow (`fcbus_attr_set(bx, by, p)` -- same packing as
`rp_system::setAtr()`), and set `ATTR_VALID` in the back mailbox.

Cost: 208 blocks x 256 px x 4 palettes = 212,992 byte loads and adds (the Doom frame covers 16 x 13 of the screen's 16 x 15 blocks). Vectorise trivially by
accumulating four `uint16_t` sums per pixel from a `uint32_t err4[256]` table (one load, four
byte-lane adds). Estimated 1.5-2.5 ms at 150 MHz.

Phase 1 (M1) shortcut: skip stage C, attribute table all zeros (sub-palette 0 everywhere),
greyscale ramp. This is what runs with the v1 boot ROM.

### Stage D -- ordered dither and bitplane packing

For each 8-pixel run: `v = lut[p][idx][bayer4x4[(y & 3) * 4 + (x & 3)]]` gives a 2-bit value;
assemble `plane0 |= (v & 1) << (7 - i)`, `plane1 |= (v >> 1) << (7 - i)`; store the 16-bit word
at `fcbus_stream_word(line, tile)`. `lut` is `uint8_t[4][256][16]` = 16 KB (each entry
precomputed as: for Doom colour `idx` under sub-palette `p`, the two palette entries `a`,`b`
and mix ratio `r/16` that best approximate it; the 16 Bayer thresholds turn `r` into a pattern).
A 4 KB variant with a 2x2 pattern (5 levels) is a compile-time option.

Cost: 51,200 pixel lookups (256 x 200) plus packing; estimated 1-2 ms. Letterbox lines need none.

The letterbox lines and the prefetch words are written once at init (all zeros = backdrop).

### Stage E -- NES palette sets

At init, for each of Doom's 14 `PLAYPAL` palettes (or the RP2350 build's synthesised tints:
red x9 for damage, yellow x4 for pickups, green x1 for the radiation suit -- see
`new_frame_init_overlays_palette_and_wipe()`), compute the 13 NES colour indices: take the 13
anchor RGB values of the chosen sub-palette preset, apply the tint the same way the original
applies it to `PLAYPAL`, and pick the nearest NES colour from a 64-entry NTSC RGB table
(`tools/nes_palette.py` emits it from a standard 2C02 palette; cite the source in the file).
Result: `uint8_t nes_pal_sets[14][16]` (backdrop, 3 colours, then 3 mirrored backdrops per
group as the PPU expects: entries 4, 8, 12 are written with the backdrop value).

When `next_pal` changes, copy the set into the back mailbox's palette block and set
`PAL_VALID`. The per-pixel LUT is **not** rebuilt (it is built for palette 0); tinting is done
by the console's palette, which is exactly how the NES does screen flashes.

Sub-palette presets (`fcvideo_presets.h`), selectable at build time, evaluated in P2-T6 by
PSNR against the 8-bit reference frames and by a legibility check of the menu font:

| Preset | P0 | P1 | P2 | P3 | Notes |
|--------|----|----|----|----|-------|
| A "hue" | `$00 $10 $30` greys | `$07 $17 $27` browns | `$06 $16 $26` reds | `$09 $19 $29` greens | Max variety; text may land on a palette without a light colour |
| B "shared white" | `$00 $10 $30` | `$07 $17 $30` | `$06 $16 $30` | `$09 $19 $30` | Every block can show white: menus, HUD digits and lighting highlights always readable; one fewer shade per hue |

Backdrop `$0F` (black) in both. Values are NES palette indices; hue numbers (`$x7` orange-brown,
`$x6` red, `$x9` green, `$x0` grey) chosen to match Doom's dominant materials. The k-means
derivation tool in P2-T6 may replace these with data-driven picks.

### What NES DOOM did (PiPU `frameprocess.c`), and what to copy

- Palette set (in `S_ChangeMusic` it is sent per level together with the track):
  P0 `$10 $09 $2D` (grey, dark green, dark grey), P1 `$07 $28 $18` (browns/orange), P2
  `$02 $01 $11` (blues), P3 `$06 $16 $3D` (reds + light grey), backdrop `$0F`. Add this as
  preset C in `fcvideo_presets.h`; it is the only set known to have been judged on a real TV.
- Pre-processing before matching: brightness +30 (saturating), contrast factor
  `259*(50+255)/(255*(259-50))`, then an 8x8 Bayer offset (+/-30) added to R, G and B, then
  nearest colour by a CCIR-601-weighted distance (`0.75 * weighted RGB^2 + luma^2`). We do the
  equivalent offline when building `err`/`lut` (the dither pattern is baked into the LUT),
  so the per-pixel cost stays one lookup.
- Palette choice per 8x1 slice by majority vote of the per-pixel best palette
  (`FindBestPalForSlice`). Ours is per 16x16 block and cost-based; the majority vote is a
  cheaper alternative if stage C proves too slow (256 lookups + a 4-way argmax per block).
- Two lookup tables over the full 24-bit RGB space (17 MB) traded memory for speed on the Pi;
  on the RP2350 the 256-entry Doom palette makes the tables 1-16 KB.

## Frame pacing and buffer handoff

- The heartbeat (`fcbus` ISR) pends `LOW_PRIO_IRQ` every console frame (60.1 Hz).
- `fcvideo_convert_frame()`: if `sem_available(&render_frame_ready)` -> acquire, flip
  `display_frame_index`, run stages A-D into `stream[back]`, write attributes/palette into
  `mailbox[back]`, call `fcbus_publish(back)`, `sem_release(&display_frame_freed)`. Else return.
- `fcbus_publish()` only records "next display buffer"; the ISR performs the swap at the next
  heartbeat, so a conversion that overruns one console frame simply publishes one frame later.
- Doom's renderer may run up to two frames ahead (`display_frame_freed` initialised to 2 in
  `I_InitGraphics`); leave it.
- Conversion must finish within one console frame (16.6 ms) to sustain 30 fps with margin;
  the estimate is 3-6 ms. P1-T8 measures it with `time_us_32()` and the serial CLI prints
  min/avg/max per second.

## Per-frame latency budget

| Stage | Where | Time |
|-------|-------|------|
| Doom render | core 0 + core 1 | 25-50 ms (20-40 fps, level dependent) |
| Wait for heartbeat | -- | 0-16.6 ms |
| Convert | core 1 IRQ | 3-6 ms (estimate) |
| Wait for next heartbeat, stream | -- | 16.6 ms + 16.6 ms |
| Console NMI applies attributes/palette | 6502 | same frame as the stream |

Attribute/palette updates ride in the mailbox of the *same* stream they belong to, and the
console applies them during the vblank that follows that stream, so a picture and its
attributes are always displayed together (the mailbox is read after the picture is fetched,
and applied before the next picture starts). Verify this in co-simulation (a test pattern whose
block palettes alternate every frame must never show a mismatched frame).

## Host build

`fcvideo` compiles for the host with no pico dependencies (`FCVIDEO_HOST=1`): stage A is
exercised through the engine's host build; stages B-E are pure functions over byte arrays and
have direct unit tests with synthetic inputs (gradients, checkerboards, a captured 320x200
frame from `chocolate-doom -timedemo`).

## Acceptance criteria (used by 10-workplan)

- Stages B-E unit-tested: decode(stream, attrs, palette) via `tools/ppu_decode.py` yields an
  image whose PSNR against the decimated 8-bit source, both mapped to RGB, exceeds a stored
  threshold per golden (thresholds are set once from the first accepted output and only ever
  raised).
- Attribute hysteresis: a static scene produces identical attribute tables on consecutive
  frames.
- Conversion time on device <= 8 ms worst case in E1M1 (serial CLI report).
- No `malloc` from core 1 (the wrapper `hard_assert`s already).
