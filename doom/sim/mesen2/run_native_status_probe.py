#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Check native tall status and 3x3 face with a compact v4 Doom frame."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "doom/tools"))
from fcpico import protocol, stream  # noqa: E402
from native_status_art import bg_status_tiles, resident_art  # noqa: E402


def packet(health: int, armor: int, face: int, keys: int, weapons: int,
           visible: bool = True, menu: bool = False, selection: int = 0,
           menu_id: int = 1) -> bytes:
    ui = bytearray(16)
    ui[0] = 1
    ui[1] = int(visible) | (4 if menu else 0) | (selection << 4)
    ui[2] = face
    ui[3] = keys
    ui[4] = weapons
    ui[5] = 2 | (menu_id << 4 if menu else 0)
    ui[6:8] = health.to_bytes(2, "little")
    ui[8:10] = armor.to_bytes(2, "little")
    check = 0xA5
    for value in ui[:15]:
        check ^= value
    ui[15] = check
    return bytes(ui)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("/tmp/fcpico-native-status-u0"))
    parser.add_argument("--health", type=int, default=100)
    parser.add_argument("--armor", type=int, default=75)
    parser.add_argument("--face", type=int, default=0)
    parser.add_argument("--menu", action="store_true")
    parser.add_argument("--menu-id", type=int, default=1)
    parser.add_argument("--no-status", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / "doom/tools/build_native_status_probe.py"),
                    "--output", str(output)], check=True)
    pixels = np.zeros((240, 256), dtype=np.uint8)
    y, x = np.indices((184, 256))
    pixels[8:192] = ((x // 16 + y // 16) % 3 + 1).astype(np.uint8)
    if not args.no_status:
        pixels[192:240] = 1
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V4)
    mailbox[protocol.MBX_FLAGS] = (protocol.MBX_FLAG_V2 | protocol.MBX_FLAG_V3 |
                                   protocol.MBX_FLAG_V4 | protocol.MBX_FLAG_UI_VALID |
                                   protocol.MBX_FLAG_ATTR_VALID |
                                   protocol.MBX_FLAG_PAL_VALID)
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + 16] = bytes((15, 0, 16, 48)) * 4
    if not args.no_status:
        mailbox[protocol.MBX_PAL + 12:protocol.MBX_PAL + 16] = bytes((15, 0, 16, 22))
        attributes = bytearray(64)
        for by in range(12, 15):
            for bx in range(16):
                stream.attr_set(attributes, bx, by, 3)
        mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR + 64] = attributes
    ui = packet(args.health, args.armor, args.face, 0x09, 0x7F,
                visible=not args.no_status, menu=args.menu,
                selection=2 if args.menu else 0, menu_id=args.menu_id)
    mailbox[protocol.MBX_UI:protocol.MBX_UI + 16] = ui
    frame = output / "frame.bin"
    frame.write_bytes(stream.encode_frame(pixels, mailbox, native_text=True))
    subprocess.run([sys.executable, str(HERE / "run_doom_frame.py"), str(frame),
                    "--rom", str(output / "probe.nes"),
                    "--output", str(output / "mesen"),
                    *(["--min-correlation", "0.85"] if args.menu else []),
                    *(["--press-start-frame", "155"] if args.menu else [])], check=True)
    run = output / "mesen/run"
    oam = (run / "oam.bin").read_bytes()
    sprite_palette = (run / "sprite_pal.bin").read_bytes()
    assert sprite_palette[1] == sprite_palette[3] == 0x16
    assert sprite_palette[11] == 0x06
    if not args.no_status:
        assert (run / "palette.bin").read_bytes()[15] == 0x16
        background_chr = (run / "background_chr.bin").read_bytes()
        for char, tile in bg_status_tiles():
            assert background_chr[char * 16:char * 16 + 16] == tile
        picture = Image.open(run / "final.png").convert("RGB")
        assert picture.getpixel((100, 230)) == picture.getpixel((10, 230))
    if args.menu:
        assert __import__("json").loads((run / "lua_result.json").read_text())["start_polls"] >= 1
        if not args.no_status:
            assert all(oam[n * 4] < 0xEF for n in range(25)), "HUD vanished on Start/menu"
        else:
            assert all(oam[n * 4] == 0xEF for n in range(35))
        expected_first = {1: (87, "N"), 2: (87, "E"), 3: (87, "E"),
                          4: (58, "E"), 5: (87, "E")}[args.menu_id]
        assert oam[35 * 4:35 * 4 + 4] == bytes((expected_first[0],
            ord(expected_first[1]), 0, 80 if args.menu_id >= 4 else 92))
        assert oam[61 * 4] == expected_first[0] + 32
        assert Image.open(run / "final.png").convert("RGB").getpixel((70, 100)) != (0, 0, 0)
        print("native sprite menu and status state survive Start")
        return 0
    art = resident_art()
    assert (run / "sprite_chr.bin").read_bytes()[0x800:0x800 + len(art)] == art
    assert sprite_palette[5:8] == bytes((0x07, 0x18, 0x26))
    assert (run / "ui_mailbox.bin").read_bytes() == ui
    assert (run / "text_row.bin").read_bytes()[2:30] == bytes((32,)) * 28
    assert all(oam[n * 4] == 191 for n in range(8))
    assert all(oam[n * 4] == 199 for n in range(8, 16))
    assert [oam[n * 4] for n in range(16, 25)] == [207] * 3 + [215] * 3 + [223] * 3
    assert [oam[n * 4] for n in range(25, 32)] == [207] * 5 + [215] * 2
    assert [oam[n * 4 + 2] for n in range(25, 32)] == [0, 2, 0, 0, 0, 0, 0]
    assert [oam[n * 4] for n in range(32, 35)] == [215, 0xEF, 0xEF]
    for group, value in ((0, args.health), (4, args.armor)):
        digits = f"{min(value, 999):03d}"
        for i, char in enumerate(digits):
            tile = 0x80 + int(char) * 2
            assert oam[(group + 1 + i) * 4 + 1] == tile
            assert oam[(group + 9 + i) * 4 + 1] == tile + 1
    face_group = (args.face - 33 if args.face >= 40 else
                  10 if args.face % 8 == 6 else
                  9 if args.face % 8 in (3, 4, 5) else
                  args.face if args.face < 3 else
                  args.face // 8 + 2 if args.face >= 8 else 0)
    assert [oam[n * 4 + 1] for n in range(16, 25)] == [
        0x98 + face_group * 9 + n for n in range(9)]
    print("native status: tall values, face, keys/weapons, v4 count=15122")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
