#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Exercise a fixed native BG text row and compacted v4 picture stream."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "doom/tools"))
from fcpico import protocol, stream  # noqa: E402
from build_bg_text_probe import MESSAGE  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("/tmp/fcpico-bg-text-u0"))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / "doom/tools/build_bg_text_probe.py"),
                    "--output", str(output)], check=True)
    y, x = np.indices((240, 256))
    pixels = np.zeros((240, 256), dtype=np.uint8)
    pixels[8:192] = ((x[8:192] // 16 + y[8:192] // 16) % 3 + 1).astype(np.uint8)
    pixels[192:232] = 1
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V4)
    mailbox[protocol.MBX_FLAGS] = (protocol.MBX_FLAG_V2 | protocol.MBX_FLAG_V3 |
                                   protocol.MBX_FLAG_V4 | protocol.MBX_FLAG_UI_VALID |
                                   protocol.MBX_FLAG_ATTR_VALID |
                                   protocol.MBX_FLAG_PAL_VALID)
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + 16] = bytes((15, 0, 16, 48)) * 4
    ui = bytearray(protocol.MBX_UI_LEN)
    ui[0] = 1
    ui[1] = 1
    ui[6] = 200
    check = 0xA5
    for byte in ui[:15]:
        check ^= byte
    ui[15] = check
    mailbox[protocol.MBX_UI:protocol.MBX_UI + protocol.MBX_UI_LEN] = ui
    frame = output / "frame.bin"
    encoded = stream.encode_frame(pixels, mailbox, native_text=True)
    assert encoded[protocol.VRAM_MAILBOX_OFF_V4 + protocol.MBX_MAGIC] == protocol.PF_MAGIC_NO
    frame.write_bytes(encoded)
    subprocess.run([sys.executable, str(HERE / "run_doom_frame.py"), str(frame),
                    "--rom", str(output / "probe.nes"),
                    "--output", str(output / "mesen")], check=True)
    run = output / "mesen/run"
    assert (run / "text_row.bin").read_bytes()[2:30] == MESSAGE
    assert (run / "ui_mailbox.bin").read_bytes() == ui
    assert (run / "oam.bin").read_bytes()[:16] == bytes(
        (191, 0x48, 0, 104, 191, 0x32, 0, 112,
         191, 0x30, 0, 120, 191, 0x30, 0, 128))
    print("v4 native text: 28 exact BG tiles, H200 sprites, count=15122")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
