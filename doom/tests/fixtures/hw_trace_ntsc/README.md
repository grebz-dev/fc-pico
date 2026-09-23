<!-- SPDX-License-Identifier: BSD-3-Clause -->
# NTSC hardware trace fixture

This directory holds the real-console evidence requested by HR-1 in
`doom/HARDWARE-REQUESTS.md`.  Keep serial output verbatim so the decoder and future
co-simulation changes can be checked against the original capture.

Expected files:

- `stats.txt` -- startup/visual observations and one or more complete `stats` lines.
- `trace_1.hex`, `trace_2.hex`, `trace_3.hex` -- the text from `TRACE ...` through
  `END TRACE`, inclusive, produced by three separate `trace` commands.
- `tables.txt` -- the small `dump pal`, `dump attr`, and `dump mailbox` outputs.
- `picotool.txt` -- `picotool info -a` output for both the UF2 and connected board.

Decode each completed trace from the repository root with:

```sh
python3 doom/tools/trace_decode.py \
  doom/tests/fixtures/hw_trace_ntsc/trace_1.hex --json
```

Do not trim repeated values from `stats.txt`; the distribution and rare outliers are
part of the measurement.

The initial 2026-09-22 captures used a 65,536-word/15.7 ms dump.  All three are
incomplete: trace 1 has only 60,414 declared words despite its terminator, and traces 2
and 3 end before `END TRACE`.  The firmware now captures 8,192 words/2.0 ms (about 31
NTSC scanlines) so a complete line remains measurable without overrunning typical
terminal capture limits.  `trace_decode.py` must accept a file before it is used as a
fixture.
