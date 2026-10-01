#!/usr/bin/env python3
"""Quick visual regression signal for the Mesen HUD playback captures."""

from pathlib import Path
import sys

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
REFERENCE = np.asarray(Image.open(
    ROOT / "doom/assets/hud_edit_reference/full-frame-reference-edited.png"
).convert("RGB"))


def red_mask(rgb: np.ndarray) -> np.ndarray:
    red = rgb[:, :, 0].astype(np.int16)
    green = rgb[:, :, 1].astype(np.int16)
    blue = rgb[:, :, 2].astype(np.int16)
    return (red > 120) & (red * 10 > green * 14) & (red * 10 > blue * 14)


def ratio(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.count_nonzero(a & b) / np.count_nonzero(a | b))


def check(path: Path, healthy: bool) -> list[str]:
    screen = np.asarray(Image.open(path).convert("RGB"))
    failures = []
    if healthy:
        for name, left, right in (("AMMO", 4, 31), ("HEALTH", 35, 74),
                                  ("ARMOR", 148, 182)):
            target = red_mask(REFERENCE[198:216, left:right])
            actual = red_mask(screen[198:216, left:right])
            score = ratio(target, actual)
            if score < 0.42:
                failures.append(f"{name} red glyph overlap {score:.3f} < 0.42")
    cell = screen[225:232, 195:220]
    white = np.count_nonzero(np.all(cell > 180, axis=2))
    if white < 40:
        failures.append(f"CELL label has {white} white pixels < 40")
    key = screen[200:208, 184:192].astype(np.int16)
    blue = (key[:, :, 2] > key[:, :, 0] * 1.3) & (key[:, :, 2] > 70)
    if int(blue[3].sum()) < 5 or int(blue[4].sum()) < 5:
        failures.append("blue key silhouette narrows to a stem instead of a card")
    return failures


if __name__ == "__main__":
    folder = ROOT / "doom/assets/hud_playback_candidates"
    paths = [folder / style / f"frame-{i:02d}.png"
             for style in ("digit", "plate", "border") for i in (1, 2, 3)]
    bad = 0
    for path in paths:
        failures = check(path, path.stem == "frame-01")
        print(path.parent.name, path.stem, "PASS" if not failures else "; ".join(failures))
        bad += bool(failures)
    sys.exit(1 if bad else 0)
