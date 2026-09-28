#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""U0: build a setup-time sprite probe and compare Mesen with a baseline ROM."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "doom/tools"))
from fcpico import protocol, stream  # noqa: E402
from ppu_decode import decode_to_rgb, decode_with_sprites  # noqa: E402


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("/tmp/fcpico-sprite-probe"))
    parser.add_argument("--baseline-rom", type=Path,
                        help="existing v2 ROM (default: assemble a fresh one)")
    parser.add_argument("--nesasm", type=Path,
                        default=ROOT / ".build/tools/nesasm/nesasm")
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--diagnostic", action="store_true",
                        help="test duplicate A/diamond OAM slots and CHR addresses")
    parser.add_argument("--limit", action="store_true",
                        help="prove Mesen drops the ninth same-scanline sprite")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = [sys.executable, str(ROOT / "doom/tools/build_sprite_probe.py"),
             "--output", str(output), "--nesasm", str(args.nesasm)]
    if args.diagnostic:
        build.append("--diagnostic")
    if args.limit:
        build.append("--limit")
    run(build)
    baseline_rom = args.baseline_rom
    if baseline_rom is None:
        baseline_dir = output / "baseline-rom"
        subprocess.run([str(ROOT / "doom/bootrom/build.sh"), str(baseline_dir)],
                       check=True, env=dict(os.environ, NESASM_BIN=str(args.nesasm.resolve())))
        baseline_rom = baseline_dir / "doom.nes"

    y, x = np.indices((240, 256))
    pixels = np.zeros((240, 256), dtype=np.uint8)
    pixels[8:232] = ((x[8:232] // 16 + y[8:232] // 16) % 3 + 1).astype(np.uint8)
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V2)
    mailbox[protocol.MBX_FLAGS] = (protocol.MBX_FLAG_V2 |
                                    protocol.MBX_FLAG_ATTR_VALID |
                                    protocol.MBX_FLAG_PAL_VALID)
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + 16] = bytes((15, 0, 16, 48)) * 4
    frame = output / "frame.bin"
    frame.write_bytes(stream.encode_frame(pixels, mailbox))

    for name, rom in (("probe", output / "probe.nes"),
                      ("baseline", baseline_rom.resolve())):
        run([sys.executable, str(HERE / "run_doom_frame.py"), str(frame),
             "--rom", str(rom), "--frames", str(args.frames),
             "--output", str(output / name)])

    probe = output / "probe/run"
    baseline = output / "baseline/run"
    oam = (probe / "oam.bin").read_bytes()
    chr_data = (probe / "sprite_chr.bin").read_bytes()
    sprite_pal = (probe / "sprite_pal.bin").read_bytes()
    expected_chr = (output / "sprite_chr.bin").read_bytes()
    if oam != (output / "oam.bin").read_bytes():
        raise AssertionError("Mesen OAM differs from probe setup")
    art_end = 0x880 if args.diagnostic else 0x860
    if chr_data[0x800:art_end] != expected_chr[0x800:art_end] or \
            chr_data[0xFF0:] != expected_chr[0xFF0:] or any(chr_data[art_end:0xFF0]):
        raise AssertionError("sprite CHR storage/write selection differs")
    if sprite_pal != (output / "sprite_pal.bin").read_bytes():
        raise AssertionError("sprite palette differs")

    actual = np.asarray(Image.open(probe / "final.png").convert("RGB"))
    old = np.asarray(Image.open(baseline / "final.png").convert("RGB"))
    changed = np.any(actual != old, axis=2)
    outside = changed.copy()
    outside[32:48, 24:168 if args.limit else 128] = False
    if args.diagnostic:
        outside[64:72, 24:96] = False
    if outside.any() or changed.sum() == 0:
        raise AssertionError("probe changed background pixels or drew no sprites")

    attr = (probe / "attributes.bin").read_bytes()
    pal = (probe / "palette.bin").read_bytes()
    reference = decode_with_sprites(frame.read_bytes(), oam, chr_data, sprite_pal, attr, pal)
    reference_bg = decode_to_rgb(frame.read_bytes(), attr, pal)
    expected_mask = np.any(reference != reference_bg, axis=2)
    if not np.array_equal(changed, expected_mask):
        raise AssertionError(f"Mesen sprite mask differs at {np.count_nonzero(changed != expected_mask)} pixels")
    if args.limit:
        # Entry 8 is the ninth sprite on lines 32..39, at x=160. It must
        # exist in OAM yet contribute no visible pixels.
        if oam[8 * 4:8 * 4 + 4] != bytes((31, 129, 0, 160)):
            raise AssertionError("ninth sprite is absent from OAM")
        if changed[32:40, 160:168].any() or not changed[32:40, 136:144].any():
            raise AssertionError("Mesen did not enforce the eight-sprite scanline limit")
    print(f"U0 sprite probe: {changed.sum()} exact sprite-mask pixels; "
          "OAM/CHR/palette exact; background unchanged; v2 stream stable")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
