<!-- SPDX-License-Identifier: BSD-3-Clause -->
# Doom video legibility study (2026-09-25)

The original shared-white preset has no dark gray between the near-black
backdrop (`$0F`, RGB 5) and `$00` (RGB 128). Dark Doom surfaces therefore use
isolated medium-gray pixels. The [side-by-side reference preview](palette_eval.png)
shows DEMO1 frames 100, 300 and 500 at 2× nearest-neighbor magnification.
The columns compare the old default, a darker palette, the darker palette
with a small shadow lift, and the existing PiPU preset. The final column is
an alternative measured on a TV by the PiPU project, not on this FC PICO.

| Variant | Gray ramp | Brown ramp | PLAYPAL transform | Frame 100 / 300 / 500 PSNR (dB) |
|---|---|---|---|---|
| Old default | `$00 $10 $30` | `$07 $17 $30` | none | 13.04 / 10.86 / 10.71 |
| Darker ramp | `$1D $00 $30` | `$08 $18 $30` | none | 14.91 / 11.98 / 11.94 |
| **Shadow detail candidate** | `$1D $00 $30` | `$08 $18 $30` | `v + (v*(255-v)+384)/768` | 13.90 / 10.93 / 11.15 |

The red and green ramps stay `$06 $16 $30` and `$09 $19 $30` in the candidate.
PSNR compares the decoded 224-line NES picture with the original PLAYPAL RGB
image after horizontal decimation and vertical scaling. The lift intentionally
changes brightness, so its lower PSNR than the darker ramp alone does not mean
less legibility. On frame 300, the portion of the game view above the HUD with
luma at least 40 rises from 28.2% for the old default to 35.1% for the candidate;
walls and the enemy outlines are easier to distinguish in the preview.

The 4×4 Bayer dither and arbitrary two-entry mixing remain. Penalizing large
brightness differences between the two dither colors made the HUD text and
bright highlights collapse into flat gray in the preview. Turning off white
in the colored subpalettes also made text harder to read. Horizontal 320→256
nearest-neighbor decimation drops one source pixel in five; filtering those
palette indices would blend thin menu/HUD strokes, and an RGB interpolation
would need a new color-quantization path and more conversion time. Vertical
interpolation would similarly soften the one-pixel strokes. Neither is part
of this candidate while the converter already exceeds its timing target.

The chosen preset retains the old ones as build-time options. These are host
previews: the old NES-001 photo shows stronger cyan dot appearance than the
emulator's RGB output, so detailed scene comparisons remain in
[HR-6](../HARDWARE-REQUESTS.md#hr-6-shadow-detail-and-pending-bvideo-checks-task-p2-t6).

Hardware follow-up (2026-09-25): the user reports much better contrast and working
B input with the shadow-detail/224-line firmware. Serial conversion averages
38.208 ms (maximum 38.251 ms); see [the hardware log](../HARDWARE-LOG.md).
