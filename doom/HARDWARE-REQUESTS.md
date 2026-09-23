# Hardware requests

Requests from the executing agent to a human with a Famicom/NES and the FC PICO cartridge.
Each request must be self-contained: what to flash, what to run, what to record, where to put
the result. The human fills in the result section and commits; the agent then continues.

## Template

```
### HR-<n> <title>  (task <id>)
- Flash: <artifact path or CI run link>
- Console ROM expected: <tutorial | doom v2 | doom v3>
- Steps: 1. ... 2. ...
- Record: <files / values / photos>, commit under <path>
- Result: (human fills in) date, console model, outcome, attachments
```

## Completed requests

### HR-1 Phase 0 trace capture and board facts (task P0-T9)
- Flash: `/tmp/fcpico-doom-device-resume/port/fcpico_testpattern.uf2` from the current
  P0-T5/P0-T9 build (SHA-256 `be283708ba21e4ffa75ad4dec665461cd52db9220888d86888096f8051389cbf`).
- Console ROM expected: tutorial (unchanged).
- Steps: 1. Insert the cartridge, power on, confirm the test pattern is visible.
  2. Connect USB-C, open the serial console at 115200, run `stats` for 60 s and paste the
     output. 3. Run `trace` three times; save each complete dump as `trace_<n>.hex` and
     validate each with `python3 doom/tools/trace_decode.py <dump> --json`.
  4. Run `picotool info -a` on the UF2 and on the connected board; paste both.
- Record: `doom/tests/fixtures/hw_trace_ntsc/` (see its `README.md`; dumps + `stats.txt` +
  `picotool.txt`), and the console model/revision in `HARDWARE-LOG.md`.
- **Sharpened by co-simulation (2026-09-20).** The trace no longer has to explore the whole
  count discrepancy; two of the three candidate explanations in `plan/01-constraints.md` can
  be settled or have already been settled off the bench:
  - The counter's qualifier is not in doubt. `fcppu_rna` counts a read exactly when CS1 is
    low (`sm_config_set_jmp_pin(&cn, PI_CS1_BIT)` in the tutorial's `rp_system.cpp`), so
    whatever the count is, it is "CS1-low reads per frame" and nothing else.
  - The cheap CS1 hypothesis is dead as an explanation of the count. Running the real
    tutorial ROM against a cycle-accurate PPU gives 16388 selected reads per frame for a
    `$0000`-`$0FFF` decode and 20244 for `$0000`-`$1FFF`. A narrow decode is still needed to
    get the 68-bytes-per-line stream layout -- the wider one proves the sprite fetches really
    do sit at `$1000` -- but narrowing it cannot reach 15426, and no address mask can.
  - What is left: either the PIO counter misses strobes (most likely the two-tile prefetch
    at dots 321-336), or the frame is not consumed as 241 lines.
  So the one measurement that decides it is **the time distribution of CS1 strobes within a
  single line**: does a counted strobe appear for the dots 321-336 pair, and is its spacing
  different from the dots 1-256 pairs? Please make sure at least one `trace` dump covers a
  full scanline including dots 241-340, and note the `stats` `ppu_count` histogram verbatim
  even when it looks boring -- a histogram centred on 15490 versus 16452 answers this on its
  own.
- **First hardware result (2026-09-22).** An NTSC NES-001 reports 15490 on 5495 of 5507
  measured frames; the other 12 land in the low outlier bucket and trigger DMA stops.
  This validates `PPU_COUNT_VAL_V1=15490` and turns the Mesen 16453 result into a model
  mismatch, not a candidate hardware constant.  The first three 65,536-word trace dumps
  were truncated by the serial capture path, so the firmware now emits an 8,192-word,
  approximately 2 ms window that still spans about 31 scanlines.
- Result: complete on 2026-09-22. The replacement firmware from `73a6df2` displays its test
  patterns successfully on the NES-001. Traces 4 and 5 lost 28 and 21 packed words; trace 6
  is complete and measures 66 pre-render reads and 64 reads on every complete visible line.
  The calibrated strict Mesen S0 subsequently passes at `ppu_count=15490`.

## Open requests

### HR-2 Corrected stream geometry visual check (task I-17)
- Flash: `/tmp/fcpico-doom-device-resume/port/fcpico_testpattern.uf2`, built from `42fa091`;
  SHA-256 `a1f129761e70c0c3f6aa65b02e7e69cfe9e93cd244da08ddcb1bc66d25d7b2e9`.
- Console ROM expected: tutorial (unchanged).
- Steps: 1. Flash the UF2 and start the NES-001 with the cartridge inserted. Confirm the
  default moving bars appear. 2. At the 115200-baud CLI, run `pattern 1` and confirm the
  checkerboard appears; then try `pattern 0` to return to bars. 3. Run `stats` after about
  60 seconds and copy the complete line.
- Record: console model, whether each pattern appears and fills the expected picture area,
  any tearing or horizontal shift, and the `stats` line in `HARDWARE-LOG.md`. A photo is
  useful if alignment looks wrong.
- Result: pending.

## Human actions (not hardware)

### HA-1 Licensing enquiry (task P0-T13)
- Ask impact soft (product page / X: @HD64180, see `docs/pages/references.md`) whether the FC
  PICO sample sources (bus layer, boot ROM) may be redistributed in a GPLv2 combined work, and
  whether they object to a modified boot ROM being installed by a third-party firmware.
- Result: (pending)
