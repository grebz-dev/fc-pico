<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-17 The converter, on the host and on the device

| | |
|---|---|
| **Lane** | A for host conversion; B for device integration |
| **Size** | L |
| **Depends on** | [I-03](I-03-fcvideo-ref-tests.md); [I-16](I-16-stage-a-composition.md) only for engine frames and device integration |
| **Work plan task** | [P1-T2](../plan/10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox) / [P1-T3](../plan/10-workplan.md#p1-t3-device-integration-cores-irqs-semaphores) / [P2-T5](../plan/10-workplan.md#p2-t5-stages-c-and-e) |

## Goal

This is the part that makes Doom look like Doom on four colours a block. It is pure
computation over byte arrays, so it can be finished and proven on the host and only then
wired to the bus interrupt.

## Status (2026-09-24)

**partial**. The host converter matches the Python oracle on synthetic and real engine
frames. The RP2350 converter publishes frames through `fcbus`, and the NES-001 displays
Doom. Mesen D0/D1 fixed-frame checks pass. Hardware conversion averages about 35.6 ms,
above the 8 ms target; dynamic S2 and a performance pass remain. The current 200-line
image at NES lines 16..215 is the specified letterbox, not a stream-size limit. See
[`../PROGRESS.md`](../PROGRESS.md) and [`../HARDWARE-LOG.md`](../HARDWARE-LOG.md).

## Specification

[`plan/04-video.md` stages B](../plan/04-video.md#stage-b----horizontal-decimation-320---256) to [E](../plan/04-video.md#stage-e----nes-palette-sets); `tools/fcvideo_ref.py` is the reference this must
agree with, byte for byte, on the same input.

## Owns (create or modify only these)

`port/video/fcvideo.c`, `port/video/fcvideo.h`, `port/video/fcvideo_presets.h`, `port/video/CMakeLists.txt`, `tests/fcvideo/*` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Implement stages B to E in C, with the same table layout the reference uses.
2. Test by differential comparison: start with synthetic frames; add dumps from [I-16](I-16-stage-a-composition.md) when its
   8-bit composition exists. For a corpus of frames,
   the C output must equal `tools/fcvideo_ref.py`'s output exactly.
3. Add the palette presets from the plan, including PiPU's measured set as preset C.
4. Wire `fcvideo_frame_begin/line_sink/frame_end` to `fcbus_core_publish()` and measure the
   conversion time; report it through the CLI from [I-10](I-10-testpattern-firmware.md).
5. Validate the resulting Doom stream through Mesen S2 once engine frames are available.
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
