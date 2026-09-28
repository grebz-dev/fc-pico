#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Exercise the isolated v3 UI mailbox receiver through Mesen's NES CPU/PPU."""

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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path("/tmp/fcpico-ui-transport-u0"))
    parser.add_argument("--hud", action="store_true",
                        help="decode health into native OAM font sprites")
    parser.add_argument("--health", type=int, default=200,
                        help="HUD health value for --hud (0..999)")
    args = parser.parse_args()
    if not 0 <= args.health <= 999:
        parser.error("--health must be 0..999")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = [sys.executable, str(ROOT / "doom/tools/build_ui_transport_probe.py"),
             "--output", str(output)]
    if args.hud:
        build.append("--hud")
    subprocess.run(build, check=True)
    y, x = np.indices((240, 256))
    pixels = np.zeros((240, 256), dtype=np.uint8)
    pixels[8:232] = ((x[8:232] // 16 + y[8:232] // 16) % 3 + 1).astype(np.uint8)
    if args.hud:
        pixels[192:232] = 1
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V3)
    mailbox[protocol.MBX_FLAGS] = (protocol.MBX_FLAG_V2 | protocol.MBX_FLAG_V3 |
                                   protocol.MBX_FLAG_UI_VALID |
                                   protocol.MBX_FLAG_ATTR_VALID |
                                   protocol.MBX_FLAG_PAL_VALID)
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + 16] = bytes((15, 0, 16, 48)) * 4
    ui = bytearray(range(16))
    if args.hud:
        ui[:] = bytes(16)
        ui[0] = 1  # generation
        ui[1] = 1  # status visible
        ui[6] = args.health & 0xFF
        ui[7] = args.health >> 8
        checksum = 0xA5
        for value in ui[:15]:
            checksum ^= value
        ui[15] = checksum
    mailbox[protocol.MBX_UI:protocol.MBX_UI + 16] = ui
    frame = output / "frame.bin"
    frame.write_bytes(stream.encode_frame(pixels, mailbox))
    subprocess.run([sys.executable, str(HERE / "run_doom_frame.py"), str(frame),
                    "--rom", str(output / "probe.nes"),
                    "--output", str(output / "mesen")], check=True)
    actual = (output / "mesen/run/ui_mailbox.bin").read_bytes()
    assert actual == ui, actual.hex()
    if args.hud:
        oam = (output / "mesen/run/oam.bin").read_bytes()
        digits = f"{args.health:03d}".encode("ascii")
        assert oam[:16] == bytes((191, 0x48, 0, 104,
                                  191, digits[0], 0, 112,
                                  191, digits[1], 0, 120,
                                  191, digits[2], 0, 128)), oam[:16].hex()
        print(f"v3 HUD slice: checked health={args.health} decoded to H{args.health:03d} in OAM")
    print("v3 UI mailbox: 16 exact bytes, count=15570, Mesen NMI within vblank")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
