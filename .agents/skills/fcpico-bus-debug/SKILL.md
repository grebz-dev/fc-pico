---
name: fcpico-bus-debug
description: Diagnose FC PICO cartridge-to-console link problems - wrong or unstable PPU read counts (expected 15490), DMA stops and resyncs, missing heartbeats, invalid mailboxes, garbled or shifted pictures, attribute or palette offsets, PIO and chip-select (CS1) behaviour, data-mode exchanges. Use this whenever a console shows a black screen, PICO NOT FOUND, tearing, a picture shifted by bytes, flickering commands, or serial stats with stops/resyncs, or whenever someone changes rp_system, fcppu.pio, the bus core or the boot ROM's NMI.
---

# FC PICO bus debugging

The link has no shared clock: the cartridge counts CS1-qualified PPU reads and
re-arms its DMA only when a frame's count is within ±2 of 15490. Reference:
`docs/pages/protocol.md` and `docs/pages/platform-findings.md`.

## Collect evidence first

- The configuration: console model and region, boot ROM stamp, firmware build, and
  whether this is hardware or the emulator.
- Counts and counters: serial stats on hardware; on the emulator, `trace.csv` from
  `sim/mesen/scenario.py` (`last_count`, `dma_stops`, `resyncs`, heartbeats).
- A host reproduction where possible: `platform/tests` and the cartridge model tests
  run the same bus core in milliseconds.
- When the bad counts happen: steadily, or right after palette, attribute or scene
  changes (data mode), input, saves or other application events.

## Reason from the accounting

A frame is 66 pre-render reads + 240 x 64 visible reads = 15426 picture reads, then
the NMI's dummy read and 64 mailbox reads: 15491 physical reads, reported zero-based
as 15490. Compare the observed count with that:

- off by 1-2 → nudges handle it; persistent offsets point at the stream start;
- small counts (tens to a few hundred) around a palette or attribute change → data
  mode, which is expected. The boot ROM blanks the picture for it, so those frames
  count only the mailbox and the transfer, and the extra empty round that can follow
  ("`PF_COM_DMOD` can be sent once more" in `platform-findings.md`) counts 71. Data
  mode does not produce 15490 plus a few;
- far off or tiny → the selection is wrong (only `$0800`-`$0FFF` is the cartridge),
  rendering is off, or the console is not running the ROM you think;
- correct counts but wrong pixels → layout (`31 + 32 * y + x` words) or encoding.

Known traps: debugger reads must not reach the cartridge; attribute pages arrive one
byte late and the ROM discards one byte; the shipped ROM ignores `PF_COM_VRAM`.

## Steps

1. Reproduce at the cheapest level that shows the symptom: host test, then emulator,
   then hardware.
2. State one hypothesis that explains the numbers, and the check that would refute
   it. Rule alternatives out with a run where you can, not only by reading code. Use
   the host cartridge model for the cartridge's logic (nudges, stops, resyncs); its
   test console does not blank the picture for data mode, so take console-side counts
   from the emulator, which runs the real boot ROM (`fcpico-cosim`).
3. Run that check and record the result.
4. Do not change the count window or skip reads to make numbers fit; fix the cause.

## Done when

You can give the observed counts and addresses, the hypothesis, the evidence for or
against it, and the next bounded check (or the fix and the test that now passes).
Label what was measured on hardware and what was only simulated.
