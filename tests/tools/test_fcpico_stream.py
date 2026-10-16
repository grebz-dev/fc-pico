# SPDX-License-Identifier: BSD-3-Clause
"""The portable stream reference against the descriptor and the native encoder."""
from __future__ import annotations

import random

import pytest

from fcpico import layout, native, stream

G = layout.load()


def random_rows(seed: int):
    rng = random.Random(seed)
    return [[rng.randrange(4) for _ in range(256)] for _ in range(G.lines)]


def test_word_placement_follows_the_descriptor():
    assert stream.word_index(0, 0) == G.head_words
    assert stream.word_index(1, 0) == G.head_words + G.words_per_line
    assert stream.word_index(239, 31) == G.head_words + 239 * G.words_per_line + 31
    with pytest.raises(ValueError):
        stream.word_index(240, 0)


def test_word_bit_layout():
    assert stream.pack_word([1, 2, 0, 0, 0, 0, 0, 3]) == 0x0080 | 0x4000 | 0x0101
    assert stream.unpack_word(0x0080 | 0x4000 | 0x0101) == [1, 2, 0, 0, 0, 0, 0, 3]


def test_round_trip_and_mailbox():
    rows = random_rows(1)
    box = bytes(range(G.mailbox_bytes))
    buf = stream.pack_frame(rows, box)
    assert len(buf) == G.buffer_bytes
    assert stream.unpack_frame(buf) == rows
    assert stream.mailbox(buf) == box


def test_attribute_quadrants_and_backdrop():
    attributes = bytes([0b11_10_01_00] + [0] * 63)
    assert [stream.subpalette(attributes, x, y) for x, y in ((0, 0), (16, 0), (0, 16), (16, 16))] == [0, 1, 2, 3]
    palette = bytes([0x0F, 1, 2, 3, 0x30, 5, 6, 7, 0x31, 9, 10, 11, 0x32, 13, 14, 15])
    rows = [[0] * 256 for _ in range(G.lines)]
    rows[0][16] = 1  # sub-palette 1, colour 1
    indices = stream.colour_indices(rows, palette, attributes)
    assert indices[0][0] == 0x0F and indices[0][16] == 5
    assert indices[16][16] == 0x0F  # colour 0 is the backdrop in every sub-palette


@pytest.fixture(scope="module")
def lib():
    try:
        return native.load()
    except native.NativeError as error:
        pytest.skip(str(error))


def test_native_layout_matches_descriptor(lib):
    assert lib.layout == G


def test_reference_matches_the_native_encoder(lib):
    rows = random_rows(2)
    assert stream.pack_frame(rows) == lib.encode_frame([bytes(r) for r in rows])
    assert [list(r) for r in lib.decode_frame(stream.pack_frame(rows))] == rows
