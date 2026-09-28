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
        for half in (doubled[:8], doubled[8:]):
            result.extend(int(row.replace("#", "1").replace(".", "0"), 2)
                          for row in half)
            result.extend(bytes(8))
    assert len(result) == 24 * 16
    return bytes(result)


def bg_status_tiles() -> list[tuple[int, bytes]]:
    tiles = []
    for char in BG_STATUS_ORDER:
        glyph = GLYPHS.get(char, (".....",) * 7)
        rows = ["." + row + ".." for row in glyph] + ["........"]
        ink = bytes(int(row.replace("#", "1").replace(".", "0"), 2)
                    for row in rows)
        tiles.append((ord(char), bytes((0xFF,)) * 8 + ink))
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
