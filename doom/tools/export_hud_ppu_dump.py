#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Export a captured native HUD as a NAW-importable 16 KiB PPU dump.

The streamed 3D scene cannot fit in one NES pattern table. This exports the
bottom 48 pixels at their original screen coordinates, plus the captured OAM.
Run with the Mesen files made by run_native_status_probe.py.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import zipfile

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fcpico import stream  # noqa: E402

HUD_TOP = 192
BG_RGB = {
    (0, 0, 0): 0,
    (102, 102, 102): 1,
    (255, 254, 255): 2,
    (181, 49, 32): 3,
}


def sprite_coverage(oam: bytes, chr_data: bytes) -> np.ndarray:
    """Return nontransparent sprite pixels, honoring the eight-sprite limit."""
    covered = np.zeros((240, 256), dtype=bool)
    for y in range(HUD_TOP, 240):
        selected = 0
        for slot in range(64):
            sy, tile, flags, x = oam[slot * 4:slot * 4 + 4]
            row = y - (sy + 1)
            if row < 0 or row >= 8:
                continue
            selected += 1
            if selected > 8:
                break
            if flags & 0x80:
                row = 7 - row
            low = chr_data[tile * 16 + row]
            high = chr_data[tile * 16 + row + 8]
            for dx in range(min(8, 256 - x)):
                bit = dx if flags & 0x40 else 7 - dx
                if ((low >> bit) | (high >> bit)) & 1:
                    covered[y, x + dx] = True
    return covered


def encode_tile(pixels: np.ndarray) -> bytes:
    out = bytearray(16)
    for row in range(8):
        for col in range(8):
            bit = 7 - col
            out[row] |= (int(pixels[row, col]) & 1) << bit
            out[row + 8] |= ((int(pixels[row, col]) >> 1) & 1) << bit
    return bytes(out)


def decode_tile(tile: bytes) -> np.ndarray:
    pixels = np.zeros((8, 8), dtype=np.uint8)
    for row in range(8):
        for col in range(8):
            bit = 7 - col
            pixels[row, col] = ((tile[row] >> bit) & 1) | (((tile[row + 8] >> bit) & 1) << 1)
    return pixels


def make_dump(capture: Path, frame: Path, mesen: Path, output: Path) -> None:
    screenshot = np.asarray(Image.open(capture).convert("RGB"))
    if screenshot.shape != (240, 256, 3):
        raise ValueError(f"expected 256x240 capture, got {screenshot.shape}")
    oam = (mesen / "oam.bin").read_bytes()
    sprites = (mesen / "sprite_chr.bin").read_bytes()
    bg_palette = (mesen / "palette.bin").read_bytes()
    sprite_palette = (mesen / "sprite_pal.bin").read_bytes()
    if (len(oam), len(sprites), len(bg_palette), len(sprite_palette)) != (256, 4096, 16, 16):
        raise ValueError("incomplete Mesen PPU capture")
    covered = sprite_coverage(oam, sprites)
    pixels = stream.decode_frame(frame.read_bytes()).copy()
    # Mesen has drawn the ROM's resident background tiles over some streamed
    # pixels. Reconstruct visible BG from its capture wherever no sprite masks it.
    for y in range(HUD_TOP, 240):
        for x in range(256):
            if not covered[y, x]:
                rgb = tuple(int(v) for v in screenshot[y, x])
                if rgb not in BG_RGB:
                    raise ValueError(f"unexpected non-sprite HUD color {rgb} at {(x, y)}")
                pixels[y, x] = BG_RGB[rgb]

    patterns = bytearray(4096)
    nametable = bytearray(1024)
    tile_ids = {bytes(16): 0}
    for ty in range(24, 30):
        for tx in range(32):
            tile = encode_tile(pixels[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
            if tile not in tile_ids:
                if len(tile_ids) >= 256:
                    raise ValueError("HUD needs more than 256 unique background tiles")
                tile_ids[tile] = len(tile_ids)
                start = tile_ids[tile] * 16
                patterns[start:start + 16] = tile
            nametable[ty * 32 + tx] = tile_ids[tile]
    # Every HUD 16x16 block uses subpalette 3. Everything above is blank.
    attributes = bytearray(64)
    for by in range(12, 15):
        for bx in range(16):
            stream.attr_set(attributes, bx, by, 3)
    nametable[960:] = attributes

    ppu = bytearray(16384)
    ppu[0x0000:0x1000] = patterns
    ppu[0x1000:0x2000] = sprites
    for address in (0x2000, 0x2400, 0x2800, 0x2C00):
        ppu[address:address + 1024] = nametable
    ppu[0x3000:0x3F00] = ppu[0x2000:0x2F00]
    for address in range(0x3F00, 0x4000, 32):
        ppu[address:address + 16] = bg_palette
        ppu[address + 16:address + 32] = sprite_palette

    # Verify the packed tilemap reconstructs each HUD pixel exactly.
    for ty in range(24, 30):
        for tx in range(32):
            number = nametable[ty * 32 + tx]
            tile = ppu[number * 16:number * 16 + 16]
            expected = pixels[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
            if not np.array_equal(decode_tile(tile), expected):
                raise AssertionError(f"tile round-trip failed at {(tx, ty)}")

    output.mkdir(parents=True, exist_ok=True)
    (output / "doom-hud.ppu").write_bytes(ppu)
    (output / "doom-hud.oam").write_bytes(oam)
    shutil.copyfile(capture, output / "full-frame-reference.png")
    Image.fromarray(screenshot[HUD_TOP:]).resize((1024, 192), Image.Resampling.NEAREST).save(
        output / "hud-reference-4x.png")
    (output / "README.md").write_text(
        "# Editable DOOM HUD capture for NAW\n\n"
        "Open `doom-hud.ppu` with NAW's **PPU Import**. This is a standard 16 KiB "
        "NES PPU memory dump: background CHR at $0000, sprite CHR at $1000, "
        "four nametables at $2000, and palettes at $3F00. Then import "
        "`doom-hud.oam` using NAW's **OAM Import** to show the face, weapon "
        "numbers, and key sprites. The HUD is at the original screen position, "
        "rows 192–239. Set background pattern table to $0000 and sprites to "
        "$1000, in 8x8 sprite mode if needed.\n\n"
        "`full-frame-reference.png` is the complete Mesen capture; "
        "`hud-reference-4x.png` is an enlarged pixel reference. The 3D view "
        "is blank in the PPU dump because the live streamed view needs more "
        "than one NES background pattern table. Editing this dump is a visual "
        "mockup; it does not automatically patch the firmware. Save your NAW "
        "project and send that or an exported PNG to show the desired changes.\n\n"
        "Recreate these files from the native status Mesen capture with:\n\n"
        "```sh\n"
        "doom/.venv/bin/python doom/tools/export_hud_ppu_dump.py \\\n"
        "  --capture /tmp/fcpico-native-hud-repro/mesen-status/mesen/run/final.png \\\n"
        "  --frame /tmp/fcpico-native-hud-repro/mesen-status/frame.bin \\\n"
        "  --mesen /tmp/fcpico-native-hud-repro/mesen-status/mesen/run \\\n"
        "  --output doom/assets/hud_edit_reference\n"
        "```\n"
    )
    archive = output / "doom-hud-naw-import.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        for name in ("doom-hud.ppu", "doom-hud.oam", "full-frame-reference.png",
                     "hud-reference-4x.png", "README.md"):
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            bundle.writestr(info, (output / name).read_bytes())
    print(f"Wrote {output / 'doom-hud.ppu'} with {len(tile_ids)} unique background tiles")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--frame", type=Path, required=True)
    parser.add_argument("--mesen", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    make_dump(args.capture, args.frame, args.mesen, args.output)


if __name__ == "__main__":
    main()
