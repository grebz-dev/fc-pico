# SPDX-License-Identifier: BSD-3-Clause
"""Captures from the native cartridge, decoded and compared with a MesenCE screenshot."""
from __future__ import annotations

from pathlib import Path

import pytest

from fcpico import capture, native, ppu_decode, protocol

ROOT = Path(__file__).resolve().parents[2]
PRG = (ROOT / "tutorial_project/BOOTROM/rom.NES").read_bytes()[16:]
MESEN_S0 = ROOT / "tests/fixtures/mesen-s0-pattern0.png"


@pytest.fixture(scope="module")
def lib():
    try:
        return native.load()
    except native.NativeError as error:
        pytest.skip(str(error))


def test_pattern_capture_matches_the_mesen_screenshot(lib):
    """The S0 screenshot (reviewed golden) is the tutorial ROM showing pattern 0.
    Decoding the same picture natively must give a one-to-one mapping between NES
    colour indices and screenshot colours, whatever RGB palette the emulator uses."""
    image = pytest.importorskip("PIL.Image")
    with lib.cart("pattern", "0", PRG) as cart:
        shot = capture.capture(cart, lib.layout)
    assert shot.data_uploads >= 2
    indices = ppu_decode.decode(shot.stream, shot.palette, shot.attributes, lib.layout)
    screenshot = image.open(MESEN_S0).convert("RGB")
    assert screenshot.size == (256, 240)
    forward, backward = {}, {}
    for y in range(240):
        for x in range(256):
            index, rgb = indices[y][x], screenshot.getpixel((x, y))
            assert forward.setdefault(index, rgb) == rgb, (x, y)
            assert backward.setdefault(rgb, index) == index, (x, y)
    assert len(forward) > 4  # several sub-palettes are in use


def test_demo_capture_responds_to_the_controller(lib):
    with lib.cart("demo", None, PRG) as cart:
        still = capture.capture(cart, lib.layout, frames=12)
    with lib.cart("demo", None, PRG) as cart:
        moved = capture.capture(cart, lib.layout, frames=12, pads=[0] * 8 + [protocol.KEY_RIGHT] * 4)
    a = ppu_decode.decode(still.stream, still.palette, still.attributes, lib.layout)
    b = ppu_decode.decode(moved.stream, moved.palette, moved.attributes, lib.layout)
    assert a != b
    square = lambda rows: min(x for row in rows for x, i in enumerate(row) if i == 0x16)
    assert square(b) > square(a)


def test_cartridge_errors_are_reported(lib):
    with pytest.raises(native.NativeError, match="options"):
        lib.cart("pattern", "9", PRG)
    with pytest.raises(native.NativeError, match="unknown provider"):
        lib.cart("nonexistent", None, PRG)
