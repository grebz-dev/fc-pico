# SPDX-License-Identifier: BSD-3-Clause
"""Portable reference for the picture stream: packing, unpacking and colours.

Geometry comes from the generated layout descriptor (``fcpico.layout``), never from
offsets written here. The firmware's own encoder is the native
``fcbus_stream_put_line()`` (see ``fcpico.native``); the tests check that this
reference produces the same bytes.

A stream buffer holds 16-bit little-endian words. The PPU reads one word per 8-pixel
tile row: plane 0 in the low byte, plane 1 in the high byte, leftmost pixel in the
most significant bit. Visible line ``y`` starts at word ``head_words +
y * words_per_line``. The mailbox follows the picture at ``mailbox_offset``.
"""
from __future__ import annotations

from . import layout as layout_module

Rows = list  # 240 rows of 256 pixel values 0..3


def _geometry(geometry):
    return geometry or layout_module.load()


def word_index(line: int, tile: int, geometry=None) -> int:
    g = _geometry(geometry)
    if not (0 <= line < g.lines and 0 <= tile < g.words_per_line):
        raise ValueError(f"line {line}, tile {tile} outside the picture")
    return g.head_words + line * g.words_per_line + tile


def pack_word(pixels8) -> int:
    word = 0
    for colour in pixels8:
        word = (word << 1) | (colour & 1) | ((colour & 2) << 7)
    return word


def unpack_word(word: int) -> list[int]:
    return [((word >> bit) & 1) | (((word >> (8 + bit)) & 1) << 1) for bit in range(7, -1, -1)]


def pack_frame(rows: Rows, mailbox: bytes = b"", geometry=None) -> bytes:
    """A full stream buffer for `rows`, with `mailbox` at the mailbox offset."""
    g = _geometry(geometry)
    if len(rows) != g.lines or any(len(r) != 256 for r in rows):
        raise ValueError(f"expected {g.lines} rows of 256 pixels")
    if len(mailbox) > g.mailbox_bytes:
        raise ValueError(f"mailbox longer than {g.mailbox_bytes} bytes")
    buffer = bytearray(g.buffer_bytes)
    for line, row in enumerate(rows):
        for tile in range(g.words_per_line):
            word = pack_word(row[tile * 8:tile * 8 + 8])
            at = 2 * word_index(line, tile, g)
            buffer[at:at + 2] = word.to_bytes(2, "little")
    buffer[g.mailbox_offset:g.mailbox_offset + len(mailbox)] = mailbox
    return bytes(buffer)


def unpack_frame(stream: bytes, geometry=None) -> Rows:
    """The picture held in `stream` (at least `mailbox_offset` bytes)."""
    g = _geometry(geometry)
    if len(stream) < g.mailbox_offset:
        raise ValueError(f"stream has {len(stream)} bytes; the picture needs {g.mailbox_offset}")
    rows = []
    for line in range(g.lines):
        row = []
        for tile in range(g.words_per_line):
            at = 2 * word_index(line, tile, g)
            row.extend(unpack_word(int.from_bytes(stream[at:at + 2], "little")))
        rows.append(row)
    return rows


def mailbox(stream: bytes, geometry=None) -> bytes:
    g = _geometry(geometry)
    return bytes(stream[g.mailbox_offset:g.mailbox_offset + g.mailbox_bytes])


def subpalette(attributes: bytes, x: int, y: int) -> int:
    """Background sub-palette (0..3) the attribute table assigns to pixel (x, y)."""
    byte = attributes[(y // 32) * 8 + x // 32]
    shift = ((y // 16) & 1) * 4 + ((x // 16) & 1) * 2
    return (byte >> shift) & 3


def colour_indices(rows: Rows, palette: bytes, attributes: bytes) -> list[list[int]]:
    """NES colour index (0..63) of every pixel, with the console's palette rules:
    colour 0 of every sub-palette shows the backdrop, palette[0]."""
    if len(palette) < 16 or len(attributes) != 64:
        raise ValueError("need at least 16 palette bytes and a 64-byte attribute table")
    out = []
    for y, row in enumerate(rows):
        out.append([palette[0] & 0x3F if c == 0 else palette[subpalette(attributes, x, y) * 4 + c] & 0x3F
                    for x, c in enumerate(row)])
    return out
