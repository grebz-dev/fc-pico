<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-09 Device backend for the bus

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | L |
| **Depends on** | I-08 |
| **Work plan task** | P0-T5 |

## Goal

The core and the host backend are done and tested. The device backend is the one piece that
turns them into a cartridge, and it is the last thing before a test pattern can be put on a
real console.

## Specification

`plan/02-architecture.md` ("Core assignment and interrupt priorities"),
`plan/01-constraints.md` ("The bus contract", "A four-byte prelude"), and
`tutorial_project/tuto1_hw/sys/rp_system.cpp` -- `init()`, `ppu_dma()`, `jobRcvCom()`,
`rom_dma()`, `ver_dma()`, `drq_ret()` -- plus `rp_dma.cpp`. `fcbus_core` already implements
every decision; this is the PIO, DMA and interrupt plumbing around it.

## Owns (create or modify only these)

`fcbus/fcbus_device.c`, `fcbus/fcbus_device.h`, `fcbus/CMakeLists.txt` (the device target)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. Do not change `fcbus_core.*` behaviour: if the core needs something, add to it without altering what the existing tests assert.

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

The prelude is not optional bookkeeping: `pio_sm_restart()` clears the OSR, so the first
four bytes after every re-arm come from the state machine, not the buffer. The host backend
models this (`FCBUS_OSR_PRELUDE_BYTES`); the device must agree with it or the two diverge
silently.
