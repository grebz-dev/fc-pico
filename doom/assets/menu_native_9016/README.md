# Centered episode and mouse-free options menus (9016)

All three PNGs are 256×240 Mesen captures with the eight-sprite scanline
limit enabled. `episode.png` and `options.png` use the deterministic checker
frame; `episode-host-frame.png` places the episode text over a previously
captured Doom host frame. The OAM episode label slots are hidden; the cursor
is the only episode-menu text-related sprite.

The full names are one line each, centered at NES y=88, 120, and 152 using
the same permanent-bank bitmap font installed for the native menu labels.
Their red letters and offset black shadows are written directly into the
v4 background stream. The text area takes background palette 3 in 16×16
attribute cells, so the world pixels under those cells remain visible with
a temporarily changed color mapping. This is visible as gray/red bands on the
checker fixture. The settings menu follows the compiled mouse-free Doom
menu: `END GAME`, `MESSAGE`, `SOUND`; row 2 activates Sound.

Recreate both captures with `doom/tools/reproduce_native_hud.sh`. The
`20DOOM-04-9016` merged UF2 produced by the two-build run has SHA-256
`44228ccb98282e2f35decfea7123cdd35e8e7044910cea258f5b4037d73e6667`.
This revision has Mesen and build validation only; physical NES validation
remains pending.
