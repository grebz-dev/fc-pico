<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-10 Test-pattern firmware and serial CLI

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | M |
| **Depends on** | I-09 |
| **Work plan task** | P0-T5 / P0-T9 |

## Goal

This is the artifact a human needs in order to answer the project's biggest open question.
`plan/01-constraints.md` documents two irreconcilable readings of the frame read count, and
only a strobe trace from a real console can settle it. Nothing else unblocks calibration.

## Specification

`plan/09-testing-ci.md` sections "L7 -- hardware" (the CLI command table) and "The
trace sampler (P0-T9), concretely"; `HARDWARE-REQUESTS.md` HR-1, which is written against
this firmware.

## Owns (create or modify only these)

`port/main_testpattern.c`, `port/cli.c`, `port/cli.h`, `port/trace.c`, `port/trace.h`, `port/CMakeLists.txt`

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. A firmware that brings up `fcbus_device`, publishes moving test patterns (bars, checker,
   alternating block palettes, a frame counter) and nothing else.
2. The serial CLI from the plan: `stats` (fps, conversion time, the `ppu_count` histogram,
   resyncs, timeouts), `pattern N`, `dump attr|pal|mailbox`, `reboot`, `bootsel`.
3. `trace`: the PIO1 sampler plus DMA described in the plan, dumping one frame of CS1, /RD
   and /WR samples as hex.
4. `tools/trace_decode.py` to turn that dump into per-line qualifying-read counts, a /RD low
   width histogram and the vblank gap -- with a synthetic round-trip test, since no real
   dump exists yet.

## Acceptance

Every command must pass from the repository root.

```
# then: the doom-device workflow produces fcpico_testpattern.uf2, and
python3 -m pytest doom/tests/tools -q   # covers trace_decode against synthetic input
```

## Traps

Write `trace_decode.py` and its test before the hardware session, not after. A session
that produces a dump nobody can parse wastes the scarcest resource in the project.
