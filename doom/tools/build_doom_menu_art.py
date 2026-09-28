#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Derive a compact sprite Doom logo and paired-glyph episode labels."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image

from build_doomguy_faces import ROOT, WHDATA, Whx

LOGO_SIZE = (64, 24)
EPISODE_LINES = (
    (("KNEE DEEP IN", "THE DEAD"), ("SHORES OF HELL",), ("INFERNO",)),
)
FONT = {
    " ": ("...",) * 5,
    "A": (".#.", "#.#", "###", "#.#", "#.#"),
    "D": ("##.", "#.#", "#.#", "#.#", "##."),
    "E": ("###", "#..", "##.", "#..", "###"),
    "F": ("###", "#..", "##.", "#..", "#.."),
    "H": ("#.#", "#.#", "###", "#.#", "#.#"),
    "I": ("###", ".#.", ".#.", ".#.", "###"),
    "K": ("#.#", "#.#", "##.", "#.#", "#.#"),
    "L": ("#..", "#..", "#..", "#..", "###"),
    "M": ("#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"),
    "N": ("#.#", "###", "###", "###", "#.#"),
    "O": ("###", "#.#", "#.#", "#.#", "###"),
    "P": ("##.", "#.#", "##.", "#..", "#.."),
    "R": ("##.", "#.#", "##.", "#.#", "#.#"),
    "S": (".##", "#..", ".#.", "..#", "##."),
    "T": ("###", ".#.", ".#.", ".#.", ".#."),
}


def patch_pixels(patch: bytes) -> np.ndarray:
    width = patch[0] | ((patch[3] & 2) << 7)
    height = patch[1]
    if patch[3] >> 2 != 4:
        raise ValueError("M_DOOM is expected to use vp8_runs")
    palette = np.frombuffer(patch[6:6 + patch[2]], dtype=np.uint8)
    data = memoryview(patch)[6 + patch[2]:]
    pixels = np.full((height, width), -1, dtype=np.int16)
    offset = 0
    for y in range(height):
        x = 0
        while x < width:
            gap = data[offset]
            offset += 1
            if gap == 0xFF:
                break
            x += gap
            length = data[offset]
            offset += 1
            pixels[y, x:x + length] = palette[np.frombuffer(
                data[offset:offset + length], dtype=np.uint8)]
            offset += length
            x += length
    return pixels


def encode_tiles(pixels: np.ndarray) -> bytes:
    height, width = pixels.shape
    assert height % 8 == width % 8 == 0
    data = bytearray()
    for top in range(0, height, 8):
        for left in range(0, width, 8):
            tile = pixels[top:top + 8, left:left + 8]
            for plane in (0, 1):
                for row in tile:
                    data.append(sum(((int(value) >> plane) & 1) << (7 - x)
                                    for x, value in enumerate(row)))
    return bytes(data)


def logo_pixels(whx: Whx) -> np.ndarray:
    source = WHDATA.read_text().split("#define VPATCH_LIST \\", 1)[1]
    labels = re.findall(r"VPATCH_NAME\(([^)]+)\)", source.split("\n\nenum", 1)[0])
    lookup = np.frombuffer(whx.named("P_START"), dtype="<u2")
    original = patch_pixels(whx.lump(int(lookup[labels.index("M_DOOM") + 1])))
    assert original.shape == (60, 123)
    # The 123x60 patch exceeds the NES's eight-sprites-per-line limit. Redraw
    # its four block letters as a legible 64x24 mark, retaining its red/gold
    # palette relationship. Each column is three pixels wide.
    logo_font = {
        "D": ("####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."),
        "O": (".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."),
        "M": FONT["M"],
    }
    result = np.zeros((LOGO_SIZE[1], LOGO_SIZE[0]), dtype=np.uint8)
    for letter, char in enumerate("DOOM"):
        for row, pattern in enumerate(logo_font[char]):
            for col, pixel in enumerate(pattern):
                if pixel != "#":
                    continue
                x0, y0 = letter * 16 + col * 3, row * 3 + 1
                result[y0:y0 + 3, x0:x0 + 3] = 3 if row >= 5 else 2
    opaque = result != 0
    for y in range(LOGO_SIZE[1] - 2, -1, -1):
        for x in range(LOGO_SIZE[0] - 2, -1, -1):
            if opaque[y, x] and not opaque[y + 1, x + 1]:
                result[y + 1, x + 1] = 1
    return result


def episode_tiles() -> tuple[bytes, dict[str, int]]:
    pairs = sorted({(line + " ")[at:at + 2]
                    for episode in EPISODE_LINES[0] for line in episode
                    for at in range(0, len(line), 2)})
    data = bytearray()
    for pair in pairs:
        tile = np.zeros((8, 8), dtype=np.uint8)
        for char_index, char in enumerate(pair):
            for row, source_row in enumerate((0, 1, 2, 2, 3, 4, 4)):
                pattern = FONT[char][source_row]
                for col, pixel in enumerate(pattern):
                    if pixel == "#":
                        x = char_index * 4 + col
                        tile[row, x] = 1
                        if row + 1 < 8 and x + 1 < 8:
                            tile[row + 1, x + 1] = 2
        data.extend(encode_tiles(tile))
    return bytes(data), {pair: index for index, pair in enumerate(pairs)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--whx", type=Path, default=ROOT / "doom/rp2040-doom/doom1.whx")
    parser.add_argument("--output", type=Path, default=ROOT / "doom/assets")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    logo = logo_pixels(Whx(args.whx))
    pairs, lookup = episode_tiles()
    assert len(pairs) <= 32 * 16
    (args.output / "doom_menu_logo.chr").write_bytes(encode_tiles(logo))
    (args.output / "doom_episode_pairs.chr").write_bytes(pairs)
    palette = np.array(((0, 0, 0, 0), (72, 16, 16, 255),
                        (176, 32, 32, 255), (232, 184, 64, 255)), dtype=np.uint8)
    preview = Image.fromarray(palette[logo], "RGBA")
    preview.resize((256, 96), Image.Resampling.NEAREST).save(
        args.output / "doom_menu_logo_preview.png")
    (args.output / "doom_menu_art.json").write_text(json.dumps({
        "source": str(args.whx.resolve().relative_to(ROOT)),
        "logo_source_patch": "M_DOOM", "logo_size": list(LOGO_SIZE),
        "logo_tiles": len(encode_tiles(logo)) // 16,
        "episode_lines": EPISODE_LINES[0], "pair_tile_ids": lookup,
    }, indent=2) + "\n")
    print(f"logo: {len(encode_tiles(logo)) // 16} tiles; episode pairs: {len(lookup)} tiles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
