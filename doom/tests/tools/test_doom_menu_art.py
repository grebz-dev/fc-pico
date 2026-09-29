# SPDX-License-Identifier: BSD-3-Clause
"""Resident menu art fits its sprite and CHR budgets."""

from __future__ import annotations

import json
import re

import numpy as np
from PIL import Image

from build_doom_menu_art import (ROOT, SMALL_LOGO_SIZE, WHDATA, Whx,
                                 encode_tiles, episode_tiles, logo_pixels, patch_pixels)
from build_native_status_probe import (MENU_GLYPHS, episode_menu_oam,
                                       logo_oam, main_menu_oam, small_logo_oam)
from edit_sprite_sheets import SHEETS, decode_tiles, encode_sheet, rgba_to_indices


def test_logo_sheet_is_reproducible_and_eight_sprites_wide():
    whx = Whx(ROOT / "doom/rp2040-doom/doom1.whx")
    pixels = logo_pixels(whx)
    assert len(encode_tiles(pixels)) == 32 * 16
    labels = re.findall(r"VPATCH_NAME\(([^)]+)\)",
                        WHDATA.read_text().split("#define VPATCH_LIST \\", 1)[1]
                        .split("\n\nenum", 1)[0])
    lookup = np.frombuffer(whx.named("P_START"), dtype="<u2")
    source = patch_pixels(whx.lump(int(lookup[labels.index("M_DOOM") + 1])))
    sampled = np.asarray(Image.fromarray(source).resize((64, 32), Image.Resampling.NEAREST))
    assert np.array_equal(pixels != 0, sampled >= 0), "sprite outline must match M_DOOM"
    small = logo_pixels(whx, SMALL_LOGO_SIZE)
    assert len(encode_tiles(small)) == 21 * 16
    assert len(small_logo_oam()) == 21 * 4
    oam = logo_oam()
    assert len(oam) == 32 * 4
    assert max(oam[index * 4 + 3] for index in range(32)) == 152
    assert all(sum(oam[index * 4] == y for index in range(32)) == 8
               for y in (15, 23, 31, 39))


def test_actual_shareware_episode_names_fit_menu_slots():
    art, lookup = episode_tiles()
    assert len(art) == 19 * 16
    assert len(lookup) == 19
    manifest = json.loads((ROOT / "doom/assets/doom_menu_art.json").read_text())
    assert manifest["episode_lines"] == [["KNEE DEEP IN", "THE DEAD"],
                                         ["SHORES OF HELL"], ["INFERNO"]]
    oam = episode_menu_oam()
    assert len(oam) == MENU_GLYPHS * 4
    assert sum(oam[index * 4] < 0xEF for index in range(MENU_GLYPHS)) == 21
    main = main_menu_oam()
    assert sum(main[index * 4] < 0xEF for index in range(MENU_GLYPHS)) == 28
    assert 12 + 28 + 21 + 1 <= 64


def test_editable_png_sheets_round_trip_to_the_rom_chr():
    for name, (stem, columns, rows, width, height, count, palette) in SHEETS.items():
        data = (ROOT / "doom/assets" / f"{stem}.chr").read_bytes()
        image = Image.open(ROOT / "doom/assets" / f"{stem}_edit.png")
        if name == "large_faces":
            from build_large_face_art import pack
            sheet = rgba_to_indices(image, palette)
            faces = np.stack([sheet[(i // 4) * 32:(i // 4 + 1) * 32,
                                    (i % 4) * 32:(i % 4 + 1) * 32]
                              for i in range(10)])
            packed, tile_map, flips = pack(faces)
            assert packed == data
            assert tile_map == (ROOT / "doom/assets/doomguy_faces_large_tiles.bin").read_bytes()
            assert flips == (ROOT / "doom/assets/doomguy_faces_large_flips.bin").read_bytes()
            continue
        pixels = decode_tiles(data, columns, rows, width, height)
        assert np.array_equal(rgba_to_indices(image, palette), pixels), name
        assert encode_sheet(pixels, columns, rows, width, height, count) == data
