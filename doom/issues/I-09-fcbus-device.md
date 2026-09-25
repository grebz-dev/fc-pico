<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-09 Device backend for the bus

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | L |
| **Depends on** | [I-08](I-08-device-superbuild.md) |
| **Work plan task** | [P0-T5](../plan/10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware) |

## Status (2026-09-24)

**done**. Test patterns and the v2 Doom stream run on an NES-001 with stable selected-read counts. See [`../PROGRESS.md`](../PROGRESS.md) for dated evidence.

## Goal

The core and the host backend are done and tested. The device backend is the one piece that
turns them into a cartridge, and it is the last thing before a test pattern can be put on a
real console.

## Specification

[`plan/02-architecture.md`](../plan/02-architecture.md) (["Core assignment and interrupt priorities"](../plan/02-architecture.md#core-assignment-and-interrupt-priorities)),
[`plan/01-constraints.md`](../plan/01-constraints.md) (["The bus contract"](../plan/01-constraints.md#the-bus-contract-fixed-abi-from-docspagesprotocolmd), ["Hardware calibration"](../plan/01-constraints.md#hardware-calibration-of-the-read-count-measured-2026-09-22)), and
`tutorial_project/tuto1_hw/sys/rp_system.cpp` -- `init()`, `ppu_dma()`, `jobRcvCom()`,
`rom_dma()`, `ver_dma()`, `drq_ret()` -- plus `rp_dma.cpp`. `fcbus_core` already implements
every decision; this is the PIO, DMA and interrupt plumbing around it.

## Owns (create or modify only these)

`fcbus/fcbus_device.c`, `fcbus/fcbus_device.h`, `fcbus/CMakeLists.txt` (the device target)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. Do not modify `fcbus_core.*` in this
issue. If the device backend requires a core API change, split that work into a separate
prerequisite issue and complete it before continuing this issue.

## Steps

1. Bring up PIO0 with the four programs from `fcppu.pio`, configured exactly as
   `rp_system::init()` does (in/out pin bases, jmp pins, FIFO joins, shift directions and
   autopush/autopull thresholds). Generate the header with `pioasm` at build time; the
   checked-in `fcppu.pio.h` is stale by the project's own documentation.
2. One DMA channel to `SM_TRAN`'s TX FIFO, re-armed per heartbeat from the core's front
   buffer, with the two manual `out pins, 8` nudges on a short count.
3. The receive interrupt on `PIO0_IRQ_0`, servicing `fcbus_core_rx_byte` and acting on the
   actions it returns. Everything reachable from the interrupt goes in RAM
   (`__not_in_flash_func`), and it must never print or wait.
4. Sample the `SM_TRCNT` counter the way `ppu_dma()` does (`push noblock`, `mov x, !null`,
   restart) and hand it to the core through `fcbus_core_set_read_count()`.
5. Record in a comment, for each of the five things above, which tutorial line it mirrors.

## Acceptance

Every command must pass from the repository root.

```
# then: the doom-device workflow builds fcbus_device into a firmware image
# and the host lane is unaffected:
ctest --test-dir build-host --output-on-failure
```

## Traps

Hardware calibration supersedes the earlier four-byte-prelude hypothesis: normal frame
re-arm begins at buffer byte zero. Manual nudge instructions consume one or two real buffer
bytes. Keep the host backend aligned with strict S0 and the NES-001 trace arithmetic.
