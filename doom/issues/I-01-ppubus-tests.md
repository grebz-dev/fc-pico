<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-01 Tests for the PPU-bus model

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | S |
| **Depends on** | none |
| **Work plan task** | P0-T7 |

## Goal — complete; calibrated by I-19

The model tests now pin both internal consistency and the real trace: 66 pre-render reads,
64 reads per visible line, 15,491 physical reads and a zero-based report of 15,490.

## Specification

`plan/09-testing-ci.md` section "L3 -- PPU-bus model"; `plan/01-constraints.md`
sections "The bus contract", "Prior-art evidence" and "Hardware calibration of the read
count".

## Owns (create or modify only these)

`tests/tools/test_ppubus.py` (new); `sim/ppubus/ppubus.py` (fixes only, no redesign)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Read `sim/ppubus/ppubus.py` and trace 6's fixture result.
2. Test the calibrated defaults: 66 pre-render, 64 per visible line, 15,491 physical
   reads for v1 and a zero-based report of 15,490. For v2, the reported value is 15,554.
   Check frame event order, mailbox extraction, visible pixels and the effect of a dropped
   or duplicated read.
3. Assert that transmitted bytes and counted reads use the same CS1-qualified `/RD` edge;
   the stream has no normal-frame zero-byte prefix. Check that manual nudges advance one
   or two buffer bytes.
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
