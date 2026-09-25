<!-- SPDX-License-Identifier: BSD-3-Clause -->
# P3-U1 sprite UI feasibility audit

Status: **in progress, source audit only** (2026-09-25). This is working evidence
for [plan 12](12-sprite-ui.md), not an approved layout or transport. No sprite
UI build, emulator probe or NES-001 test has been run for this audit.

## Baseline

| Component | Revision or condition |
|-----------|-----------------------|
| Main repository | `1a395b5c245d50d9f837061c26b95f1e96c9102b` plus uncommitted planning changes |
| Doom engine submodule | `3c8af2bbdc4aa6ed424689d514b72c27db02ac5f`; clean at audit time |
| Mesen2 submodule | `9688726c982c57cfbcb81bc6d519decfe82eff91` |
| Existing evidence | D1 reports 1614 critical / 2021 total NMI cycles in [PROGRESS](../PROGRESS.md); these are v2 measurements, not UI results |

The active `doom_tiny_fcpico` target sets `DOOM_TINY=1`, `USE_WHD=1`,
`USE_FPS=1`, `NO_USE_ENDDOOM=1`, `USE_PICO_NET=0` and
`FCPICO_AUTO_SAVENAME=1` in `rp2040-doom/src/CMakeLists.txt`. Menu, HUD,
status, save and load drawing are enabled by that configuration.

## Foreground producer inventory

| Producer | Information or patches to account for | Migration seam / qualification |
|----------|---------------------------------------|--------------------------------|
| `st_stuff.c`: `ST_drawWidgets`, `ST_createWidgets` | Ready ammo; health and armor plus percent signs; six weapon ownership indicators; three key slots (cards or skulls); four current and four maximum ammo counts; deathmatch frags; face; optional FPS | `ST_updateFaceWidget` already selects the face. Capture its resolved index, not a new face timer. `DOOM_TINY` draws `STBAR` every frame at `(0,168)` in 320x200 coordinates; its baked labels must also be replaced or cleared. |
| `st_stuff.c`: `ST_loadUnloadGraphics` | `STTNUM`, `STYSNUM`, `STTPRCNT`, `STKEYS`, `STARMS`, `STGNUM`, `STBAR`, `STFB` and 42 face IDs: five pain levels times eight states, plus god and dead | Record exact source identity and selected face ID in the snapshot. The optional colored face background is conditional. |
| `hu_stuff.c`, `hu_lib.c`: `HU_Drawer`, `HUlib_drawTextLine` | One message line, chat/input cursor, automap map title; 63 characters in the `STCFN033` through `STCFN095` range; message storage permits 80 characters | Text is converted to uppercase and variable-width patches; a space advances four source pixels. Capture bounded final strings and visibility, including color/translation if used. Long lines need a deliberate wrapping or paging policy. |
| `m_menu.c`: `M_Drawer`, menu routines | Patch labels and title, animated skull cursor, options values, sound sliders, prompts, save/load names and underscore cursor | Menu rows are 16 source pixels apart. Save/load has six rows and up to 24 eight-pixel border segments per row; borders can remain crisp background decoration only if their text/selection stays readable and synchronized. |
| `wi_stuff.c` | Intermission map names, labels, numbers, percent/time and player totals | Separate foreground values from decorative background and animation patches. |
| `f_finale.c` | Finale text and foreground end graphics | Inventory the active shareware path before suppressing any patch; full-screen artwork can remain background. |
| `am_map.c` | Automap marks drawn as patches; map title comes from `HU_Drawer` | Classify marks by purpose; do not treat every patch as text. |
| `i_video_fcpico.c`: `compose_frame` | Merges base line and `vpatchlists` overlays into 320-pixel indexed rows before `fcvideo_line_sink` | Patch identity disappears here. Capture semantic UI and classified draws before composition, then pair it with the matching frame at `fcvideo_frame_end`. |

The inventory is intentionally source based. It does not yet claim that every
visible patch on every screen is classified. Title art, help screens, wipe and
intermission backgrounds remain candidates for the streamed background.

## Capacity facts and immediate consequences

| Limit or measurement | Calculation | Consequence |
|----------------------|-------------|-------------|
| OAM | 64 entries × 4 bytes = 256 bytes | Full replacement OAM costs 256 transfer bytes plus update work; a proposed `$0200-$02FF` shadow still needs a fixed-bank RAM audit. |
| Scanline | 8 sprites × 8 pixels = at most 64 horizontal sprite pixels | A full 256-pixel menu row, 80-character HUD message or original wide status row cannot be a single sprite text row. Packed narrow glyphs help tile use but cannot raise this limit. |
| 16×16 face candidate | 4 tiles/entries and 2 slots on each of 16 lines | On a shared line, only six slots remain for numbers, labels and icons. A 24×24 face consumes 9 entries and 3 slots on each of 24 lines. |
| Pattern table if independent | `$1000-$1FFF` = 4096 bytes = 256 2bpp 8×8 tiles | This is an upper bound, not an allocation: mirroring, font ownership, physical writes and reset behavior are unproven. The calibrated cartridge stream selects `$0800-$0FFF`. |
| V2 mailbox | 128 bytes: 16 command, 32 APU, 16 BG palette, 64 attributes | No spare UI or sprite palette field can be assumed. A successor version needs its own fixed length and count. |
| Existing NMI headroom | 1900 − 1614 = 286 critical; 2200 − 2021 = 179 total cycles | Adding a 513/514-cycle OAM DMA to the measured worst case would exceed the limits by at least 227 critical and 334 total cycles, before new transfer or palette work. |

The existing game status coordinates are 320×200 (`STBAR` starts at y=168);
the native display is 256×240 and the current converted world occupies native
lines 8..231. A proposed native status region at y=208..239 therefore requires
revisiting world/status composition, not just mapping x coordinates. No native
positions are approved yet.

## Candidate design work still required to close U1

1. Extract actual shareware patch widths/heights and representative text from
   the active WHX, then draw native layouts for play, automap, messages, each
   menu family, prompts, save/load, intermission and finale. Record the 240-line
   occupancy array and total OAM count for worst cases. Require every line to
   stay at or below eight sprites and every frame at or below 64 entries.
2. Decide a legible long-text/menu policy. At least the original full-width
   rows exceed the sprite coverage bound. If a foreground background-text
   exception is needed, write the exact scope decision before implementation.
3. Audit `$0200-$02FF`, sprite pattern aliases and system-font occupancy in
   the permanent ROM setup and physical board. Specify U2's known glyph/icon/
   face upload, OAM and palette probe without changing the calibrated CS1 mask.
4. Compare a compact descriptor with dirty OAM changes and full OAM transfer.
   For each, table fixed read bytes, worst-case replacement bytes, decoder cycles,
   both DMA parities, all palette/attribute flags and 15 APU pairs. A design
   that cannot meet the measured 1900/2200-cycle limits must use a measured
   deferred schedule and atomic generation commit.
5. Choose the minimal immutable engine snapshot after checking each producer;
   include frame/generation, mode, visibility, resolved face, bounded values
   and strings, menu selection and palette/transition state. No overlay pointers
   may cross the frame handoff.

U2 and later implementation remain gated on these results and the physical
pattern-storage probe specified by [plan 12](12-sprite-ui.md).
