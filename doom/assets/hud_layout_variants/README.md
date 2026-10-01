# Native HUD layout evaluation — Mesen captures

Generated 2026-09-30 from `doom/tools/prototypes/hud_reference_variants.py`.
These are **static NES mapper-0 ROM fixtures captured by Mesen at frame 30**.
They exercise normal background attributes, palette RAM, CHR, OAM and sprite
evaluation. Mesen's `RemoveSpriteLimit` is `false`. They are visual prototypes;
they do not run the fc-pico stream, DOOM status updates, animation or RP2350.

To regenerate from the repository root:

```sh
doom/.venv/bin/python doom/tools/prototypes/hud_reference_variants.py
```

The script uses the user's edited `doom/assets/hud_edit_reference/full-frame-reference-edited.png`
and the source `doom/assets/doomguy_faces.png`. It places the first face,
`STFST00`, at x112..143, y196..235. Each 24x24 face cell is enlarged to 32x40
with nearest-neighbor sampling. `faces-32x40-normalized.png` shows all 42
normalized states, while `faces-32x40-indexed.bin` holds its 224x240 row-major
2-bit indices in unpacked bytes. `faces-32x40.json` gives state order and palette.

## Captures

| Candidate | Allocation | OAM / peak scanline | BG / sprite CHR tiles | Key visual cost |
|---|---|---:|---:|---|
| A | 8x8 sprite face, colored ARMS and keys; one HUD BG palette | 29 / 8 | 142 / 30 | Closest to the edited compartment layout; peak exactly fills the NES scanline budget |
| B | 8x16 sprite face, ARMS and keys; one HUD BG palette | 18 / 8 | 142 / 74 | Bottom ARMS number row moves up four pixels to pair with the top row; the global sprite size also affects menus |
| C | Warm face in BG palette 1, colored ARMS and keys as 8x8 sprites | 9 / 4 | 161 / 10 | Face reserves two 16-pixel attribute columns for all six HUD tile rows; world has two independently chosen BG palettes left |
| D | All HUD art in BG: panel, face, weapon grid and colored key column | 0 / 0 | 160 / 1 | Key region widens to a full attribute column and obscures part of the armor section; four BG palettes are committed to HUD regions |

Palette sets (four entries each, first entry is the shared backdrop): panel
`0F 00 30 16` = black/gray/white/red; face `0F 06 17 26` =
black/dark red/brown/skin; ARMS `0F 00 30 38` = black/gray/white/yellow;
keys `0F 12 38 16` = black/blue/yellow/red. A/B use one BG palette for the
HUD and three independently choosable world palettes. C uses two and leaves
two. D uses all four, although the world can reuse any of their colors.
Sprite palettes are separate from the four BG palettes.

`comparison.png` contains the edited target, previous Mesen image and all four
new Mesen HUD crops. The original full-resolution DOOM reference remains at
`doom/StatusBar.png`. `target-annotated.png` marks the proposed section edges
and 16x16 palette attribute grid. Each `A`–`D` directory contains the real
256x240 `mesen.png`, pixel-sharp `hud-4x.png`, runnable `fixture.nes`, assembly,
CHR, nametable, OAM, palette and measured `metrics.json`.

## Evaluation

A matches the intended border, face recess, red percentages, small white
labels and stacked inventory most closely while preserving three world
background palettes. The source face is visibly more orange than the edited
mockup because the four source colors map to the chosen legal NES face palette.
C looks almost the same at a glance and frees twenty face OAM entries, so it is
the best background-face direction if the lost world palette is acceptable.
B tests 8x16 honestly: it saves OAM entries, yet its eight-per-line maximum
remains eight. D demonstrates the cost of giving the face, weapon selection
and keys independent BG color choices; its yellow-outlined key band is wider
than the target and the armor edge needs a redraw.

The `RCKT 4/0` and other inventory values are copied from the user's artwork
for these fixed fixtures. Gameplay must continue to take both current and
maximum amounts from DOOM. The fixtures do not demonstrate changing numbers,
face animation, menus, or a moving world. They also do not verify physical
overscan or palette appearance on the user's display.

## Transport work before production

The current v4 fc-pico stream omits 28 background tiles in y224..231,
columns 2..29; all candidates have graphics in that row. A/B can use the
existing static resident row for fixed art but must update any changing
numbers or keys there through a revised native tile mechanism or a transport
change. C/D also need generation-matched background face updates. The
existing converter, packet, and Mesen fc-pico probe have not been changed by
these static fixtures. A production implementation must measure the 15122-read
schedule, CHR residency for *all* face states, atomic updates, and pause/menu
behavior. Candidate B additionally requires repacking every menu sprite for
the global 8x16 PPU mode.

For visual iteration, inspect `comparison.png` and the four original-resolution
Mesen images. A is the lowest-risk match to the provided layout; C is the
most useful next integration experiment if freeing OAM is the priority.
