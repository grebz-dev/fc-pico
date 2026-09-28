#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Export/import pixel-exact PNG sources for editable NES sprite sheets."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

from build_doom_menu_art import encode_tiles

ASSETS = Path(__file__).resolve().parents[1] / "assets"
LOGO_PALETTE = ((0, 0, 0, 0), (20, 24, 80, 255),
                (80, 96, 216, 255), (232, 184, 64, 255))
FACE_PALETTE = ((0, 0, 0, 0), (135, 87, 51, 255),
                (207, 131, 83, 255), (131, 31, 31, 255))
TEXT_PALETTE = ((0, 0, 0, 0), (184, 40, 32, 255),
                (48, 16, 16, 255), (240, 176, 120, 255))
SHEETS = {
    "logo": ("doom_menu_logo", 8, 4, 1, 1, 32, LOGO_PALETTE),
    "pause_logo": ("doom_menu_logo_small", 7, 3, 1, 1, 21, LOGO_PALETTE),
    "faces": ("doomguy_faces", 7, 6, 3, 3, 378, FACE_PALETTE),
    "paired_text": ("doom_episode_pairs", 8, 3, 1, 1, 19, TEXT_PALETTE),
}


def decode_tiles(data: bytes, columns: int, rows: int,
                 group_width: int, group_height: int) -> np.ndarray:
    pixels = np.zeros((rows * group_height * 8,
                       columns * group_width * 8), dtype=np.uint8)
    for tile_index in range(len(data) // 16):
        group, within = divmod(tile_index, group_width * group_height)
        group_y, group_x = divmod(group, columns)
        tile_y, tile_x = divmod(within, group_width)
        top = (group_y * group_height + tile_y) * 8
        left = (group_x * group_width + tile_x) * 8
        tile = data[tile_index * 16:(tile_index + 1) * 16]
        for y in range(8):
            for x in range(8):
                bit = 7 - x
                pixels[top + y, left + x] = (((tile[y] >> bit) & 1) |
                                              (((tile[y + 8] >> bit) & 1) << 1))
    return pixels


def encode_sheet(pixels: np.ndarray, columns: int, rows: int,
                 group_width: int, group_height: int, tile_count: int) -> bytes:
    data = bytearray()
    for group_y in range(rows):
        for group_x in range(columns):
            for tile_y in range(group_height):
                for tile_x in range(group_width):
                    if len(data) == tile_count * 16:
                        return bytes(data)
                    top = (group_y * group_height + tile_y) * 8
                    left = (group_x * group_width + tile_x) * 8
                    data.extend(encode_tiles(pixels[top:top + 8, left:left + 8]))
    return bytes(data)


def rgba_to_indices(image: Image.Image, palette: tuple[tuple[int, ...], ...]) -> np.ndarray:
    rgba = np.asarray(image.convert("RGBA"))
    result = np.zeros(rgba.shape[:2], dtype=np.uint8)
    matched = rgba[:, :, 3] == 0
    for index, color in enumerate(palette[1:], 1):
        mask = np.all(rgba == color, axis=2)
        result[mask] = index
        matched |= mask
    if not np.all(matched):
        y, x = np.argwhere(~matched)[0]
        raise ValueError(f"pixel ({x}, {y}) is outside the sheet's four-color palette")
    return result


def process(sheet: str, operation: str) -> None:
    name, columns, rows, group_width, group_height, tile_count, palette = SHEETS[sheet]
    chr_path = ASSETS / f"{name}.chr"
    png_path = ASSETS / f"{name}_edit.png"
    size = (columns * group_width * 8, rows * group_height * 8)
    if operation == "export":
        data = chr_path.read_bytes()
        if len(data) != tile_count * 16:
            raise ValueError(f"{chr_path}: expected {tile_count} tiles")
        pixels = decode_tiles(data, columns, rows, group_width, group_height)
        Image.fromarray(np.asarray(palette, dtype=np.uint8)[pixels], "RGBA").save(png_path)
        print(f"editable sheet: {png_path} ({size[0]}x{size[1]})")
    else:
        image = Image.open(png_path)
        if image.size != size:
            raise ValueError(f"{png_path}: expected {size[0]}x{size[1]} pixels")
        pixels = rgba_to_indices(image, palette)
        chr_path.write_bytes(encode_sheet(pixels, columns, rows,
                                          group_width, group_height, tile_count))
        print(f"updated CHR: {chr_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("export", "import"))
    parser.add_argument("sheets", nargs="*", choices=tuple(SHEETS))
    args = parser.parse_args()
    for sheet in args.sheets or SHEETS:
        process(sheet, args.operation)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
