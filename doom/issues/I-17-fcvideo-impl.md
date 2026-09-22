<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-17 The converter, on the host and on the device

| | |
|---|---|
| **Lane** | A for host conversion; B for device integration |
| **Size** | L |
| **Depends on** | I-03; I-16 only for engine frames and device integration |
| **Work plan task** | P1-T2 / P1-T3 / P2-T5 |

## Goal

This is the part that makes Doom look like Doom on four colours a block. It is pure
computation over byte arrays, so it can be finished and proven on the host and only then
wired to the bus interrupt.

## Current execution (2026-09-21)

The host core under `port/video/` decimates and letterboxes 320x200 frames, chooses
attributes with hysteresis, dithers through supplied `err`/`lut` tables, packs the 34-word
scanline stream and writes the v2 mailbox. A differential test compares the complete C stream
and attributes against `tools/fcvideo_ref.py` for gradient, checkerboard and random frames.
Presets A/B/C and palette-0 `err`/`lut` generation now match that Python oracle byte for byte,
including a complete stream built from C-generated tables. Engine frame input, the 14 flash
palette sets, device publication/timing, and Mesen pixel acceptance remain open. Mesen S0
validates the 34-word fetch schedule, but strict S0 still needs I-19 before a picture golden
is meaningful.

## Specification

`plan/04-video.md` stages B to E; `tools/fcvideo_ref.py` is the reference this must
agree with, byte for byte, on the same input.

## Owns (create or modify only these)

`port/video/fcvideo.c`, `port/video/fcvideo.h`, `port/video/fcvideo_presets.h`, `port/video/CMakeLists.txt`, `tests/fcvideo/*` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Implement stages B to E in C, with the same table layout the reference uses.
2. Test by differential comparison: start with synthetic frames; add dumps from I-16 when its
   8-bit composition exists. For a corpus of frames,
   the C output must equal `tools/fcvideo_ref.py`'s output exactly.
3. Add the palette presets from the plan, including PiPU's measured set as preset C.
4. Wire `fcvideo_frame_begin/line_sink/frame_end` to `fcbus_core_publish()` and measure the
   conversion time; report it through the CLI from I-10.
5. Validate the resulting stream through Mesen S2 once strict S0 is calibrated by I-19.
   Host reference equality is the acceptance gate for the independent first slice; an
   open-bus S0 screenshot is not pixel evidence.

## Acceptance

Every command must pass from the repository root.

```
cmake --build build-host && ctest --test-dir build-host --output-on-failure
python3 -m pytest doom/tests/tools doom/tests/fcvideo -q
```

## Traps

Attribute hysteresis exists so a still scene does not flicker between equally good
palettes. Test it on a still scene, not a synthetic gradient.
