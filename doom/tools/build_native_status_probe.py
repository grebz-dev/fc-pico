#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build a v4 ROM with tall sprite values and a 3x3 authentic Doomguy face."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess

from build_bg_text_probe import MESSAGE, text_source
from build_sprite_probe import ROOT, SOURCE, table
from native_status_art import (bg_status_tiles, episode_pair_art,
                               face_resident_index, menu_logo_art, resident_art)

STAMP = "20DOOM-04-9009"
STATUS_OAM_COUNT = 35
MENU_GLYPHS = 26
OAM_COUNT = 64
MENU_LAYOUTS = (
    (("NEW", "OPTIONS", "LOAD", "SAVE", "READ", "QUIT"), 87, 16, 92),
    (("", "", ""), 87, 32, 92),
    (("EASY", "NORMAL", "HARD", "ULTRA", "NIGHT"), 87, 16, 92),
    (("END GAME", "MESSAGE", "MOUSE", "", "SOUND"), 58, 16, 80),
    (("EFFECTS", "", "MUSIC"), 87, 16, 80),
)


def menu_oam(layout: tuple[tuple[str, ...], int, int, int]) -> bytes:
    labels, y0, step, x0 = layout
    data = bytearray()
    for row, label in enumerate(labels):
        assert sum(char != " " for char in label) <= 7
        for col, char in enumerate(label):
            if char != " ":
                data.extend((y0 + row * step, ord(char), 0, x0 + col * 8))
    assert len(data) <= MENU_GLYPHS * 4
    data.extend((0xEF, 0x20, 0, 0) * (MENU_GLYPHS - len(data) // 4))
    return bytes(data)


def episode_menu_oam() -> bytes:
    _, lookup, episodes = episode_pair_art()
    data = bytearray()
    for episode, lines in enumerate(episodes):
        for line_index, line in enumerate(lines):
            for column in range(0, len(line), 2):
                pair = (line + " ")[column:column + 2]
                data.extend((87 + episode * 32 + line_index * 8,
                             lookup[pair], 0, 92 + column * 4))
    assert len(data) <= MENU_GLYPHS * 4
    data.extend((0xEF, 0, 0, 0) * (MENU_GLYPHS - len(data) // 4))
    return bytes(data)


def logo_oam() -> bytes:
    return bytes(value for row in range(3) for column in range(8)
                 for value in (15 + row * 8, 0x60 + row * 8 + column,
                               3, 96 + column * 8))


def oam_template() -> bytes:
    data = bytearray()
    # Eight tall glyphs on each of two scanline bands.
    for half in (0, 1):
        for char, x in zip("H000R000", (16, 24, 32, 40, 176, 184, 192, 200)):
            glyph = "0123456789HR".index(char)
            data.extend((0xEF, 0x80 + glyph * 2 + half, 0, x))
    for row in range(3):
        for col in range(3):
            data.extend((0xEF, 0x98 + row * 3 + col, 1, 116 + col * 8))
    for char, x in zip("1234567", (16, 32, 48, 64, 176, 192, 208)):
        data.extend((0xEF, ord(char), 0, x))
    for char, x in zip("123", (16, 32, 48)):
        data.extend((0xEF, ord(char), 0, x))
    data.extend((0xEF, 0x20, 0, 0) * MENU_GLYPHS)
    data.extend((0xEF, ord(">"), 0, 76))
    data.extend((0xEF, 0, 0, 0) * (OAM_COUNT - STATUS_OAM_COUNT - MENU_GLYPHS - 1))
    assert len(data) == OAM_COUNT * 4
    return bytes(data)


def status_routine() -> str:
    lines = [
        "; Checked snapshot to 8x16 health/armor and a 3x3 face.",
        "UI_APPLY:",
        "        lda <MBX_ZP_LO+MBX_FLAGS", "        and #MBX_FLAG_UI_VALID",
        "        bne .packet", "        rts", ".packet:",
        "        lda $0300", "        cmp $0311", "        bne .new", "        rts", ".new:",
        "        lda #$A5", "        ldx #0", ".checksum:",
        "        eor $0300,x", "        inx", "        cpx #16", "        bne .checksum",
        "        cmp #0", "        beq .checked", "        rts", ".checked:",
        "        lda #0", "        sta $0310", "        lda $0300", "        sta $0311",
        "        lda $0301", "        and #$01", "        cmp #1", "        bne .hide",
        "        jmp .show", ".hide:",
        "        lda #$EF",
    ]
    lines += [f"        sta $02{n * 4:02X}" for n in range(OAM_COUNT)]
    lines += ["        jsr MENU_APPLY", "        lda #1", "        sta $0310", "        rts", ".show:",
              "        lda #191"]
    lines += [f"        sta $02{n * 4:02X}" for n in range(8)]
    lines += ["        lda #199"]
    lines += [f"        sta $02{n * 4:02X}" for n in range(8, 16)]
    for row in range(3):
        lines.append(f"        lda #{207 + row * 8}")
        lines += [f"        sta $02{n * 4:02X}"
                  for n in range(16 + row * 3, 19 + row * 3)]
    lines += [
        "        lda $0306", "        sta $12", "        lda $0307", "        sta $13",
        "        ldx #0", "        jsr UI_NUM3", "        lda $0308", "        sta $12",
        "        lda $0309", "        sta $13", "        ldx #16", "        jsr UI_NUM3",
        "        ldx $0302", "        cpx #42", "        bcc .face_valid",
        "        ldx #0", ".face_valid:",
        "        lda face_tile_base_data,x", "        sta $14",
    ]
    for index in range(9):
        lines += ["        lda $14", "        clc", f"        adc #{index}",
                  f"        sta $02{(16 + index) * 4 + 1:02X}"]
    for i in range(7):
        offset = (25 + i) * 4
        mask = 1 << i  # weapon 1..7 in snapshot bits 0..6
        y = 207 if i < 5 else 215
        lines += ["        lda $0304", f"        and #${mask:02X}",
                  f"        beq .weapon_hide_{i}", f"        lda #{y}",
                  f"        sta $02{offset:02X}",
                  "        lda $0305", "        and #$0F", f"        cmp #{i + 1}",
                  f"        bne .weapon_normal_{i}", "        lda #2",
                  f"        sta $02{offset + 2:02X}", f"        jmp .weapon_done_{i}",
                  f".weapon_normal_{i}:", "        lda #0",
                  f"        sta $02{offset + 2:02X}", f"        jmp .weapon_done_{i}",
                  f".weapon_hide_{i}:", "        lda #$EF",
                  f"        sta $02{offset:02X}", f".weapon_done_{i}:"]
    for i in range(3):
        offset = (32 + i) * 4
        mask = (1 << i) | (1 << (i + 3))
        lines += ["        lda $0303", f"        and #${mask:02X}",
                  f"        beq .key_hide_{i}", "        lda #215",
                  f"        sta $02{offset:02X}", f"        jmp .key_done_{i}",
                  f".key_hide_{i}:", "        lda #$EF",
                  f"        sta $02{offset:02X}", f".key_done_{i}:"]
    lines += [
        "        jsr MENU_APPLY",
        "        lda #1", "        sta $0310", "        rts", "",
        "; Copy up to 26 menu glyphs from resident ROM, then place cursor.",
        "MENU_APPLY:",
        "        lda #$EF", "        ldx #0", ".hide_menu:",
        "        sta $028C,x", "        inx", "        inx", "        inx", "        inx",
        "        cpx #108", "        bne .hide_menu",
        "        lda $0301", "        and #$04", "        beq .menu_done",
        "        lda $0305", "        lsr a", "        lsr a", "        lsr a", "        lsr a",
        "        beq .menu_done", "        cmp #6", "        bcs .menu_done",
        "        sec", "        sbc #1", "        tax",
        "        lda menu_ptr_lo,x", "        sta $16",
        "        lda menu_ptr_hi,x", "        sta $17",
        "        lda menu_first_y,x", "        sta $18",
        "        lda menu_cursor_x,x", "        sta $19",
        "        ldy #0", ".copy_menu:",
        "        lda [$16],y", "        sta $028C,y", "        iny",
        "        cpy #104", "        bne .copy_menu",
        "        lda $0301", "        lsr a", "        lsr a", "        lsr a", "        lsr a",
        "        and #7", "        asl a", "        asl a", "        asl a", "        asl a",
        "        sta $1A", "        lda menu_cursor_step,x", "        cmp #32",
        "        bne .cursor_base", "        asl $1A", ".cursor_base:",
        "        lda $1A", "        clc", "        adc $18", "        sta $02F4",
        "        lda $19", "        sta $02F7",
        ".menu_done:",
        "        lda $0301", "        and #1", "        bne .no_logo",
        "        lda $0305", "        lsr a", "        lsr a", "        lsr a", "        lsr a",
        "        cmp #1", "        bne .no_logo", "        ldx #0",
        ".copy_logo:", "        lda logo_oam_data,x", "        sta $0200,x",
        "        inx", "        cpx #96", "        bne .copy_logo",
        ".no_logo:", "        rts", "",
        "; X selects the health (0) or armor (16) OAM byte offset.",
        "UI_NUM3:", "        ldy #0", ".hundreds:",
        "        lda $13", "        bne .subtract100", "        lda $12",
        "        cmp #100", "        bcc .tens", ".subtract100:",
        "        sec", "        lda $12", "        sbc #100", "        sta $12",
        "        lda $13", "        sbc #0", "        sta $13", "        iny",
        "        cpy #10", "        bne .hundreds", ".tens:",
        "        tya", "        asl a", "        clc", "        adc #$80",
        "        sta $0205,x", "        clc", "        adc #1", "        sta $0225,x",
        "        ldy #0", ".tenloop:", "        lda $12", "        cmp #10",
        "        bcc .ones", "        sec", "        sbc #10", "        sta $12",
        "        iny", "        bne .tenloop", ".ones:",
        "        tya", "        asl a", "        clc", "        adc #$80",
        "        sta $0209,x", "        clc", "        adc #1", "        sta $0229,x",
        "        lda $12", "        asl a", "        clc", "        adc #$80",
        "        sta $020D,x", "        clc", "        adc #1", "        sta $022D,x",
        "        rts", "",
    ]
    return "\n".join(lines) + "\n"


def native_status_source() -> str:
    source = text_source()
    old_message = "native_text_data:\n        db " + ",".join(f"${v:02X}" for v in MESSAGE)
    if source.count(old_message) != 1 or source.count("20DOOM-04-9001") != 1:
        raise ValueError("v4 text source changed")
    source = source.replace("20DOOM-04-9001", STAMP)
    source = source.replace(old_message,
                            "native_text_data:\n        db " + ",".join("$20" for _ in MESSAGE))
    start = source.index("; A minimal dynamic HUD slice:")
    end = source.index("native_text_data:\n", start)
    source = source[:start] + status_routine() + source[end:]
    setup_anchor = "        lda #$FF\n        sta $0311\n"
    if source.count(setup_anchor) != 1:
        raise ValueError("no unique HUD setup point")
    setup = [
        "        ; Resident tall digits and eleven 3x3 face states.",
        "        lda #$18", "        sta $2006", "        lda #0", "        sta $2006",
    ]
    art = resident_art()
    for page in range((len(art) + 255) // 256):
        size = min(256, len(art) - page * 256)
        setup += ["        ldx #0", f".chr_page_{page}:",
                  f"        lda native_chr_data+${page * 256:04X},x",
                  "        sta $2007", "        inx"]
        if size == 256:
            setup += [f"        bne .chr_page_{page}"]
        else:
            setup += [f"        cpx #{size}", f"        bne .chr_page_{page}"]
    for label, data, address in (
        ("episode", episode_pair_art()[0], 0x1000),
        ("logo", menu_logo_art(), 0x1600),
    ):
        setup += [f"        lda #${address >> 8:02X}", "        sta $2006",
                  f"        lda #${address & 0xFF:02X}", "        sta $2006"]
        for page in range((len(data) + 255) // 256):
            size = min(256, len(data) - page * 256)
            setup += ["        ldx #0", f".{label}_page_{page}:",
                      f"        lda native_{label}_chr_data+${page * 256:04X},x",
                      "        sta $2007", "        inx"]
            setup += ([f"        bne .{label}_page_{page}"] if size == 256 else
                      [f"        cpx #{size}", f"        bne .{label}_page_{page}"])
    bg_tiles = bg_status_tiles()
    for index, (char, _) in enumerate(bg_tiles):
        address = char * 16
        setup += [f"        lda #${address >> 8:02X}", "        sta $2006",
                  f"        lda #${address & 0xFF:02X}", "        sta $2006",
                  "        ldx #0", f".bg_tile_{index}:",
                  f"        lda native_bg_text_data+${index * 16:04X},x",
                  "        sta $2007", "        inx", "        cpx #16",
                  f"        bne .bg_tile_{index}"]
    setup += [
        "        ldx #0", ".native_oam:", "        lda native_oam_data,x",
        "        sta $0200,x", "        inx", "        bne .native_oam",
        "        lda #$3F", "        sta $2006",
        "        lda #$11", "        sta $2006", "        lda #$16", "        sta $2007",
        "        lda #$3F", "        sta $2006",
        "        lda #$12", "        sta $2006", "        lda #$0F", "        sta $2007",
        "        lda #$3F", "        sta $2006", "        lda #$15", "        sta $2006",
        "        lda #$07", "        sta $2007", "        lda #$18", "        sta $2007",
        "        lda #$26", "        sta $2007",
        "        lda #$3F", "        sta $2006", "        lda #$1B", "        sta $2006",
        "        lda #$06", "        sta $2007",
        "        lda #$3F", "        sta $2006",
        "        lda #$1D", "        sta $2006",
        "        lda #$06", "        sta $2007",
        "        lda #$16", "        sta $2007",
        "        lda #$28", "        sta $2007",
    ]
    source = source.replace(setup_anchor, setup_anchor + "\n".join(setup) + "\n")
    font_palette = ("        lda #$13\n        sta $2006\n"
                    "        lda #$30\n        sta $2007")
    if source.count(font_palette) != 1:
        raise ValueError("no unique inherited sprite font palette")
    source = source.replace(font_palette, font_palette.replace("#$30", "#$16"))
    command_anchor = ".valid:\n"
    if source.count(command_anchor) != 1:
        raise ValueError("no unique v4 NMI command point")
    commands = '''.valid:
        ; Two native row pokes per frame. Defer OAM and BG tables on this NMI;
        ; the complete shadow remains ready for the next NMI.
        lda <MBX_ZP_LO+MBX_CMD
        cmp #$C0
        bcc .ui_no_text
        and #$3F
        sta $2006
        lda <MBX_ZP_LO+MBX_CMD+1
        sta $2006
        lda <MBX_ZP_LO+MBX_CMD+2
        sta $2007
        lda <MBX_ZP_LO+MBX_CMD+3
        cmp #$C0
        bcs .second_text
        jmp .no_pal
.second_text:
        and #$3F
        sta $2006
        lda <MBX_ZP_LO+MBX_CMD+4
        sta $2006
        lda <MBX_ZP_LO+MBX_CMD+5
        sta $2007
        jmp .no_pal
.ui_no_text:
'''
    source = source.replace(command_anchor, commands)
    anchor = "native_text_data:\n"
    if source.count(anchor) != 1:
        raise ValueError("no unique data point")
    face_bases = bytes(0x98 + 9 * face_resident_index(index)
                       for index in range(42))
    menu_data = "".join(table(f"menu_oam_{index}",
                              episode_menu_oam() if index == 1 else menu_oam(layout))
                        for index, layout in enumerate(MENU_LAYOUTS))
    menu_data += table("menu_ptr_lo", bytes((0,)) * len(MENU_LAYOUTS)).replace(
        "        db " + ",".join("$00" for _ in MENU_LAYOUTS),
        "        db " + ",".join(f"LOW(menu_oam_{i})" for i in range(len(MENU_LAYOUTS))))
    menu_data += table("menu_ptr_hi", bytes((0,)) * len(MENU_LAYOUTS)).replace(
        "        db " + ",".join("$00" for _ in MENU_LAYOUTS),
        "        db " + ",".join(f"HIGH(menu_oam_{i})" for i in range(len(MENU_LAYOUTS))))
    menu_data += table("menu_first_y", bytes(layout[1] for layout in MENU_LAYOUTS))
    menu_data += table("menu_cursor_step", bytes(layout[2] for layout in MENU_LAYOUTS))
    menu_data += table("menu_cursor_x", bytes((76, 76, 76, 60, 60)))
    source = source.replace(anchor,
        table("native_chr_data", art) + table("native_oam_data", oam_template()) +
        table("native_episode_chr_data", episode_pair_art()[0]) +
        table("native_logo_chr_data", menu_logo_art()) +
        table("logo_oam_data", logo_oam()) +
        table("native_bg_text_data", b"".join(tile for _, tile in bg_tiles)) +
        table("face_tile_base_data", face_bases) + menu_data + "\n" + anchor)
    return source


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--nesasm", type=Path, default=ROOT / ".build/tools/nesasm/nesasm")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    temporary = SOURCE.parent / "PG_native_status_tmp.asm"
    try:
        temporary.write_text(native_status_source())
        subprocess.run([str(args.nesasm.resolve()), "-s", "-f", "-o",
                        str(output / "probe.raw.nes"), temporary.name],
                       cwd=SOURCE.parent, check=True)
    finally:
        temporary.unlink(missing_ok=True)
    subprocess.run(["python3", str(ROOT / "doom/tools/nes/normalize_ines.py"),
                    str(output / "probe.raw.nes"), str(output / "probe.nes")], check=True)
    subprocess.run(["python3", str(ROOT / "doom/tools/nes/check_fixbank.py"),
                    str(output / "probe.nes"),
                    str(ROOT / "tutorial_project/BOOTROM/bootrom_fixr.bin")], check=True)
    (output / "sprite_chr.bin").write_bytes(resident_art())
    (output / "oam_template.bin").write_bytes(oam_template())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
