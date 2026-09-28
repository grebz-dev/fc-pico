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
system font. The native status probe loads seven representative faces: five
straight faces, god, and dead (63 tiles). It maps the other 35 face indices
to the matching pain-level representative until face-art paging is added.
The 24 tall number tiles plus 63 face tiles occupy 87 of those 128 slots.
The face uses three sprites on each of its 24 scanlines; the status layout
keeps the total at or below eight on every scanline.
