# Live yellow-plate HUD integration

These are full-frame Mesen screenshots of the v4 ROM and the C host panel
renderer, captured with the hardware eight-sprite-per-scanline limit enabled.
They are not the earlier NROM playback mockups.

| Capture | Health | Armor | Selected weapon | Keys | Face |
|---|---:|---:|---:|---|---|
| `healthy.png` | 100% | 75% | 3 | blue | STFST00 |
| `hurt.png` | 67% | 46% | 4 | blue | damage |
| `pickup.png` | 23% | 12% | 6 | blue/yellow/red | pickup |

The host panel uses `full-frame-reference-edited.png` for concrete, borders,
labels, and the approved red value glyphs. `build_live_hud_art.py` regenerates
the packed C header; the missing large glyphs come from the original DOOM
`STTNUM` patches. The ROM renders the selected ARMS number as a yellow plate
sprite and uses one keycard shape with blue, yellow, and red palette indices.
The fixed v4 background row moved to scanlines 232–239 so the edited labels
and CELL totals on scanlines 224–231 are streamed every frame. Frame count and
mailbox offset remain 15,122 and 14,978 bytes respectively.

The full device UF2 is built by `doom/tools/build_bg_text_probe_device.sh
--native-status`. Physical NES output remains to be checked with this revision.
