#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Resident NES art for tall numbers and eleven Doomguy face variants."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "doom/assets"

GLYPHS = {
    "0": (".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."),
    "1": ("..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."),
    "2": (".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"),
    "3": ("####.", "....#", "....#", ".###.", "....#", "....#", "####."),
    "4": ("...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."),
    "5": ("#####", "#....", "#....", "####.", "....#", "....#", "####."),
    "6": (".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."),
    "7": ("#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."),
    "8": (".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."),
    "9": (".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."),
    "H": ("#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"),
    "R": ("####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"),
    "B": ("####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."),
    "S": (".####", "#....", "#....", ".###.", "....#", "....#", "####."),
    "C": (".####", "#....", "#....", "#....", "#....", "#....", ".####"),
}
ORDER = "0123456789HR"
BG_STATUS_ORDER = " 0123456789BSRC"
FACE_NAMES = ["STFST00", "STFST01", "STFST02",
              *(f"STFST{i}0" for i in range(1, 5)),
              "STFGOD0", "STFDEAD0", "STFOUCH0", "STFEVL0"]


def face_resident_index(index: int) -> int:
    if index >= 40:
        return index - 33
    expression = index % 8
    if expression == 6:
        return 10
    if expression in (3, 4, 5):
        return 9
    if index < 3:
        return index
    return index // 8 + 2 if index >= 8 else 0


def tall_glyphs() -> bytes:
    result = bytearray()
    for char in ORDER:
        rows = ["." + row + ".." for row in GLYPHS[char]] + ["........"]
        doubled = [line for row in rows for line in (row, row)]
        pixels = [[1 if value == "#" else 0 for value in row] for row in doubled]
        for y in range(15):
            for x in range(7):
                if pixels[y][x] == 1 and pixels[y + 1][x + 1] == 0:
                    pixels[y + 1][x + 1] = 2
        for half in (pixels[:8], pixels[8:]):
            for plane in (0, 1):
                result.extend(sum(((pixel >> plane) & 1) << (7 - x)
                                  for x, pixel in enumerate(row)) for row in half)
    assert len(result) == 24 * 16
    return bytes(result)


def bg_status_tiles() -> list[tuple[int, bytes]]:
    tiles = []
    for char in BG_STATUS_ORDER:
        glyph = GLYPHS.get(char, (".....",) * 7)
        rows = ["." + row + ".." for row in glyph] + ["........"]
        pixels = [[3 if value == "#" else 1 for value in row] for row in rows]
        for y in range(7):
            for x in range(7):
                if pixels[y][x] == 3 and pixels[y + 1][x + 1] == 1:
                    pixels[y + 1][x + 1] = 0
        tile = bytearray()
        for plane in (0, 1):
            tile.extend(sum(((pixel >> plane) & 1) << (7 - x)
                            for x, pixel in enumerate(row)) for row in pixels)
        tiles.append((ord(char), bytes(tile)))
    return tiles


def resident_face_art() -> bytes:
    manifest = json.loads((ASSETS / "doomguy_faces.json").read_text())
    sheet = (ASSETS / "doomguy_faces.chr").read_bytes()
    assert len(sheet) == 42 * 9 * 16
    return b"".join(sheet[manifest["order"].index(name) * 144:
                          (manifest["order"].index(name) + 1) * 144]
                    for name in FACE_NAMES)


def resident_art() -> bytes:
    art = tall_glyphs() + resident_face_art()
    assert len(art) == (24 + len(FACE_NAMES) * 9) * 16
    return art


def menu_logo_art() -> bytes:
    art = (ASSETS / "doom_menu_logo.chr").read_bytes()
    assert len(art) == 32 * 16
    return art


def small_menu_logo_art() -> bytes:
    art = (ASSETS / "doom_menu_logo_small.chr").read_bytes()
    assert len(art) == 18 * 16
    return art


def episode_pair_art() -> tuple[bytes, dict[str, int], tuple[tuple[str, ...], ...]]:
    manifest = json.loads((ASSETS / "doom_menu_art.json").read_text())
    art = (ASSETS / "doom_episode_pairs.chr").read_bytes()
    lookup = manifest["pair_tile_ids"]
    assert len(art) == len(lookup) * 16
    return art, lookup, tuple(tuple(lines) for lines in manifest["episode_lines"])
