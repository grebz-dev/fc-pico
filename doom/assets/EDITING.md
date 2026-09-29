# Editing native UI sprite sheets

The editable PNGs are the exact native pixel grids used by the CHR importer:

| Sheet | Size | Tile layout | ROM use |
|---|---:|---|---|
| `doom_menu_logo_edit.png` | 64×32 | 8×4 | Title logo |
| `doom_menu_logo_small_edit.png` | 56×24 | 7×3 | Paused main-menu logo |
| `doomguy_faces_edit.png` | 168×144 | 7×6 faces, each 3×3 tiles | Face source sheet |
| `doomguy_faces_large_edit.png` | 128×96 | 10 resident faces, each 4×4 tiles | Current 9013 status face |
| `doom_episode_pairs_edit.png` | 64×24 | 8×3 tile cells | Episode letters |

Use an RGBA PNG pixel editor at 100% image size, with nearest-neighbor zoom.
Transparent pixels and the three opaque colors already in each PNG are the
only accepted colors. Avoid brush opacity, antialiasing, resizing, and color
profile conversion. The importer reports the first off-palette pixel with its
coordinates. GIMP or Aseprite can edit these PNGs; neither is installed in
this workspace. The existing `doom/.venv` supplies Python, Pillow and NumPy.

After editing, from the repository root run:

```sh
doom/.venv/bin/python doom/tools/edit_sprite_sheets.py import pause_logo
bash doom/tools/preview_native_ui.sh pause
```

The sheet names are `logo`, `pause_logo`, `faces`, `large_faces`, and `paired_text`. Omit the
name to import all sheets. The importer writes the `.chr` files consumed by
the native UI ROM builder. The preview command writes a Mesen screenshot and
ROM under `/tmp/fcpico-native-ui-pause/mesen/`; add `--gui` to open that fixed
test scene in the bundled Mesen. Other scenes are `title`, `episode`,
`difficulty`, `options`, `status`, and `stale`. The GUI scene repeats a captured
frame; controls do not navigate Doom menus. Re-run the preview command with a
different scene or edited sheet to compare layouts. The `difficulty` scene
builds the host engine and captures Doom's actual styled difficulty patches,
then passes that frame through the v4 Mesen probe. It needs the Pico SDK at
`$PICO_SDK_PATH` or `~/.local/fcpico/pico-sdk`; it does not build a UF2.
Set `FCPICO_UI_HOST_FRAME` to reuse a different captured v2 stream.

The `export` operation recreates the editable PNGs from `.chr` and overwrites
PNG edits. The WHX source generators also overwrite their derived `.chr`
files. Keep a copy of manual work before running either operation. Sprite
palette entries are shared within each sheet; a fourth opaque color cannot
be displayed by one NES sprite palette. The importer preserves exact pixels,
while Mesen shows the NES palette colors.

For the current 4×4 status face, edit `doomguy_faces_large_edit.png` and run
`doom/.venv/bin/python doom/tools/edit_sprite_sheets.py import large_faces`.
The importer deduplicates equal or flipped 8×8 tiles and rejects edits that
need more than 128 resident tiles. Running `build_large_face_art.py` overwrites
the edited PNG and its atlas. The 24×24 `faces` sheet remains a separate older
source asset and does not drive the 9013 resident face.
