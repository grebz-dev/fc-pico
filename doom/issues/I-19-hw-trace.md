<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-19 Hardware trace capture and model calibration

| | |
|---|---|
| **Lane** | C -- human, hardware |
| **Size** | M |
| **Depends on** | I-10 |
| **Work plan task** | P0-T9 / P0-T10 |

## Goal

The project's largest open question. `plan/01-constraints.md` now states two readings of the
frame read count that cannot both be true, and no amount of reasoning settles it: 15490
counted reads per frame is incompatible with a 68-byte line stride, and the alternative
(64 counted reads per line) is incompatible with the picture not shearing. Everything that
claims to model the bus is provisional until a real console is measured.

## Specification

`HARDWARE-REQUESTS.md` HR-1; `plan/09-testing-ci.md` "The trace sampler,
concretely"; `plan/01-constraints.md` "An unresolved discrepancy" and the two irreconcilable
identities recorded there.

## Owns (create or modify only these)

`tests/fixtures/hw_trace_ntsc/*`, `HARDWARE-LOG.md`, `plan/01-constraints.md` (the calibration result), `sim/ppubus/ppubus.py` (parameter defaults)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Flash `fcpico_testpattern.uf2` (I-10) and confirm a pattern appears on a genuine
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
