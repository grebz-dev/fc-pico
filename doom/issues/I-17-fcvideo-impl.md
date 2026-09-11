<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-17 The converter, on the host and on the device

| | |
|---|---|
| **Lane** | B -- depends on I-16 |
| **Size** | L |
| **Depends on** | I-03, I-16 |
| **Work plan task** | P1-T2 / P1-T3 / P2-T5 |

## Goal

This is the part that makes Doom look like Doom on four colours a block. It is pure
computation over byte arrays, so it can be finished and proven on the host and only then
wired to the bus interrupt.

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
2. Test by differential comparison: for a corpus of frames (synthetic plus dumps from I-16),
   the C output must equal `tools/fcvideo_ref.py`'s output exactly.
3. Add the palette presets from the plan, including PiPU's measured set as preset C.
4. Wire `fcvideo_frame_begin/line_sink/frame_end` to `fcbus_core_publish()` and measure the
   conversion time; report it through the CLI from I-10.

## Acceptance

Every command must pass from the repository root.

```
cmake --build build-host && ctest --test-dir build-host --output-on-failure
python3 -m pytest doom/tests/tools -q
```

## Traps

Attribute hysteresis exists so a still scene does not flicker between equally good
palettes. Test it on a still scene, not a synthetic gradient.
