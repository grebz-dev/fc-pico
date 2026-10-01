# HUD playback candidates (Mesen)

These are three **NES mapper-0 playback ROMs** rendered and captured by Mesen,
with the hardware sprite limit enabled. Each ROM starts in a healthy state,
changes to a hurt state at frame 60, then to a pickup state at frame 120. Lua
captures the settled screens at frames **30, 90, and 150**. The changing
background, sprites, and OAM come from ROM code during emulation; the PNGs are
Mesen screenshots, not host image composites.

Run from the repository root:

```sh
doom/.venv/bin/python doom/tools/prototypes/hud_playback_candidates.py
```

`comparison.png` lays out all nine HUD crops. Each style directory includes its
three 256x240 `frame-XX.png` Mesen captures, enlarged `hud-XX-4x.png`, ROM,
assembly, CHR, three nametables, three OAM pages, palettes, and metrics. The
`ppu-XX.txt` dumps record the ROM's state counter and PPU state at capture.

| Candidate | Selected ARMS treatment | Healthy | Hurt | Pickup |
|---|---|---|---|---|
| `digit` | Yellow selected numeral; white unselected numerals | `digit/frame-01.png` | `digit/frame-02.png` | `digit/frame-03.png` |
| `plate` | Yellow inset with a black cutout numeral | `plate/frame-01.png` | `plate/frame-02.png` | `plate/frame-03.png` |
| `border` | Yellow inset outline around a white numeral | `border/frame-01.png` | `border/frame-02.png` | `border/frame-03.png` |

The three key graphics use exactly the same 8x8 pixel mask and differ only in
their blue, yellow, or red sprite palette entry. The editable source preview is
`keys-same-shape.png`. The selected ARMS masks use one consistent five-pixel
wide number font and a 9x10 well. `numeric-font.png` exposes all ten digits,
percent and slash in the candidate palettes. The generated background CHR also
reserves tile patterns for every one of these characters, including digits not
visible in the three sampled states. All six selected-weapon sprite numeral
masks (2–7) are present.

| Captured state | AMMO | HEALTH | ARMOR | Selected ARMS | Face | Keys | BULL | SHEL | RCKT | CELL |
|---|---:|---:|---:|---:|---|---|---|---|---|---|
| Healthy, frame 30 | 20 | 100% | 75% | 2 | `STFST00` | blue/yellow/red | 20/200 | 18/50 | 4/50 | 100/300 |
| Hurt, frame 90 | 7 | 67% | 46% | 3 | `STFOUCH2` | blue | 63/200 | 7/50 | 5/50 | 40/300 |
| Pickup, frame 150 | 124 | 23% | 12% | 6 | `STFEVL3` | blue/yellow | 153/400 | 0/100 | 18/100 | 124/600 |

The ready ammo matches its ammunition type in each state. The face artwork is
from the user's `doomguy_faces.png`, stretched per cell from 24x24 to 32x40
with nearest-neighbor sampling. The face remains 20 8x8 sprites. Yellow ARMS
selection is one sprite, and acquired keys are one sprite each. OAM use is
24/22/23 across the three scenes, with a measured peak of **six sprites per
visible scanline**. The generated BG atlas uses 232 tiles for `digit` and
`plate`, or 227 for `border`; the sprite atlases use 66/66/61 tiles. All are
within one 4 KiB background and one 4 KiB sprite pattern table.

`digit` keeps the most of the user's original gray/black boxed ARMS grid and
has the least visual noise. `plate` is the strongest highlight. `border` keeps
the selected numeral white and marks its box yellow. All three keep the same
uniform key silhouette, red values, white labels and tall warm face.

This playback ROM tests three prebuilt states; it does not run DOOM or the
fc-pico video stream. The source face sheet has 42 expressions. At 32x40, these
contain **576 unique 8x8 tiles** before flip-aware packing, so all expressions
cannot stay in one 256-tile NES sprite bank. Production animation needs a
resident subset or safe CHR updates. The current fc-pico v4 stream also omits
background y224..231, columns2..29. Native changing values crossing that row
need the resident-tile update path or a transport revision. World palette
impact, menu coexistence, and physical output remain integration checks.
