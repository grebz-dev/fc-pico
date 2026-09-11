<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-02 Tests for the stream decoder

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | S |
| **Depends on** | none |
| **Work plan task** | P0-T7 |

## Goal

The decoder is what turns a stream buffer back into something a human or a PSNR gate can
look at, so every golden image in the project will be produced by it. It has no tests.

## Specification

`plan/04-video.md` (the output format section) and `plan/09-testing-ci.md` L2/L3.
`tools/fcpico/stream.py` is already tested by `tests/tools/test_stream.py`; the decoder on
top of it is not.

## Owns (create or modify only these)

`tests/tools/test_ppu_decode.py` (new); `tools/ppu_decode.py` (fixes only)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Build a synthetic 256x240 frame of known 2-bit values with `tools/fcpico/stream.py`,
   encode it, decode it with `decode_to_rgb()` and assert the RGB values are exactly the
   NES palette entries the attribute table and palette select.
2. Cover: the default grey ramp when no palette is given; a per-block attribute table where
   neighbouring 16x16 blocks use different sub-palettes; colour 0 always showing the
   backdrop regardless of sub-palette.
3. Cover the `--mailbox-v2` path: attributes and palette taken from the mailbox at
   `VRAM_MAILBOX_OFF` only when the flag bits are set, and ignored when they are not.
4. Exercise the CLI through `subprocess` and assert a PNG of the right size is written.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/tools -q
```

## Traps

Pixel value 0 is the shared backdrop, not "sub-palette entry 0" -- getting this wrong
makes every golden subtly wrong in a way that looks plausible.
