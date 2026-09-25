<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-19 Hardware trace capture and model calibration

| | |
|---|---|
| **Lane** | C -- human, hardware |
| **Size** | M |
| **Depends on** | [I-10](I-10-testpattern-firmware.md) |
| **Work plan task** | [P0-T9](../plan/10-workplan.md#p0-t9-hardware-trace-capture-hw) / [P0-T10](../plan/10-workplan.md#p0-t10-model-calibration) |

## Status (2026-09-24)

**done**. NES-001 trace 6 calibrated the 66/64 selected-read cadence and zero-based count. See [`../PROGRESS.md`](../PROGRESS.md) for dated evidence.

## Goal — complete 2026-09-22

Resolve the former read-count contradiction with a real NES-001 trace and calibrate the
model and co-simulation to it. Trace 6 measures 66 pre-render and 64 visible-line reads;
the strict S0 count, mailbox, fetch and screenshot gates now pass.

## Specification

[`HARDWARE-REQUESTS.md`](../HARDWARE-REQUESTS.md) [HR-1](../HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9); [`plan/09-testing-ci.md` "The trace sampler, concretely"](../plan/09-testing-ci.md#the-trace-sampler-p0-t9-concretely); [`plan/01-constraints.md` "Hardware calibration of the read count"](../plan/01-constraints.md#hardware-calibration-of-the-read-count-measured-2026-09-22).

## Owns (create or modify only these)

`tests/fixtures/hw_trace_ntsc/*`, [`HARDWARE-LOG.md`](../HARDWARE-LOG.md), [`plan/01-constraints.md`](../plan/01-constraints.md) (the calibration result), `sim/ppubus/ppubus.py` (parameter defaults)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Flash `fcpico_testpattern.uf2` ([I-10](I-10-testpattern-firmware.md)) and confirm a pattern appears on a genuine
   Famicom.
2. Run `stats` for a minute and capture `trace` three times; record `picotool info -a`, the
   reported flash size and the console model.
3. Commit the dumps as fixtures; run `tools/trace_decode.py` and record the per-line
   qualifying-read count, the /RD low width and the vblank gap.
4. Fit `reads_per_line`, `prerender_reads`, `cs1_mask` and `osr_prelude_bytes` to the trace;
   test the cheap hypothesis first, that CS1 decodes only `$0000`-`$0FFF` and so excludes
   every sprite fetch.
5. Rewrite the "unresolved discrepancy" section as a resolved one, and drop the model's
   UNCALIBRATED marker.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/tools -q   # the model's tests, now against a real trace
```

## Traps

Capture before changing anything. A trace taken with the tutorial firmware on an
unmodified console is the reference; a trace taken after a change measures the change.
