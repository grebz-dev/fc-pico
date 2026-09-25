# SPDX-License-Identifier: BSD-3-Clause
"""Capacity boundary checks for proposed NES sprite layouts."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from check_sprite_layout import check_screen  # noqa: E402


class SpriteLayoutBudgetTests(unittest.TestCase):
    def test_scanline_boundary_and_face_cost(self):
        screen = {"name": "status", "groups": [
            {"name": "face", "x": 100, "y": 208, "width": 16, "height": 16},
            {"name": "packed values", "x": 8, "y": 208,
             "width": 48, "height": 8},
        ]}
        result = check_screen(screen)
        self.assertEqual(result["entries"], 10)
        self.assertEqual(result["max_scanline"], 8)
        self.assertEqual(result["scanlines"][208:216], [8] * 8)
        self.assertEqual(result["scanlines"][216:224], [2] * 8)
        self.assertEqual(result["errors"], [])

        screen["groups"].append({"name": "key", "x": 180, "y": 208,
                                 "width": 8, "height": 8})
        result = check_screen(screen)
        self.assertEqual(result["max_scanline"], 9)
        self.assertIn("scanlines", result["errors"][0])

    def test_oam_boundary_across_nonoverlapping_lines(self):
        groups = [{"name": f"row {i}", "x": 8, "y": i * 8,
                   "width": 64, "height": 8} for i in range(8)]
        screen = {"name": "menu", "groups": groups}
        self.assertEqual(check_screen(screen)["errors"], [])
        screen["groups"].append({"name": "extra", "x": 8, "y": 80,
                                 "width": 8, "height": 8})
        self.assertIn("OAM: 65", check_screen(screen)["errors"][0])

    def test_rejects_off_screen_group(self):
        screen = {"name": "menu", "groups": [
            {"name": "long text", "x": 248, "y": 10,
             "width": 16, "height": 8},
        ]}
        self.assertIn("outside", check_screen(screen)["errors"][0])


if __name__ == "__main__":
    unittest.main()
