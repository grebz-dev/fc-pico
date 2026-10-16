# SPDX-License-Identifier: BSD-3-Clause
"""bin2c.py must reproduce the tutorial's resdata.c byte for byte."""
from pathlib import Path

from nes import bin2c

RES = Path(__file__).resolve().parents[2] / "tutorial_project/tuto1_hw/res"


def test_reproduces_resdata_c():
    text = bin2c.bin2c_text((RES / "res.bin").read_bytes(), "_resdata")
    assert text.encode() == (RES / "resdata.c").read_bytes()


def test_cli(tmp_path):
    out = tmp_path / "resdata.c"
    assert bin2c.main([str(RES / "res.bin"), str(out), "_resdata"]) == 0
    assert out.read_bytes() == (RES / "resdata.c").read_bytes()
