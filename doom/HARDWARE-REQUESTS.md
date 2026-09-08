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

## Open requests

### HR-1 Phase 0 trace capture and board facts (task P0-T9)
- Flash: `fcpico_testpattern.uf2` from the P0-T5/P0-T9 build (not yet available).
- Console ROM expected: tutorial (unchanged).
- Steps: 1. Insert the cartridge, power on, confirm the test pattern is visible.
  2. Connect USB-C, open the serial console at 115200, run `stats` for 60 s and paste the
     output. 3. Run `trace` three times; save each dump as `trace_<n>.hex`.
  4. Run `picotool info -a` on the UF2 and on the connected board; paste both.
- Record: `doom/tests/fixtures/hw_trace_ntsc/` (dumps + `stats.txt` + `picotool.txt`), and the
  console model/revision in `HARDWARE-LOG.md`.
- Result: (pending)

## Human actions (not hardware)

### HA-1 Licensing enquiry (task P0-T13)
- Ask impact soft (product page / X: @HD64180, see `docs/pages/references.md`) whether the FC
  PICO sample sources (bus layer, boot ROM) may be redistributed in a GPLv2 combined work, and
  whether they object to a modified boot ROM being installed by a third-party firmware.
- Result: (pending)
