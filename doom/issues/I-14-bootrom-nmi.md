<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-14 The v2 vertical-blank handler

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | L |
| **Depends on** | I-13 |
| **Work plan task** | P2-T2 |

## Goal

This is the frame heartbeat, the attribute and palette upload, and the APU replay -- the
console half of protocol v2. It either fits in vertical blank or the picture tears, and the
harness that measures it already exists and is calibrated against the tutorial ROM.

## Specification

`plan/07-bootrom.md`: the assembly draft, the cycle table (1595 critical, 1975 total
with 15 APU pairs) and the init sequence including the OAM parking step.

## Owns (create or modify only these)

`bootrom/src/SysNMI.asm`, `bootrom/src/SysMain.asm` (or equivalents), `tests/bootrom/test_doom_nmi.py` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. `tests/bootrom/nmi_harness.py` is shared: extend it, do not restructure it.

## Steps

1. Implement the handler from the draft in the plan. Keep all `$2006`/`$2007` traffic before
   the `$2000`/`$2005` restore, and the controller packet last.
2. Implement the init sequence, including parking OAM at Y = `$EF` once (sprites stay
   enabled for fetch-pattern fidelity, so unparked OAM is visible garbage).
3. Extend `nmi_harness.py` for the 128-byte mailbox split across `$20`-`$5F` and `$C0`-`$FF`,
   and measure: the critical section (up to and including the second `$2005` write) must be
   at most `NMI_CRITICAL_CYCLES_MAX`, the total at most `NMI_TOTAL_CYCLES_MAX`.
4. Assert the register order and that nothing touches `$2006`/`$2007` after the packet.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/bootrom -q
# and the doom-bootrom workflow stays green
```

## Traps

The budget constants live in `fcbus/fcbus_protocol.h` and reach the assembler through
`gen/protocol.inc`. Read them from there in the test too, so one edit moves all three.
