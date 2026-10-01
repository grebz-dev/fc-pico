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

The `20DOOM-04-9015` follow-up restores all four OAM fields after a menu
closes, so the face grid and selected yellow ARMS plate reappear in their
gameplay positions. It also uses the 64×32 blue-and-yellow logo in the paused
menu. The [menu-close trace](menu_close_trace/README.md) includes before,
open-menu, and restored Mesen captures plus OAM and bus traces.

From the repository root, run `doom/tools/reproduce_native_hud.sh
--verify-twice` to regenerate the panel, run Python and C tests, capture
status/title/menu-close scenes in Mesen, build the merged device image twice,
compare ROM and UF2 bytes, and write `manifest.json` under
`/tmp/fcpico-native-hud-repro`. The verified 9015 ROM SHA-256 is
`d85f4dd9ebd429e687e92a381b1302f07c31f6cc01bb3e66ab2c9feab6509ad2`;
the UF2 SHA-256 is
`ee303541dbbbc89d1b644e3105a86f027035bdb3b8b74d027249ae1b9cb248ba`.
The flashable file is
`/tmp/fcpico-native-hud-repro-9015/device-first/artifacts/fcpico_doom_native_status_whx.uf2`.
Physical NES output remains to be checked for this revision.
