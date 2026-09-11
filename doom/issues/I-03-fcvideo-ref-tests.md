<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-03 Tests for the reference video pipeline

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | M |
| **Depends on** | I-02 (soft) |
| **Work plan task** | P1-T2 |

## Goal

This module is the specification the on-device converter will be checked against. Until it
is tested, "the device matches the reference" means nothing.

## Specification

`plan/04-video.md`, stages B to E, including the corrected block counts (the Doom
frame covers 16 x 13 of the screen's 16 x 15 blocks, 51,200 pixels).

## Owns (create or modify only these)

`tests/tools/test_fcvideo_ref.py` (new); `tools/fcvideo_ref.py` (fixes only)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Stage B: `decimate_320_to_256` keeps exactly the columns where `x % 5 != 4`, in order,
   and returns 256 of them.
2. `place_letterbox` puts the Doom frame at console lines 16..215 and leaves the rest at the
   backdrop.
3. `build_err_and_lut`: a Doom colour that equals a sub-palette entry gets `err == 0` and a
   LUT row that is constant at that entry for all 16 Bayer thresholds; a colour halfway
   between two entries alternates between them.
4. `choose_block_palettes`: a block of a single colour picks a sub-palette containing it;
   hysteresis keeps the previous choice when the challenger is within the threshold and
   switches when it is not.
5. `quantize` uses the Bayer position `(x & 3) | ((y & 3) << 2)`, so a flat mid-tone field
   produces a 4x4-periodic pattern.
6. End to end: `convert()` on a synthetic 320x200 gradient produces a stream that
   `tools/ppu_decode.py` decodes, and the decoded image's PSNR against the reference beats a
   threshold you record in the test as a constant with a comment saying how it was derived.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/tools -q
```

## Traps

Do not tune the thresholds until the test passes; derive one from the first accepted
output and then only ever raise it (`plan/09-testing-ci.md`, L2).
