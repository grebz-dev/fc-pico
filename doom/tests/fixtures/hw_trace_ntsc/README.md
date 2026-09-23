<!-- SPDX-License-Identifier: BSD-3-Clause -->
# NTSC hardware trace fixture

This directory holds the real-console evidence requested by HR-1 in
`doom/HARDWARE-REQUESTS.md`.  Keep serial output verbatim so the decoder and future
co-simulation changes can be checked against the original capture.

Expected files:

- `stats.txt` -- startup/visual observations and one or more complete `stats` lines.
- `trace_6.hex` -- the complete 8,192-word calibration capture.
- `trace_6.json` -- its checked-in decoder result.
- `trace_1.hex` through `trace_5.hex` -- retained incomplete diagnostic captures;
  they are not calibration fixtures.
- `tables.txt` -- the small `dump pal`, `dump attr`, and `dump mailbox` outputs.
- `picotool.txt` -- `picotool info -a` output for both the UF2 and connected board.

Decode each completed trace from the repository root with:

```sh
python3 doom/tools/trace_decode.py \
  doom/tests/fixtures/hw_trace_ntsc/trace_6.hex --json
```

Do not trim repeated values from `stats.txt`; the distribution and rare outliers are
part of the measurement.

The initial 2026-09-22 captures used a 65,536-word/15.7 ms dump.  All three are
incomplete: trace 1 has only 60,414 declared words despite its terminator, and traces 2
and 3 end before `END TRACE`.  The firmware now captures 8,192 words/2.0 ms (about 31
NTSC scanlines) so a complete line remains measurable without overrunning typical
terminal capture limits. Traces 4 and 5 still lost 28 and 21 packed words respectively;
trace 6 is complete and is the only raw calibration input.

Trace 6 measures 66 CS1-qualified reads on the pre-render line and 64 on each of
14 complete visible lines. `/RD` is low for 4-5 samples (160-200 ns) on 2,705 of
2,706 captured pulses, ruling out a missed-edge explanation. Combined with the
firmware's stable `ppu_count=15490`, this gives:

```
66 + 240*64 + (1 dummy + 64 mailbox) = 15491 physical reads
15491 - 1 zero-based counter-report bias = 15490 reported
```
