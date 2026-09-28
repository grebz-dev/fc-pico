# SPDX-License-Identifier: BSD-3-Clause
"""Resident menu art fits its sprite and CHR budgets."""

from __future__ import annotations

import json

from build_doom_menu_art import ROOT, Whx, encode_tiles, episode_tiles, logo_pixels
from build_native_status_probe import MENU_GLYPHS, episode_menu_oam, logo_oam


def test_logo_sheet_is_reproducible_and_eight_sprites_wide():
    art = encode_tiles(logo_pixels(Whx(ROOT / "doom/rp2040-doom/doom1.whx")))
    assert art == (ROOT / "doom/assets/doom_menu_logo.chr").read_bytes()
    assert len(art) == 24 * 16
    oam = logo_oam()
    assert len(oam) == 24 * 4
    assert max(oam[index * 4 + 3] for index in range(24)) == 152
    assert all(sum(oam[index * 4] == y for index in range(24)) <= 8
               for y in (15, 23, 31))


def test_actual_shareware_episode_names_fit_menu_slots():
    art, lookup = episode_tiles()
    assert art == (ROOT / "doom/assets/doom_episode_pairs.chr").read_bytes()
    assert len(lookup) == 19
    manifest = json.loads((ROOT / "doom/assets/doom_menu_art.json").read_text())
    assert manifest["episode_lines"] == [["KNEE DEEP IN", "THE DEAD"],
                                         ["SHORES OF HELL"], ["INFERNO"]]
    oam = episode_menu_oam()
    assert len(oam) == MENU_GLYPHS * 4
    assert sum(oam[index * 4] < 0xEF for index in range(MENU_GLYPHS)) == 21
