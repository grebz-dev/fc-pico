# Editable DOOM HUD capture for NAW

Open `doom-hud.ppu` with NAW's **PPU Import**. This is a standard 16 KiB NES PPU memory dump: background CHR at $0000, sprite CHR at $1000, four nametables at $2000, and palettes at $3F00. Then import `doom-hud.oam` using NAW's **OAM Import** to show the face, weapon numbers, and key sprites. The HUD is at the original screen position, rows 192–239. Set background pattern table to $0000 and sprites to $1000, in 8x8 sprite mode if needed.

`full-frame-reference.png` is the complete Mesen capture; `hud-reference-4x.png` is an enlarged pixel reference. The 3D view is blank in the PPU dump because the live streamed view needs more than one NES background pattern table. Editing this dump is a visual mockup; it does not automatically patch the firmware. Save your NAW project and send that or an exported PNG to show the desired changes.

Recreate these files from the native status Mesen capture with:

```sh
doom/.venv/bin/python doom/tools/export_hud_ppu_dump.py \
  --capture /tmp/fcpico-native-hud-repro/mesen-status/mesen/run/final.png \
  --frame /tmp/fcpico-native-hud-repro/mesen-status/frame.bin \
  --mesen /tmp/fcpico-native-hud-repro/mesen-status/mesen/run \
  --output doom/assets/hud_edit_reference
```
