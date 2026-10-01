#!/usr/bin/env python3
"""Throwaway NES HUD comparison: A/B/C/D real PPU fixtures and Mesen captures.

Question: which layer split best reproduces the edited status bar within the
NES palette, attribute, CHR, OAM, and eight-sprite-per-line limits?
Run: doom/.venv/bin/python doom/tools/prototypes/hud_reference_variants.py
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "doom/assets/hud_layout_variants"
REF = ROOT / "doom/assets/hud_edit_reference/full-frame-reference-edited.png"
FACE = ROOT / "doom/assets/doomguy_faces.png"
MESEN = ROOT / "doom/sim/mesen2/Mesen2/bin/linux-x64/Release/linux-x64/publish/Mesen"
NESASM = ROOT / ".build/tools/nesasm/nesasm"

# Mesen NTSC RGB samples, recorded from the existing HUD capture. Values for
# new warm colors are only for choosing a nearest index; Mesen renders itself.
RGB = {0x0F: (0, 0, 0), 0x00: (102, 102, 102), 0x30: (255, 254, 255),
       0x16: (181, 49, 32), 0x06: (86, 29, 0), 0x07: (86, 29, 0),
       0x17: (169, 82, 0), 0x26: (254, 129, 112), 0x12: (38, 52, 192),
       0x28: (188, 190, 0), 0x38: (255, 255, 0)}
PANEL = [0x0F, 0x00, 0x30, 0x16]
SKIN = [0x0F, 0x06, 0x17, 0x26]
ARMS = [0x0F, 0x00, 0x30, 0x38]
KEYS = [0x0F, 0x12, 0x38, 0x16]


def nearest(rgb: np.ndarray, palette: list[int]) -> np.ndarray:
    colors = np.asarray([RGB[c] for c in palette], dtype=np.int16)
    delta = rgb.astype(np.int16)[..., None, :] - colors
    return np.argmin(np.sum(delta.astype(np.int32) ** 2, axis=-1), axis=-1).astype(np.uint8)


def encode_tile(tile: np.ndarray) -> bytes:
    a = bytearray()
    for bit in (0, 1):
        for row in tile:
            a.append(sum(((int(px) >> bit) & 1) << (7 - x)
                         for x, px in enumerate(row)))
    return bytes(a)


def face_index() -> np.ndarray:
    image = np.asarray(Image.open(FACE).convert("RGB"))[:24, :24]
    src = {(0, 0, 0): 0, (131, 31, 31): 1,
           (135, 87, 51): 2, (207, 131, 83): 3}
    indexed = np.zeros((24, 24), dtype=np.uint8)
    for color, index in src.items():
        indexed[np.all(image == color, axis=2)] = index
    # First 24x24 STFST00 cell, indexed nearest-neighbor stretch to 32x40.
    return np.asarray(Image.fromarray(indexed).resize((32, 40), Image.Resampling.NEAREST))


def layout_reference() -> None:
    source = Image.open(REF).convert("RGB")
    canvas = source.resize((1024, 960), Image.Resampling.NEAREST)
    pen = ImageDraw.Draw(canvas)
    for x in range(0, 257, 16):
        pen.line((x * 4, 192 * 4, x * 4, 959), fill=(0, 230, 230), width=1)
    for y in range(192, 241, 16):
        pen.line((0, y * 4, 1023, y * 4), fill=(0, 230, 230), width=1)
    for x in (31, 74, 109, 146, 182, 195):
        pen.line((x * 4, 192 * 4, x * 4, 959), fill=(255, 0, 255), width=2)
    canvas.save(OUT / "target-annotated.png")


def export_faces() -> None:
    original = np.asarray(Image.open(FACE).convert("RGB"))
    colors = {(0, 0, 0): 0, (131, 31, 31): 1,
              (135, 87, 51): 2, (207, 131, 83): 3}
    palette = np.asarray([RGB[v] for v in SKIN], dtype=np.uint8)
    sheet = np.zeros((6 * 40, 7 * 32, 3), dtype=np.uint8)
    indexed_sheet = np.zeros((6 * 40, 7 * 32), dtype=np.uint8)
    for row in range(6):
        for col in range(7):
            src = original[row * 24:(row + 1) * 24, col * 24:(col + 1) * 24]
            indices = np.zeros((24, 24), dtype=np.uint8)
            for rgb, value in colors.items():
                indices[np.all(src == rgb, axis=2)] = value
            scaled = np.asarray(Image.fromarray(indices).resize(
                (32, 40), Image.Resampling.NEAREST))
            sheet[row * 40:(row + 1) * 40, col * 32:(col + 1) * 32] = palette[scaled]
            indexed_sheet[row * 40:(row + 1) * 40, col * 32:(col + 1) * 32] = scaled
    Image.fromarray(sheet).save(OUT / "faces-32x40-normalized.png")
    (OUT / "faces-32x40-indexed.bin").write_bytes(indexed_sheet.tobytes())
    source = json.loads((ROOT / "doom/assets/doomguy_faces.json").read_text())
    (OUT / "faces-32x40.json").write_text(json.dumps({
        "source": str(FACE.relative_to(ROOT)), "size": [32, 40],
        "columns": 7, "order": source["order"], "nes_palette": SKIN,
        "sampling": "Pillow nearest-neighbor per 24x24 source cell"}, indent=2) + "\n")


def make_layers(kind: str, target: np.ndarray, face: np.ndarray):
    bg = nearest(target, PANEL)
    # The edited face is a visual guide. All candidates use STFST00 from the
    # supplied face sheet at the same x/y and scale.
    bg[196:236, 112:144] = 0
    attr = np.zeros((15, 16), dtype=np.uint8)
    attr[12:15, :] = 0  # palette 0 is the panel
    bg_palettes = [PANEL, SKIN, ARMS, KEYS]

    # Preserve the intended panel borders and labels. The target's warm face
    # pixels are replaced with one canonical source sheet state.
    if kind in ("C", "D"):
        attr[12:15, 7:9] = 1
        bg[196:236, 112:144] = face
    if kind == "D":
        # 16-pixel columns are required for independently colored BG art.
        # Recolor weapons within x80..111; the leftmost target pixels stay in
        # the panel palette. Key column x176..191 becomes a black slot strip.
        attr[12:15, 5:7] = 2
        attr[12:15, 11:12] = 3
        bg[192:240, 176:192] = 0
        bg[193:238, 177] = 2  # key strip border: yellow
        bg[193:238, 190] = 2
        arm_rgb = target[199:229, 80:109]
        bg[199:229, 80:109] = nearest(arm_rgb, ARMS)
        # Shift the key symbols left to keep inventory in palette 0.
        for y, col in ((201, 1), (213, 2), (225, 3)):
            bg[y:y + 5, 182:190] = 0
            bg[y + 1:y + 4, 184:189] = col
            bg[y:y + 5, 186] = col
        # A small section of the armor label overlaps the colored column in
        # this option; its loss is an explicit attribute-grid compromise.
    else:
        # For A/B/C, colored weapon digits and keys sit over panel outlines.
        # Remove only target pixels with colors that the panel cannot show.
        for ys, xs in ((slice(198, 227), slice(76, 108)),
                       (slice(198, 234), slice(183, 194))):
            region = target[ys, xs]
            normal = nearest(region, PANEL)
            palette_rgb = np.asarray([RGB[c] for c in PANEL], dtype=np.int16)
            distance = np.min(np.sum((region.astype(np.int16)[:, :, None] -
                                      palette_rgb) ** 2, axis=3), axis=2)
            bg[ys, xs][distance > 9000] = 0
    # Make the non-HUD viewport exactly the reference's checkerboard in a
    # simple two-color palette, rather than spending hundreds of CHR tiles.
    yy, xx = np.indices((192, 256))
    bg[:192] = np.where(((xx // 16 + yy // 16) & 1) == 0, 2, 1)
    return bg, attr, bg_palettes


class SpriteAtlas:
    def __init__(self, tall: bool):
        self.tall = tall
        self.tiles = [bytes(16)]  # tile 0 is transparent
        self.lookup = {bytes(16): 0}
        self.oam: list[tuple[int, int, int, int]] = []

    def tile(self, pixels: np.ndarray) -> int:
        encoded = encode_tile(pixels)
        if encoded not in self.lookup:
            self.lookup[encoded] = len(self.tiles)
            self.tiles.append(encoded)
        return self.lookup[encoded]

    def add(self, pixels: np.ndarray, x: int, y: int, palette: int) -> None:
        height = 16 if self.tall else 8
        width = (pixels.shape[1] + 7) // 8
        rows = (pixels.shape[0] + height - 1) // height
        for r in range(rows):
            for c in range(width):
                cell = np.zeros((height, 8), dtype=np.uint8)
                crop = pixels[r * height:(r + 1) * height, c * 8:(c + 1) * 8]
                cell[:crop.shape[0], :crop.shape[1]] = crop
                if not np.any(cell):
                    continue
                if self.tall:
                    # 8x16: bit 0 selects $1000, even/odd tile pair.
                    top = self.tile(cell[:8])
                    bottom = self.tile(cell[8:])
                    # Store a legal pair at an even tile number in CHR bank1.
                    if len(self.tiles) & 1:
                        self.tiles.append(bytes(16))
                    pair = len(self.tiles)
                    self.tiles.extend((self.tiles[top], self.tiles[bottom]))
                    tile = pair | 1
                else:
                    tile = self.tile(cell)
                self.oam.append((y + r * height - 1, tile, palette, x + c * 8))

    def bytes(self) -> tuple[bytes, bytes, dict]:
        if len(self.tiles) > 256:
            raise ValueError(f"sprite CHR uses {len(self.tiles)} tiles")
        if len(self.oam) > 64:
            raise ValueError(f"OAM uses {len(self.oam)} sprites")
        counts = [0] * 240
        height = 16 if self.tall else 8
        for y, _, _, _ in self.oam:
            for line in range(max(0, y + 1), min(240, y + 1 + height)):
                counts[line] += 1
        if max(counts) > 8:
            raise ValueError(f"scanline overload: {max(counts)} at {counts.index(max(counts))}")
        oam = bytearray([0xEF, 0, 0, 0] * 64)
        for i, entry in enumerate(self.oam):
            oam[4 * i:4 * i + 4] = bytes(entry)
        chr_data = b"".join(self.tiles).ljust(4096, b"\0")
        return chr_data, bytes(oam), {"oam": len(self.oam),
                                      "peak_scanline_sprites": max(counts),
                                      "peak_scanlines": ([i for i, n in enumerate(counts)
                                                          if n == max(counts)] if self.oam else []),
                                      "sprite_chr_tiles": len(self.tiles)}


def colored_sprites(kind: str, target: np.ndarray, face: np.ndarray) -> SpriteAtlas:
    atlas = SpriteAtlas(kind == "B")
    if kind in ("A", "B"):
        # Source face indices 1..3 are directly mapped through sprite palette1.
        atlas.add(face, 112, 196, 1)
    if kind == "D":
        return atlas
    # Draw the weapon numerals from the user's exact pixel layout. Color
    # differences are deliberately handled by sprite palettes, with BG boxes.
    for col, x in enumerate((80, 88, 96)):
        combined = np.zeros((16, 8), dtype=np.uint8)
        for row, y in enumerate((203, 215)):
            crop = target[y:y + 8, x:x + 8]
            chroma = np.max(crop, axis=2) - np.min(crop, axis=2)
            mask = (chroma > 28) & (np.mean(crop, axis=2) > 45)
            digit = np.zeros((8, 8), dtype=np.uint8)
            digit[mask] = 3 if (row, col) == (0, 0) else 2
            if kind == "B":
                combined[row * 8:row * 8 + 8] = digit
            else:
                atlas.add(digit, x, y, 2)
        if kind == "B":
            # Two 8-pixel rows share one 8x16 entry; the lower row moves up
            # four pixels from the edited reference to make legal pairing.
            atlas.add(combined, x, 203, 2)
    for y, color in ((200, 1), (212, 2), (224, 3)):
        icon = np.zeros((8, 8), dtype=np.uint8)
        icon[1:6, 1:7] = color
        icon[2:5, 3] = 0
        # B uses 16-pixel vertical regions; put three colors in different
        # x positions or its two rows would overlap sprite evaluation.
        atlas.add(icon, 184, (198 if kind == "B" and y == 200 else
                              214 if kind == "B" and y == 212 else
                              230 if kind == "B" else y), 3)
    return atlas


def pack_bg(bg: np.ndarray, attr: np.ndarray) -> tuple[bytes, bytes, int]:
    tiles = [bytes(16)]
    lookup = {bytes(16): 0}
    names = bytearray(960)
    for ty in range(30):
        for tx in range(32):
            key = encode_tile(bg[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
            if key not in lookup:
                lookup[key] = len(tiles)
                tiles.append(key)
            names[ty * 32 + tx] = lookup[key]
    if len(tiles) > 256:
        raise ValueError(f"background CHR uses {len(tiles)} tiles")
    attributes = bytearray(64)
    for ay in range(8):
        for ax in range(8):
            value = 0
            for dy in range(2):
                for dx in range(2):
                    by, bx = ay * 2 + dy, ax * 2 + dx
                    value |= int(attr[min(by, 14), bx]) << (2 * (dy * 2 + dx))
            attributes[ay * 8 + ax] = value
    return b"".join(tiles).ljust(4096, b"\0"), bytes(names + attributes), len(tiles)


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
 inx
 stx $2000
 stx $2001
 stx $4010
wait1:
 bit $2002
 bpl wait1
wait2:
 bit $2002
 bpl wait2
 lda #$20
 sta $2006
 lda #$00
 sta $2006
 ldx #$00
copy0:
 lda nametable,x
 sta $2007
 inx
 bne copy0
copy1:
 lda nametable+$100,x
 sta $2007
 inx
 bne copy1
copy2:
 lda nametable+$200,x
 sta $2007
 inx
 bne copy2
copy3:
 lda nametable+$300,x
 sta $2007
 inx
 bne copy3
 lda #$3f
 sta $2006
 lda #$00
 sta $2006
 ldx #$00
copy_pal:
 lda palettes,x
 sta $2007
 inx
 cpx #$20
 bne copy_pal
 ldx #$00
copy_oam:
 lda oam_data,x
 sta $0200,x
 inx
 bne copy_oam
 lda #$00
 sta $2003
 lda #$02
 sta $4014
 lda #$00
 sta $2005
 sta $2005
 lda #%CTRL%
 sta $2000
 lda #$1e
 sta $2001
forever:
 jmp forever
nmi:
 rti
irq:
 rti
.bank 1
.org $a000
nametable:
 .incbin \"nametable.bin\"
palettes:
 .incbin \"palettes.bin\"
oam_data:
 .incbin \"oam.bin\"
.bank 3
.org $fffa
 .dw nmi
 .dw reset
 .dw irq
.bank 4
.org $0000
 .incbin \"chr.bin\"
"""


def assemble(kind: str, folder: Path, chr_data: bytes, nt: bytes,
             palettes: list[int], oam: bytes) -> Path:
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "chr.bin").write_bytes(chr_data)
    (folder / "nametable.bin").write_bytes(nt)
    (folder / "palettes.bin").write_bytes(bytes(palettes))
    (folder / "oam.bin").write_bytes(oam)
    source = ASM.replace("%CTRL%", "$28" if kind == "B" else "$08")
    source = "\n".join(" " + line if line.startswith(".") else line
                       for line in source.splitlines()) + "\n"
    (folder / "fixture.asm").write_text(source)
    subprocess.run([str(NESASM), "fixture.asm"], cwd=folder, check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return folder / "fixture.nes"


def capture(rom: Path, staging: Path, folder: Path) -> None:
    folder.mkdir(exist_ok=True)
    env = dict(os.environ, HUD_VARIANT_CAPTURE=str(folder / "mesen.png"),
               SDL_AUDIODRIVER="dummy")
    dotnet = Path.home() / ".dotnet"
    if "DOTNET_ROOT" not in env and (dotnet / "host/fxr").is_dir():
        env["DOTNET_ROOT"] = str(dotnet)
    with (folder / "mesen.log").open("w") as log:
        subprocess.run([str(staging / MESEN.name), "--testRunner",
                        str(OUT / "capture.lua"), str(rom), "--timeout=40"],
                       cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=55)
    if not (folder / "mesen.png").is_file():
        raise RuntimeError(f"Mesen did not capture {folder}")
    if (folder / "mesen.log").stat().st_size == 0:
        (folder / "mesen.log").unlink()
    im = Image.open(folder / "mesen.png").convert("RGB")
    if im.size != (256, 240):
        raise RuntimeError(f"unexpected Mesen image size {im.size}")
    im.crop((0, 192, 256, 240)).resize((1024, 192), Image.Resampling.NEAREST).save(folder / "hud-4x.png")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    target = np.asarray(Image.open(REF).convert("RGB"))
    face = face_index()
    layout_reference()
    export_faces()
    sprite_palettes = [PANEL, SKIN, ARMS, KEYS]
    staging = Path("/tmp/fcpico-hud-layout-mesen-runtime")
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
    (OUT / "capture.lua").write_text('''local frames = 0
emu.addEventCallback(function()
  frames = frames + 1
  if frames == 30 then
    local path = assert(os.getenv("HUD_VARIANT_CAPTURE"))
    local file = assert(io.open(path, "wb"))
    file:write(emu.takeScreenshot())
    file:close()
    emu.stop(0)
  end
end, emu.eventType.endFrame)
''')
    report = {}
    for kind in "ABCD":
        folder = OUT / kind
        bg, attr, bgs = make_layers(kind, target, face)
        atlas = colored_sprites(kind, target, face)
        spr_chr, oam, metrics = atlas.bytes()
        bg_chr, nt, bg_count = pack_bg(bg, attr)
        palette = sum(bgs, []) + sum(sprite_palettes, [])
        rom = assemble(kind, folder, bg_chr + spr_chr, nt, palette, oam)
        capture(rom, staging, folder)
        metrics.update({"variant": kind, "sprite_mode": "8x16" if kind == "B" else "8x8",
                        "bg_chr_tiles": bg_count, "bg_palettes": bgs,
                        "sprite_palettes": sprite_palettes,
                        "mapper": "NROM static fixture", "capture_frame": 30,
                        "sprite_limit_removed": False})
        (folder / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n")
        report[kind] = metrics
        print(kind, metrics["oam"], metrics["peak_scanline_sprites"], bg_count,
              metrics["sprite_chr_tiles"], flush=True)
    # Compare the four real emulator captures on a simple labeled sheet.
    sheet = Image.new("RGB", (1024, 360), (25, 25, 25))
    pen = ImageDraw.Draw(sheet)
    target_crop = Image.open(REF).convert("RGB").crop((0, 192, 256, 240))
    source_crop = Image.open(ROOT / "doom/assets/hud_edit_reference/full-frame-reference.png").convert("RGB").crop((0, 192, 256, 240))
    for col, (name, crop) in enumerate((("EDITED TARGET", target_crop),
                                        ("PREVIOUS MESEN", source_crop))):
        x = col * 512
        sheet.paste(crop.resize((512, 96), Image.Resampling.NEAREST), (x, 24))
        pen.text((x + 8, 5), name, fill=(255, 255, 255))
    for i, kind in enumerate("ABCD"):
        x, y = (i % 2) * 512, ((i // 2) + 1) * 120
        crop = Image.open(OUT / kind / "hud-4x.png")
        sheet.paste(crop.resize((512, 96), Image.Resampling.NEAREST), (x, y + 24))
        pen.text((x + 8, y + 5), f"{kind} | {report[kind]['sprite_mode']} | {report[kind]['oam']} sprites",
                 fill=(255, 255, 255))
    sheet.save(OUT / "comparison.png")
    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
