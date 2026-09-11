<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-16 8-bit frame composition in the engine

| | |
|---|---|
| **Lane** | B -- depends on the host build |
| **Size** | L |
| **Depends on** | I-12 |
| **Work plan task** | P1-T1 |

## Goal

RP2040 Doom never assembles a whole 8-bit frame: the view is in one buffer and everything
else -- status bar, menus, HUD, intermissions, the melt wipe -- is composed per scanline into
16-bit RGB. The FC PICO path needs that composition redone in 8 bits, and it is the input
every later video stage consumes.

## Specification

`plan/04-video.md`, stage A, and the substitution table against
`rp2040-doom/src/pico/i_video.c`.

## Owns (create or modify only these)

In the submodule: `src/fcpico/i_video_fcpico.c` and `src/fcpico/fcpico_video_sink.h`. In this repository: `tests/goldens/*` (new), the submodule pin.

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Port the scanline functions, `draw_vpatch`, the overlay walk, the wipe and
   `new_frame_stuff` to write palette indices into a 320-byte line, dropping the palette
   lookup and the interpolator path.
2. Feed the lines to `fcvideo_line_sink()`; keep the existing frame-ownership semaphores.
3. Extend the host runner's `--dump-8bit` to write the composed frame as PNG.
4. Goldens: hashes for every tenth frame of the first 600 of DEMO1, plus a PNG every
   hundredth for humans. Check the title screen, the menu, the status bar and a wipe against
   `chocolate-doom` screenshots once by eye, then rely on the hashes.

## Acceptance

Every command must pass from the repository root.

```
build-host-engine/.../fcpico_doom_host --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 600 --lockstep --dump-8bit /tmp/f
python3 doom/tests/goldens/check.py /tmp/f
```

## Traps

Keep everything reachable during `VIDEO_TYPE_SAVING` out of flash: the original marks
those functions `__no_inline_not_in_flash_func` because they run while flash is being
programmed. Losing that attribute produces a crash only on hardware, only when saving.
