<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-01 Tests for the PPU-bus model

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | S |
| **Depends on** | none |
| **Work plan task** | P0-T7 |

## Goal

The model is the only thing standing between the plan's bus arithmetic and a hardware
session, and it is currently unexercised. Pin its behaviour down so a later calibration
changes numbers in one place and the tests say what broke.

## Specification

`plan/09-testing-ci.md` section "L3 -- PPU-bus model"; `plan/01-constraints.md`
sections "The bus contract", "Prior-art evidence" and "A four-byte prelude from the PIO
itself". The module exists and is documented; it has no tests.

## Owns (create or modify only these)

`tests/tools/test_ppubus.py` (new); `sim/ppubus/ppubus.py` (fixes only, no redesign)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Read `sim/ppubus/ppubus.py` fully, including its UNCALIBRATED docstring.
2. Write tests for: `frame_read_count()` equals 15490 with the default parameters and
   `mailbox_len=64`, and 15554 with `mailbox_len=128`; the first `osr_prelude_bytes` reads
   of a frame are zero and are counted; `frame_events()` yields prelude, then picture, then
   the NMI reads, in that order and in the right quantities; `run_frame(SimpleCart(buf))`
   reconstructs the buffer's visible pixels and its mailbox; a dropped or duplicated read
   changes the count by exactly that amount and nothing else.
3. Add one test that documents the open contradiction rather than hiding it: assert that
   `reads_per_line * 241 + 2 == PPU_PICTURE_COUNT` for the default 64, and that the
   consumed-bytes-per-line parameter is independent of it. If the module does not separate
   the two, that is the bug to fix.
4. Fix defects you find in the module; do not restructure it.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/tools -q
python3 -m pytest doom/tests doom/sim/pioemu -q
```

## Traps

`PPU_COUNT_VAL` is not a free parameter: it comes from `fcbus/fcbus_protocol.h` via
`tools/fcpico/protocol.py`. Import it, never retype it.
