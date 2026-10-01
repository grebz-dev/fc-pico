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
from native_status_art import (bg_status_tiles,
                               menu_logo_art, resident_art,
                               resident_face_map)  # noqa: E402


def packet(health: int, armor: int, face: int, keys: int, weapons: int,
           visible: bool = True, menu: bool = False, selection: int = 0,
           menu_id: int = 1, stale_menu_id: bool = False,
           ready_weapon: int = 2) -> bytes:
    ui = bytearray(16)
    ui[0] = 1
    ui[1] = int(visible) | (4 if menu else 0) | (selection << 4)
    ui[2] = face
    ui[3] = keys
    ui[4] = weapons
    ui[5] = ready_weapon | (menu_id << 4 if menu or stale_menu_id else 0)
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
    parser.add_argument("--keys", type=lambda value: int(value, 0), default=0x09)
    parser.add_argument("--ready-weapon", type=int, default=2)
    parser.add_argument("--shells", type=int, default=20)
    parser.add_argument("--bullets", type=int, default=60)
    parser.add_argument("--cells", type=int, default=100)
    parser.add_argument("--rockets", type=int, default=4)
    parser.add_argument("--menu", action="store_true")
    parser.add_argument("--menu-id", type=int, default=1)
    parser.add_argument("--no-status", action="store_true")
    parser.add_argument("--stale-menu-id", action="store_true")
    parser.add_argument("--close-menu-frame", type=int,
                        help="capture menu open, then force a checked gameplay packet")
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
                        str(args.armor), str(args.shells), str(args.ready_weapon),
                        str(args.bullets), str(args.cells), str(args.rockets)], check=True)
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
    ui = packet(args.health, args.armor, args.face, args.keys, 0x7F,
                visible=not args.no_status, menu=args.menu,
                selection=2 if args.menu else 0, menu_id=args.menu_id,
                stale_menu_id=args.stale_menu_id,
                ready_weapon=args.ready_weapon)
    mailbox[protocol.MBX_UI:protocol.MBX_UI + 16] = ui
    frame = output / "frame.bin"
    frame.write_bytes(stream.encode_frame(pixels, mailbox, native_text=True))
    if args.menu and args.menu_id == 2:
        overlay = output / "overlay_native_episode"
        subprocess.run(["cc", "-O2", "-I", str(ROOT / "doom/port/video"),
                        "-I", str(ROOT / "doom/fcbus"),
                        str(ROOT / "doom/tools/overlay_native_episode.c"),
                        str(ROOT / "doom/port/video/fcvideo.c"),
                        str(ROOT / "doom/port/video/fcui.c"), "-lm",
                        "-o", str(overlay)], check=True)
        subprocess.run([str(overlay), str(frame)], check=True)
    closed = bytearray(packet(args.health, args.armor, args.face, args.keys, 0x7F,
                              ready_weapon=args.ready_weapon))
    closed[0] = 2
    closed[15] ^= 3  # XOR checksum includes generation (1 -> 2).
    subprocess.run([sys.executable, str(HERE / "run_doom_frame.py"), str(frame),
                    "--rom", str(output / "probe.nes"),
                    "--output", str(output / "mesen"),
                    *(["--min-correlation", "0.85"] if args.menu else []),
                    *(["--press-start-frame", "155"] if args.menu else []),
                    *(["--ui-transition-frame", str(args.close_menu_frame),
                       "--ui-transition-packet", closed.hex()]
                      if args.close_menu_frame is not None else [])], check=True)
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
    assert sprite_palette[9:12] == bytes((0x12, 0x22, 0x38))
    assert sprite_palette[13:16] == bytes((0x12, 0x38, 0x16))
    sprite_chr = (run / "sprite_chr.bin").read_bytes()
    assert sprite_chr[0x600:0x600 + len(menu_logo_art())] == menu_logo_art()
    if not args.no_status:
        assert (run / "palette.bin").read_bytes()[15] == 0x16
        background_chr = (run / "background_chr.bin").read_bytes()
        for char, tile in bg_status_tiles():
            assert background_chr[char * 16:char * 16 + 16] == tile
        picture = Image.open(run / "final.png").convert("RGB")
        assert picture.getpixel((120, 210)) != picture.getpixel((100, 210))
        rgb = np.asarray(picture)
        cell = rgb[225:232, 195:220]
        assert np.count_nonzero(np.all(cell > 180, axis=2)) >= 40, "CELL label missing"
        plate = rgb[199:221, 76:108]
        assert np.count_nonzero((plate[:, :, 0] > 180) &
                                (plate[:, :, 1] > 180) &
                                (plate[:, :, 2] < 180)) >= 15, "yellow ARMS plate missing"
    if args.menu and args.close_menu_frame is None:
        assert __import__("json").loads((run / "lua_result.json").read_text())["start_polls"] >= 1
        if not args.no_status:
            assert all(oam[n * 4] == 0xEF for n in range(32, 35)), \
                "paused menu should yield status sprite slots"
            if args.menu_id == 1:
                large = bytes(value for row in range(4) for col in range(8)
                              for value in (15 + row * 8, 0x60 + row * 8 + col,
                                            2, 96 + col * 8))
                assert oam[:128] == large
            else:
                assert all(oam[n * 4] < 0xEF for n in range(16))
        else:
            if args.menu_id == 1:
                assert all(oam[n * 4] < 0xEF for n in range(32))
                assert oam[:128] == bytes(value for row in range(4) for col in range(8)
                                          for value in (15 + row * 8, 0x60 + row * 8 + col,
                                                        2, 96 + col * 8))
            else:
                assert all(oam[n * 4] == 0xEF for n in range(35))
        if args.menu_id == 3:
            assert all(oam[n * 4] == 0xEF for n in range(35, 64)), \
                "placeholder difficulty text still visible"
            print("original styled difficulty patches left to the Doom renderer")
            return 0
        expected_first = {1: (87, "N"), 2: (87, "E"), 3: (87, "E"),
                          4: (58, "E"), 5: (87, "E")}[args.menu_id]
        first_tile = ord(expected_first[1])
        if args.menu_id == 2:
            assert all(oam[n * 4] == 0xEF for n in range(35, 63))
            picture = np.asarray(Image.open(run / "final.png").convert("RGB"))
            for y in (88, 120, 152):
                band = picture[y:y + 9, 40:216]
                assert np.count_nonzero((band[:, :, 0] > 110) &
                                        (band[:, :, 0] > band[:, :, 1] * 1.25)) > 80
        else:
            assert oam[35 * 4:35 * 4 + 4] == bytes((expected_first[0],
                first_tile, 0, 80 if args.menu_id >= 4 else 92))
        if args.menu_id == 4:
            assert [oam[(35 + i) * 4] for i in range(19)] == [58] * 7 + [74] * 7 + [90] * 5
        assert oam[63 * 4] == expected_first[0] + (64 if args.menu_id == 2 else 32)
        assert Image.open(run / "final.png").convert("RGB").getpixel((70, 100)) != (0, 0, 0)
        print("native sprite menu and status state survive Start")
        return 0
    art = resident_art()
    assert (run / "sprite_chr.bin").read_bytes()[0x800:0x800 + len(art)] == art
    assert sprite_palette[5:8] == bytes((0x07, 0x18, 0x26))
    assert (run / "ui_mailbox.bin").read_bytes() == (closed if args.close_menu_frame else ui)
    # This fixed-frame fixture has no host-side follow-up VRAM commands.
    # The device queues the four black face-backing tiles over two conversions.
    assert (run / "text_row.bin").read_bytes()[2:30] == b" " * 28
    assert [oam[n * 4] for n in range(16)] == [199] * 4 + [207] * 4 + [215] * 4 + [223] * 4
    assert [oam[n * 4 + 3] for n in range(16)] == [112, 120, 128, 136] * 4
    assert [oam[n * 4] for n in range(16, 22)] == [
        (200 if i < 3 else 212) if args.ready_weapon == i + 1 else 0xEF
        for i in range(6)]
    selected = args.ready_weapon - 1
    if 0 <= selected < 6:
        assert oam[(16 + selected) * 4 + 1:(16 + selected) * 4 + 4] == bytes(
            (0x32 + selected, 3, 78 + selected % 3 * 10))
    assert [oam[n * 4] for n in range(22, 25)] == [
        200 + i * 12 if args.keys & ((1 << i) | (1 << (i + 3))) else 0xEF
        for i in range(3)]
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
    if args.close_menu_frame is not None:
        open_oam = (run / "menu-open-oam.bin").read_bytes()
        assert open_oam[:128] == bytes(value for row in range(4) for col in range(8)
                                        for value in (15 + row * 8,
                                                      0x60 + row * 8 + col,
                                                      2, 96 + col * 8))
        assert open_oam[35 * 4] == 87
        logo = np.asarray(Image.open(run / "menu-open.png").convert("RGB"))[16:48, 96:160]
        assert np.count_nonzero((logo[:, :, 0] > 170) &
                                (logo[:, :, 1] > 170) &
                                (logo[:, :, 2] < 180)) > 500, "logo lost yellow art"
        assert np.count_nonzero((logo[:, :, 2] > logo[:, :, 0] * 1.3) &
                                (logo[:, :, 2] > 100)) > 500, "logo lost blue art"
        assert not np.count_nonzero((logo[:, :, 0] > 120) &
                                    (logo[:, :, 0] > logo[:, :, 1] * 1.4) &
                                    (logo[:, :, 0] > logo[:, :, 2] * 1.4)), \
            "logo still uses red"
        print("menu close restores face geometry and yellow ARMS plate")
    print("native concrete status: percentages, 4x4 face, v4 count=15122")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
