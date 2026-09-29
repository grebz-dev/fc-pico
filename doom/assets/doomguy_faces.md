# Doomguy native face sheet

The [24×24 sprite sheet](doomguy_faces.png) contains all 42 Doom status-face
patches from the local `doom1.whx`, in the order recorded by
[the manifest](doomguy_faces.json). Each face is divided into a 3×3 grid of
8×8 NES sprites. [The CHR file](doomguy_faces.chr) stores nine 16-byte tiles
per face, left to right and top to bottom; the [4× preview](doomguy_faces_preview.png)
shows the quantized art at a readable size. Transparent pixels use color zero.

The source patches are 24×29 or 24×31. The generator scales all rows into
24×24 and assigns opaque pixels to one shared three-color palette. Run
`doom/.venv/bin/python doom/tools/build_doomguy_faces.py --output doom/assets`
to reproduce the sheet from the local WHX.

All 42 faces need 378 tiles, beyond the 128-tile region available beside the
system font. The earlier 9012 native status probe loaded eleven representative faces: three
healthy idle, four damage, god, dead, ouch and grin (99 tiles). It maps the
remaining face indices to a resident expression until face-art paging is
added. The 24 tall number tiles plus 99 face tiles occupy 123 of the 128
available slots. The face uses three sprites on each of its 24 scanlines.
Its lower panel niche places it away from the health/armor rows; the weapon
and key rows peak at eight sprites per scanline.

The 9013 candidate uses [the editable 4×4 face sheet](doomguy_faces_large_edit.png)
generated directly from the WHX patches. It keeps ten resident expressions,
each 28×32 within a 32×32 black panel square. Flip-aware deduplication packs
160 face tile placements into 119 CHR tiles. The most damaged ordinary state
reuses the previous damage expression to fit. Face tiles use four sprites per
scanline; the ARMS grid and vertical keys bring the status peak to eight.
Edit with `edit_sprite_sheets.py import large_faces`; see [EDITING.md](EDITING.md).

Edit [`doomguy_faces_edit.png`](doomguy_faces_edit.png) at its native 168×144
size to change any of the 42 faces. Keep the existing four-color palette and
transparent pixels. Run
`doom/.venv/bin/python doom/tools/edit_sprite_sheets.py import faces` to
update the CHR file. Running `build_doomguy_faces.py` again restores WHX-derived
art and overwrites manual CHR edits.
