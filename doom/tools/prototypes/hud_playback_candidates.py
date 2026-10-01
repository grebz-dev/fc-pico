#!/usr/bin/env python3
"""Throwaway Mesen playback of three cleaned HUD variants and three states each.

Run: doom/.venv/bin/python doom/tools/prototypes/hud_playback_candidates.py
Each ROM changes its own nametable and OAM at frames 60 and 120; Lua captures
the settled states at frames 30, 90 and 150.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "doom/tools"))

from native_status_art import GLYPHS  # noqa: E402
from hud_reference_variants import (ARMS, FACE, KEYS, MESEN, NESASM, PANEL,
                                    REF, RGB, SKIN, SpriteAtlas, encode_tile,
                                    nearest)  # noqa: E402

OUT = ROOT / "doom/assets/hud_playback_candidates"
FACE_META = json.loads((ROOT / "doom/assets/doomguy_faces.json").read_text())
FACES = {name: i for i, name in enumerate(FACE_META["order"])}
PERCENT = ("##..#", "##.#.", "...#.", "..#..", ".#...", ".#.##", "#..##")
SLASH = ("....#", "....#", "...#.", "..#..", ".#...", "#....", "#....")
FONT = {**GLYPHS, "%": PERCENT, "/": SLASH}
SMALL = {
    "0": ("###", "#.#", "#.#", "#.#", "###"),
    "1": (".#.", "##.", ".#.", ".#.", "###"),
    "2": ("###", "..#", "###", "#..", "###"),
    "3": ("###", "..#", ".##", "..#", "###"),
    "4": ("#.#", "#.#", "###", "..#", "..#"),
    "5": ("###", "#..", "###", "..#", "###"),
    "6": ("###", "#..", "###", "#.#", "###"),
    "7": ("###", "..#", "..#", ".#.", ".#."),
    "8": ("###", "#.#", "###", "#.#", "###"),
    "9": ("###", "#.#", "###", "..#", "###"),
    "/": ("..#", "..#", ".#.", "#..", "#.."),
}

STATES = [
    dict(label="healthy", ammo=20, health=100, armor=75, weapon=2,
         keys=(True, True, True), face="STFST00",
         inventory=((20, 200), (18, 50), (4, 50), (100, 300))),
    dict(label="hurt", ammo=7, health=67, armor=46, weapon=3,
         keys=(True, False, False), face="STFOUCH2",
         inventory=((63, 200), (7, 50), (5, 50), (40, 300))),
    dict(label="pickup", ammo=124, health=23, armor=12, weapon=6,
         keys=(True, True, False), face="STFEVL3",
         inventory=((153, 400), (0, 100), (18, 100), (124, 600))),
]

KEY_SHAPE = np.asarray([
    [0, 0, 1, 1, 1, 1, 0, 0],
    [0, 1, 1, 1, 1, 1, 1, 0],
    [0, 1, 1, 1, 1, 1, 1, 0],
    [0, 0, 0, 1, 1, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 0, 0],
    [0, 0, 0, 1, 1, 0, 0, 0],
    [0, 0, 0, 1, 1, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0],
], dtype=np.uint8)


def glyph(ch: str) -> np.ndarray:
    return np.asarray([[1 if p == "#" else 0 for p in row]
                       for row in FONT[ch]], dtype=np.uint8)


def face(name: str) -> np.ndarray:
    source = np.asarray(Image.open(FACE).convert("RGB"))
    i = FACES[name]
    y, x = divmod(i, 7)
    crop = source[y * 24:y * 24 + 24, x * 24:x * 24 + 24]
    colors = {(0, 0, 0): 0, (131, 31, 31): 1,
              (135, 87, 51): 2, (207, 131, 83): 3}
    indexed = np.zeros((24, 24), dtype=np.uint8)
    for rgb, index in colors.items():
        indexed[np.all(crop == rgb, axis=2)] = index
    return np.asarray(Image.fromarray(indexed).resize((32, 40), Image.Resampling.NEAREST))


def draw_big(bg: np.ndarray, text: str, x: int, y: int) -> None:
    for i, ch in enumerate(text):
        g = glyph(ch)
        for yy in range(7):
            for xx in range(5):
                if g[yy, xx]:
                    px, py = x + i * 8 + xx, y + yy * 2
                    if px + 1 < 256 and py + 2 < 240:
                        bg[py + 1:py + 3, px + 1] = 0  # lower-right outline
                    bg[py:py + 2, px] = 3


def draw_inventory(bg: np.ndarray, pair: tuple[int, int], y: int) -> None:
    current, capacity = pair
    text = f"{current}/{capacity}"
    x = 255 - len(text) * 4
    for ch in text:
        small = np.asarray([[v == "#" for v in row] for row in SMALL[ch]])
        color = 3 if ch == "/" else 2
        bg[y:y + 5, x:x + 3][small != 0] = color
        x += 4


def draw_arms(bg: np.ndarray, atlas: SpriteAtlas, selected: int,
              style: str) -> None:
    for row in range(2):
        for col in range(3):
            number = 2 + row * 3 + col
            x, y = 77 + col * 10, 199 + row * 12
            # Identical 9x10 wells for all six weapons.
            bg[y:y + 10, x:x + 9] = 0
            bg[y, x:x + 9] = 2
            bg[y + 9, x:x + 9] = 2
            bg[y:y + 10, x] = 2
            bg[y:y + 10, x + 8] = 2
            digit = glyph(str(number))
            center_x, center_y = x + 2, y + 1
            if number != selected or style == "border":
                bg[center_y:center_y + 7, center_x:center_x + 5][digit != 0] = 2
            if number == selected:
                sprite = np.zeros((8, 8), dtype=np.uint8)
                if style == "digit":
                    sprite[:7, :5][digit != 0] = 3
                    atlas.add(sprite, center_x, center_y, 2)
                elif style == "plate":
                    sprite[:, :] = 3
                    sprite[:7, :5][digit != 0] = 0  # black BG numeral hole
                    atlas.add(sprite, x + 1, y + 1, 2)
                elif style == "border":
                    sprite[0, :] = 3
                    sprite[7, :] = 3
                    sprite[:, 0] = 3
                    sprite[:, 7] = 3
                    atlas.add(sprite, x + 1, y + 1, 2)
                else:
                    raise ValueError(style)


def build_state(target: np.ndarray, atlas: SpriteAtlas,
                state: dict, style: str) -> tuple[np.ndarray, np.ndarray]:
    bg = nearest(target, PANEL)
    yy, xx = np.indices((192, 256))
    bg[:192] = np.where(((xx // 16 + yy // 16) & 1) == 0, 2, 1)
    # Clear the changing artwork while preserving the panel borders and labels.
    for x0, x1 in ((4, 31), (35, 74), (148, 182)):
        bg[198:222, x0:x1] = 1
    bg[196:237, 112:144] = 0
    bg[198:221, 76:107] = 1
    bg[198:238, 183:194] = 1
    bg[193:228, 220:255] = 1
    bg[225:238, 195:255] = 1
    draw_big(bg, str(state["ammo"]), 5, 199)
    draw_big(bg, f'{state["health"]}%', 36, 199)
    draw_big(bg, f'{state["armor"]}%', 149, 199)
    for y, values in zip((196, 203, 210, 217), state["inventory"]):
        draw_inventory(bg, values, y)
    draw_arms(bg, atlas, state["weapon"], style)
    atlas.add(face(state["face"]), 112, 196, 1)
    for i, available in enumerate(state["keys"]):
        y = 199 + i * 12
        bg[y:y + 10, 183:193] = 0
        bg[y, 183:193] = 2
        bg[y + 9, 183:193] = 2
        bg[y:y + 10, 183] = 2
        bg[y:y + 10, 192] = 2
        if available:
            key = KEY_SHAPE * (i + 1)
            atlas.add(key, 184, y + 1, 3)
    # This is a one-palette HUD: all 16x16 attributes select palette 0.
    attributes = np.zeros((15, 16), dtype=np.uint8)
    return bg, attributes


def pack_backgrounds(frames: list[np.ndarray], attrs: np.ndarray) -> tuple[bytes, list[bytes], int, dict]:
    tiles = [bytes(16)]
    known = {bytes(16): 0}
    tables: list[bytes] = []
    attribute_data = bytearray(64)  # all palette 0
    for frame in frames:
        names = bytearray(960)
        for ty in range(30):
            for tx in range(32):
                key = encode_tile(frame[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
                if key not in known:
                    known[key] = len(tiles)
                    tiles.append(key)
                names[ty * 32 + tx] = known[key]
        tables.append(bytes(names + attribute_data))
    # Keep every numeric glyph resident, including values absent from the
    # three screenshots. The prototype's streamed nametable may still need to
    # compose boundary-crossing glyph tiles for arbitrary live values.
    font_tiles: dict[str, list[int]] = {}
    for ch in "0123456789%/":
        stamp = np.ones((16, 8), dtype=np.uint8)
        draw_big(stamp, ch, 0, 0)
        indices = []
        for half in (stamp[:8], stamp[8:]):
            key = encode_tile(half)
            if key not in known:
                known[key] = len(tiles)
                tiles.append(key)
            indices.append(known[key])
        font_tiles[f"large:{ch}"] = indices
    for ch in "0123456789/":
        tile = np.ones((8, 8), dtype=np.uint8)
        small = np.asarray([[v == "#" for v in row] for row in SMALL[ch]])
        tile[:5, :3][small] = 3 if ch == "/" else 2
        key = encode_tile(tile)
        if key not in known:
            known[key] = len(tiles)
            tiles.append(key)
        font_tiles[f"small:{ch}"] = [known[key]]
    if len(tiles) > 256:
        raise ValueError(f"background needs {len(tiles)} CHR tiles")
    return b"".join(tiles).ljust(4096, b"\0"), tables, len(tiles), font_tiles


ASM = """.inesprg 2
.ineschr 1
.inesmap 0
.inesmir 1
.bank 0
.org $8000
reset:
 sei
 cld
 ldx #$40
 stx $4017
 ldx #$ff
 txs
 ldx #$00
 stx $2000
 stx $2001
 stx $4010
 stx frame_count
 stx state
 stx pending
wait1:
 bit $2002
 bpl wait1
wait2:
 bit $2002
 bpl wait2
 jsr upload_state
 lda #$88
 sta $2000
 lda #$1e
 sta $2001
main:
 lda pending
 beq main
 lda #$00
 sta $2001
 jsr upload_state
 lda #$00
 sta $2005
 sta $2005
 sta pending
 lda #$88
 sta $2000
 lda #$1e
 sta $2001
 jmp main
nmi:
 pha
 txa
 pha
 tya
 pha
 inc frame_count
 lda frame_count
 cmp #60
 bne nmi120
 lda #1
 sta state
 sta pending
nmi120:
 lda frame_count
 cmp #120
 bne nmi_done
 lda #2
 sta state
 lda #1
 sta pending
nmi_done:
 pla
 tay
 pla
 tax
 pla
 rti
irq:
 rti
upload_state:
 lda #$20
 sta $2006
 lda #$00
 sta $2006
 ldx state
 lda nt_lo,x
 sta $00
 lda nt_hi,x
 sta $01
 ldx #4
 ldy #0
copy_page:
 lda [$00],y
 sta $2007
 iny
 bne copy_page
 inc $01
 dex
 bne copy_page
 lda #$3f
 sta $2006
 lda #$00
 sta $2006
 ldx #0
palette_loop:
 lda palettes,x
 sta $2007
 inx
 cpx #$20
 bne palette_loop
 ldx state
 lda oam_lo,x
 sta $00
 lda oam_hi,x
 sta $01
 ldy #0
copy_oam:
 lda [$00],y
 sta $0200,y
 iny
 bne copy_oam
 lda #0
 sta $2003
 lda #$02
 sta $4014
 lda #0
 sta $2005
 sta $2005
 rts
nt_lo:
 .db LOW(nt0),LOW(nt1),LOW(nt2)
nt_hi:
 .db HIGH(nt0),HIGH(nt1),HIGH(nt2)
oam_lo:
 .db LOW(oam0),LOW(oam1),LOW(oam2)
oam_hi:
 .db HIGH(oam0),HIGH(oam1),HIGH(oam2)
frame_count = $10
state = $11
pending = $12
.bank 1
.org $a000
nt0:
 .incbin \"state-1.nt\"
nt1:
 .incbin \"state-2.nt\"
nt2:
 .incbin \"state-3.nt\"
oam0:
 .incbin \"state-1.oam\"
oam1:
 .incbin \"state-2.oam\"
oam2:
 .incbin \"state-3.oam\"
palettes:
 .incbin \"palettes.bin\"
.bank 3
.org $fffa
 .dw nmi
 .dw reset
 .dw irq
.bank 4
.org $0000
 .incbin \"chr.bin\"
"""


def write_rom(folder: Path, chr_data: bytes, tables: list[bytes],
              oams: list[bytes]) -> Path:
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "chr.bin").write_bytes(chr_data)
    for i, (nt, oam) in enumerate(zip(tables, oams), 1):
        (folder / f"state-{i}.nt").write_bytes(nt)
        (folder / f"state-{i}.oam").write_bytes(oam)
    palettes = [PANEL, SKIN, ARMS, KEYS]
    (folder / "palettes.bin").write_bytes(bytes(sum(palettes, []) * 2))
    source = "\n".join(" " + line if line.startswith(".") else line
                       for line in ASM.splitlines()) + "\n"
    (folder / "playback.asm").write_text(source)
    result = subprocess.run([str(NESASM), "playback.asm"], cwd=folder,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return folder / "playback.nes"


def capture(rom: Path, folder: Path, staging: Path) -> None:
    env = dict(os.environ, HUD_PLAYBACK_OUTPUT=str(folder),
               SDL_AUDIODRIVER="dummy")
    dotnet = Path.home() / ".dotnet"
    if "DOTNET_ROOT" not in env and (dotnet / "host/fxr").is_dir():
        env["DOTNET_ROOT"] = str(dotnet)
    with (folder / "mesen.log").open("w") as log:
        result = subprocess.run([str(staging / MESEN.name), "--testRunner",
                                 str(OUT / "capture.lua"), str(rom), "--timeout=45"],
                                cwd=folder, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=60)
    if result.returncode:
        raise RuntimeError((folder / "mesen.log").read_text())
    if (folder / "mesen.log").stat().st_size == 0:
        (folder / "mesen.log").unlink()
    for i in range(1, 4):
        path = folder / f"frame-{i:02d}.png"
        if not path.is_file():
            raise RuntimeError(f"Mesen did not capture {path}")
        image = Image.open(path).convert("RGB")
        if image.size != (256, 240):
            raise ValueError(f"bad capture dimensions: {image.size}")
        image.crop((0, 192, 256, 240)).resize((1024, 192), Image.Resampling.NEAREST).save(
            folder / f"hud-{i:02d}-4x.png")


def comparison() -> None:
    sheet = Image.new("RGB", (3 * 512, 3 * 132), (25, 25, 25))
    pen = ImageDraw.Draw(sheet)
    for col, style in enumerate(("digit", "plate", "border")):
        for row, state in enumerate(STATES):
            x, y = col * 512, row * 132
            source = Image.open(OUT / style / f"hud-{row + 1:02d}-4x.png")
            sheet.paste(source.resize((512, 96), Image.Resampling.NEAREST), (x, y + 24))
            pen.text((x + 8, y + 6), f"{style.upper()} | {state['label'].upper()} | {state['face']}",
                     fill=(255, 255, 255))
    sheet.save(OUT / "comparison.png")


def export_editable_art() -> None:
    key_sheet = np.zeros((8, 24, 3), dtype=np.uint8)
    for i, color in enumerate(KEYS[1:]):
        key_sheet[:, i * 8:(i + 1) * 8][KEY_SHAPE != 0] = RGB[color]
    Image.fromarray(key_sheet).save(OUT / "keys-same-shape.png")
    font = np.zeros((24, 12 * 8, 3), dtype=np.uint8)
    for i, ch in enumerate("0123456789%/"):
        tall = np.ones((16, 8), dtype=np.uint8)
        draw_big(tall, ch, 0, 0)
        rgb = np.asarray([RGB[c] for c in PANEL], dtype=np.uint8)
        font[:16, i * 8:(i + 1) * 8] = rgb[tall]
        if ch in SMALL:
            small = np.ones((8, 8), dtype=np.uint8)
            pixels = np.asarray([[v == "#" for v in row] for row in SMALL[ch]])
            small[:5, :3][pixels] = 3 if ch == "/" else 2
            font[16:, i * 8:(i + 1) * 8] = rgb[small]
    Image.fromarray(font).save(OUT / "numeric-font.png")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    target = np.asarray(Image.open(REF).convert("RGB"))
    staging = Path("/tmp/fcpico-hud-playback-mesen-runtime")
    shutil.copytree(MESEN.parent, staging, dirs_exist_ok=True)
    settings = {"Nes": {"Region": "Ntsc", "DisableGameDatabase": True,
                         "RamPowerOnState": "AllZeros", "RandomizeCpuPpuAlignment": False,
                         "RandomizeMapperPowerOnState": False,
                         "RemoveSpriteLimit": False},
                "Preferences": {"AutomaticallyCheckForUpdates": False,
                                "EnableRewind": False, "EnableAutoSaveState": False,
                                "DisableOsd": True},
                "Debug": {"ScriptWindow": {"AllowIoOsAccess": True}}}
    (staging / "settings.json").write_text(json.dumps(settings, indent=2))
    export_editable_art()
    (OUT / "capture.lua").write_text('''local output = assert(os.getenv("HUD_PLAYBACK_OUTPUT"))
local frames = 0
emu.addEventCallback(function()
  frames = frames + 1
  local number = frames == 30 and 1 or frames == 90 and 2 or frames == 150 and 3 or 0
  if number ~= 0 then
    local file = assert(io.open(string.format("%s/frame-%02d.png", output, number), "wb"))
    file:write(emu.takeScreenshot())
    file:close()
    local debug = assert(io.open(string.format("%s/ppu-%02d.txt", output, number), "w"))
    for _, address in ipairs({0x2000, 0x2001, 0x2002, 0x2003, 0x23c0, 0x3f00}) do
      debug:write(string.format("%04X %02X\\n", address,
        emu.read(address, emu.memType.nesPpuDebug)))
    end
    for _, address in ipairs({0x10, 0x11, 0x12, 0x2000, 0x2001}) do
      debug:write(string.format("CPU %04X %02X\\n", address,
        emu.read(address, emu.memType.nesDebug)))
    end
    for key, value in pairs(emu.getState()) do
      if string.find(key, "ppu") then
        debug:write(string.format("%s %s\\n", key, tostring(value)))
      end
    end
    debug:close()
    if number == 3 then emu.stop(0) end
  end
end, emu.eventType.endFrame)
''')
    for style in ("digit", "plate", "border"):
        folder = OUT / style
        atlas = SpriteAtlas(False)
        frames, oams, measured = [], [], []
        for state in STATES:
            atlas.oam.clear()
            bg, attrs = build_state(target, atlas, state, style)
            frames.append(bg)
            _, oam, metrics = atlas.bytes()
            oams.append(oam)
            measured.append(metrics)
        # Include yellow digit masks for weapon numbers not selected in the
        # three playback frames, so 2..7 can all use the selected treatment.
        for weapon in range(2, 8):
            digit = glyph(str(weapon))
            sprite = np.zeros((8, 8), dtype=np.uint8)
            if style == "digit":
                sprite[:7, :5][digit != 0] = 3
            elif style == "plate":
                sprite[:] = 3
                sprite[:7, :5][digit != 0] = 0
            else:
                sprite[0, :] = sprite[7, :] = 3
                sprite[:, 0] = sprite[:, 7] = 3
            atlas.tile(sprite)
        bg_chr, tables, tile_count, font_tiles = pack_backgrounds(frames, attrs)
        sprite_chr, _, _ = atlas.bytes()
        rom = write_rom(folder, bg_chr + sprite_chr, tables, oams)
        capture(rom, folder, staging)
        manifest = {"style": style, "states": STATES,
                    "capture_frames": [30, 90, 150], "bg_tiles": tile_count,
                    "sprite_tiles": len(atlas.tiles), "state_metrics": measured,
                    "font_tiles": font_tiles,
                    "bg_palettes": [PANEL, SKIN, ARMS, KEYS],
                    "sprite_palettes": [PANEL, SKIN, ARMS, KEYS],
                    "sprite_limit_removed": False, "mapper": "NROM playback fixture"}
        (folder / "metrics.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(style, tile_count, len(atlas.tiles),
              [m["peak_scanline_sprites"] for m in measured], flush=True)
    comparison()


if __name__ == "__main__":
    main()
