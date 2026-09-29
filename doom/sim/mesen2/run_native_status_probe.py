#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Check the native concrete status and 4x4 face with a v4 Doom frame."""

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
from native_status_art import (bg_status_tiles, episode_pair_art,
                               menu_logo_art, resident_art,
                               resident_face_map,
                               small_menu_logo_art)  # noqa: E402


def packet(health: int, armor: int, face: int, keys: int, weapons: int,
           visible: bool = True, menu: bool = False, selection: int = 0,
           menu_id: int = 1, stale_menu_id: bool = False) -> bytes:
    ui = bytearray(16)
    ui[0] = 1
    ui[1] = int(visible) | (4 if menu else 0) | (selection << 4)
    ui[2] = face
    ui[3] = keys
    ui[4] = weapons
    ui[5] = 2 | (menu_id << 4 if menu or stale_menu_id else 0)
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
    parser.add_argument("--stale-menu-id", action="store_true")
    parser.add_argument("--host-frame", type=Path,
                        help="use a real host Doom v2 picture under the native UI")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / "doom/tools/build_native_status_probe.py"),
                    "--output", str(output)], check=True)
    host = args.host_frame.read_bytes() if args.host_frame else None
    if host:
        pixels = stream.decode_frame(host)
    else:
        pixels = np.zeros((240, 256), dtype=np.uint8)
        y, x = np.indices((184, 256))
        pixels[8:192] = ((x // 16 + y // 16) % 3 + 1).astype(np.uint8)
    if not args.no_status:
        panel_tool = output / "render_native_status_panel"
        subprocess.run(["cc", "-O2", "-I", str(ROOT / "doom/port/video"),
                        "-I", str(ROOT / "doom/fcbus"),
                        str(ROOT / "doom/tools/render_native_status_panel.c"),
                        str(ROOT / "doom/port/video/fcvideo.c"),
                        str(ROOT / "doom/port/video/fcui.c"), "-lm",
                        "-o", str(panel_tool)], check=True)
        panel_file = output / "panel_pixels.bin"
        subprocess.run([str(panel_tool), str(panel_file), str(args.health),
                        str(args.armor), "20"], check=True)
        panel = np.frombuffer(panel_file.read_bytes(), dtype=np.uint8).reshape(240, 256)
        pixels[192:] = panel[192:]
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V4)
    mailbox[protocol.MBX_FLAGS] = (protocol.MBX_FLAG_V2 | protocol.MBX_FLAG_V3 |
                                   protocol.MBX_FLAG_V4 | protocol.MBX_FLAG_UI_VALID |
                                   protocol.MBX_FLAG_ATTR_VALID |
                                   protocol.MBX_FLAG_PAL_VALID)
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + 16] = (
        host[protocol.VRAM_MAILBOX_OFF_V2 + protocol.MBX_PAL:
             protocol.VRAM_MAILBOX_OFF_V2 + protocol.MBX_PAL + 16]
        if host else bytes((15, 0, 16, 48)) * 4)
    attributes = bytearray(
        host[protocol.VRAM_MAILBOX_OFF_V2 + protocol.MBX_ATTR:
             protocol.VRAM_MAILBOX_OFF_V2 + protocol.MBX_ATTR + 64]
        if host else bytes(64))
    if not args.no_status:
        mailbox[protocol.MBX_PAL + 12:protocol.MBX_PAL + 16] = bytes((15, 0, 48, 22))
        for by in range(12, 15):
            for bx in range(16):
                stream.attr_set(attributes, bx, by, 3)
    mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR + 64] = attributes
    ui = packet(args.health, args.armor, args.face, 0x09, 0x7F,
                visible=not args.no_status, menu=args.menu,
                selection=2 if args.menu else 0, menu_id=args.menu_id,
                stale_menu_id=args.stale_menu_id)
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
    if args.stale_menu_id and not args.menu:
        assert all(oam[n * 4] == 0xEF for n in range(35, 64)), \
            "closed menu left a sprite behind"
    if not args.no_status:
        for scanline in range(192, 240):
            visible = [n for n in range(64)
                       if oam[n * 4] + 1 <= scanline < oam[n * 4] + 9]
            assert len(visible) <= 8, (scanline, visible)
    sprite_palette = (run / "sprite_pal.bin").read_bytes()
    assert sprite_palette[1] == sprite_palette[3] == 0x16
    assert sprite_palette[2] == 0x0F
    assert sprite_palette[11] == 0x06
    assert sprite_palette[13:16] == bytes((0x12, 0x21, 0x28))
    sprite_chr = (run / "sprite_chr.bin").read_bytes()
    assert sprite_chr[:len(episode_pair_art()[0])] == episode_pair_art()[0]
    assert sprite_chr[0x130:0x130 + len(small_menu_logo_art())] == small_menu_logo_art()
    assert sprite_chr[0x600:0x600 + len(menu_logo_art())] == menu_logo_art()
    if not args.no_status:
        assert (run / "palette.bin").read_bytes()[15] == 0x16
        background_chr = (run / "background_chr.bin").read_bytes()
        for char, tile in bg_status_tiles():
            assert background_chr[char * 16:char * 16 + 16] == tile
        picture = Image.open(run / "final.png").convert("RGB")
        assert picture.getpixel((120, 210)) != picture.getpixel((100, 210))
    if args.menu:
        assert __import__("json").loads((run / "lua_result.json").read_text())["start_polls"] >= 1
        if not args.no_status:
            assert all(oam[n * 4] == 0xEF for n in range(21, 35)), \
                "paused menu should yield status sprite slots"
            if args.menu_id == 1:
                small = bytes(value for row in range(3) for col in range(7)
                              for value in (39 + row * 8, 0x13 + row * 7 + col,
                                            3, 100 + col * 8))
                assert oam[:84] == small
            else:
                assert all(oam[n * 4] == 0xEF for n in (0, 4, 8, 12))
                assert all(oam[n * 4] < 0xEF for n in range(16))
        else:
            if args.menu_id == 1:
                assert all(oam[n * 4] < 0xEF for n in range(32))
                assert oam[:128] == bytes(value for row in range(4) for col in range(8)
                                          for value in (15 + row * 8, 0x60 + row * 8 + col,
                                                        3, 96 + col * 8))
            else:
                assert all(oam[n * 4] == 0xEF for n in range(35))
        if args.menu_id == 3:
            assert all(oam[n * 4] == 0xEF for n in range(35, 64)), \
                "placeholder difficulty text still visible"
            print("original styled difficulty patches left to the Doom renderer")
            return 0
        expected_first = {1: (87, "N"), 2: (87, "E"), 3: (87, "E"),
                          4: (58, "E"), 5: (87, "E")}[args.menu_id]
        first_tile = (episode_pair_art()[1]["KN"] if args.menu_id == 2 else
                      ord(expected_first[1]))
        assert oam[35 * 4:35 * 4 + 4] == bytes((expected_first[0],
            first_tile, 0, 80 if args.menu_id >= 4 else 92))
        assert oam[63 * 4] == expected_first[0] + (64 if args.menu_id == 2 else 32)
        assert Image.open(run / "final.png").convert("RGB").getpixel((70, 100)) != (0, 0, 0)
        print("native sprite menu and status state survive Start")
        return 0
    art = resident_art()
    assert (run / "sprite_chr.bin").read_bytes()[0x800:0x800 + len(art)] == art
    assert sprite_palette[5:8] == bytes((0x07, 0x18, 0x26))
    assert (run / "ui_mailbox.bin").read_bytes() == ui
    # This fixed-frame fixture has no host-side follow-up VRAM commands.
    # The device queues the four black face-backing tiles over two conversions.
    assert (run / "text_row.bin").read_bytes()[2:30] == b" " * 28
    assert [oam[n * 4] for n in range(16)] == [199] * 4 + [207] * 4 + [215] * 4 + [223] * 4
    assert [oam[n * 4] for n in range(16, 22)] == [203] * 3 + [215] * 3
    assert [oam[n * 4] for n in range(22, 25)] == [199, 0xEF, 0xEF]
    face_group = (args.face - 34 if args.face >= 40 else
                  9 if args.face % 8 == 6 else
                  8 if args.face % 8 in (3, 4, 5) else
                  args.face if args.face < 3 else
                  5 if args.face >= 32 else
                  args.face // 8 + 2 if args.face >= 8 else 0)
    tile_map, flips = resident_face_map()
    assert [oam[n * 4 + 1] for n in range(16)] == list(
        tile_map[face_group * 16:face_group * 16 + 16])
    assert [oam[n * 4 + 2] for n in range(16)] == [
        value | 1 for value in flips[face_group * 16:face_group * 16 + 16]]
    print("native concrete status: percentages, 4x4 face, v4 count=15122")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
