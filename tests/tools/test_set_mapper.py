# SPDX-License-Identifier: BSD-3-Clause
"""The emulator-only mapper header must leave the boot ROM's PRG untouched."""
from pathlib import Path

import pytest

from nes.set_mapper import set_mapper

ROM = Path(__file__).resolve().parents[2] / "tutorial_project/BOOTROM/rom.NES"


def test_prg_and_fix_bank_unchanged():
    rom = ROM.read_bytes()
    mapped = set_mapper(rom)
    assert mapped[16:] == rom[16:]
    assert mapped[7] & 0x0C == 0x08  # NES 2.0
    assert (mapped[6] >> 4) | (mapped[7] & 0xF0) | ((mapped[8] & 0x0F) << 8) == 4093
    assert mapped[11] == 7  # 8 KiB CHR RAM


@pytest.mark.parametrize("data", [b"", b"NES\x1a" + bytes(32780), b"bad!" + bytes(32780)])
def test_rejects_other_images(data):
    with pytest.raises(ValueError):
        set_mapper(data)
