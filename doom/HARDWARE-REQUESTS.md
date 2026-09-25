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

### HR-4 Controller input on Doom (task P1-T4)
- Flash: `/tmp/fcpico-hw-doom/fcpico_doom_input_whx.uf2`, SHA-256
  `82405a0823193a0d7ea7c6b8f5c053450e7d049399fa23d1d7f21973caf5fbd8`.
  Send `bootsel` followed by Enter on the currently running Doom firmware's USB
  serial port, then copy this UF2 to the RP2350 drive with the NES off. The Doom
  console ROM stamp remains `20DOOM-02-0002`, so a ROM erase/update is not expected.
- Steps: 1. Power on the NES after firmware startup and confirm Doom playback.
  2. Press Start to open the menu; use the D-pad to move and A to choose a new
  game. 3. In E1M1, hold Up to move, Left/Right to turn, A to fire, tap B to
  use, and hold B with Left/Right to strafe. 4. Tap Select to change weapon;
  hold Select for automap. 5. Record a few serial `video frames` or
  `[DEBUG-hr3]` lines after using the pad.
- Record: console model, which actions worked, any missed/held inputs, whether
  the picture stayed stable, and serial lines in `HARDWARE-LOG.md`.
- Result: pending physical controller validation.

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

### HR-3 First Doom boot: v2 reflash and engine display (tasks P2-T4, P1-T3, P1-T6)
**Current candidate (2026-09-24 review):**
`/tmp/fcpico-hw-doom/fcpico_doom_streamfix_delay_bootsel_whx.uf2`, ROM
`20DOOM-02-0002`, SHA-256
`1221d341ac281b28cdd0712410aefa7b91b3fab99e7f0a15c76ad8e07e593127`.
This image accepts `bootsel` followed by Enter on its USB serial port, both during
the 30-second startup delay and while Doom is displaying frames. The USB drive
then appears for the next UF2 without removing the cartridge. The firmware
currently on the cartridge must first be put into BOOTSEL using its existing
method; installing this image enables the serial command for later updates.
This supersedes the raw-count-only diagnostic below. Review found two console-ROM bugs:
tile `$00` selected the font area instead of the `$0800` stream, and the v2 heartbeat left
the PPU address four rendering reads out of phase. Both are fixed and reproduce/pass in
the corrected address-based Mesen mapper. See `PROGRESS.md` for before/after evidence.

For the next hardware validation, keep the NES off while flashing; wait through the
30-second startup delay, then power on. The new stamp should cause one complete ROM
erase/update before booting Doom; let it finish. Record the screen result and several
`[DEBUG-hr3]` lines after heartbeats start. Expected steady result: `count=15554`, increasing
`converted`, and no increase in `stops` after startup. The `raw` counter remains available
if these fixes expose another issue. A later NES power cycle should not reflash again.

Rebuilt recovery image: `/tmp/fcpico-hw-doom/recovery_fcpico_testpattern.uf2`, SHA-256
`00a8bb2d7e476d7b96f4b0352422c80fa3210d901d3cf0ab3e2f2596a8a4b257`.
It serves the original tutorial ROM; the reverse reflash passed in Mesen. The fixed
`$F000` bank is unchanged. Physical validation of the new image is still pending.

**Earlier candidates and results (history):**

- Flash: `/tmp/fcpico-hw-doom/fcpico_doom_whx.uf2` (firmware plus `doom1.whx` at
  `0x10080000` in one file), SHA-256
  `6bcadd150cf31ef39b280862e24bf1e6d0b9dbfdbf5807552fa6283836659bac`. Built from
  `2910484` plus the uncommitted 2026-09-23 S1 changes, ROM stamp `20DOOM-02-0001`.
- Recovery: `/tmp/fcpico-hw-doom/recovery_fcpico_testpattern.uf2`, SHA-256
  `c62b0f1d350711d5c411db9c244b1bbbeea94c4575525a5c9340d4e8f3041206`. It serves the
  tutorial ROM, so a console holding the Doom ROM reflashes itself back (co-simulated).
  If a reflash is interrupted the fix bank retries on the next boot; holding Start at
  power-on forces a reflash from whatever firmware is installed.
- Console ROM expected: tutorial before the first boot, Doom v2 afterwards.
- Steps: 1. Hold BOOTSEL, drag the UF2 to the RP2350 drive; wait for it to reboot. 2. Insert
  the cartridge and power the NES-001. Expect the `MEMORY` test, then about 3 s of two stamps
  on screen (tutorial date vs `20DOOM-02-0001`), then `ROM ERACE`, `ROM UPDATE`,
  `ROM UPDATE END`, the memory test again, then the Doom demo loop. Do not power off during ERACE/UPDATE. 3. Watch at least one
  full demo; note picture, colour, stability and approximate frame rate. Input and sound are
  not implemented: pads do nothing and there is no audio. 4. Power-cycle once and confirm it
  boots straight to Doom with no reflash. 5. If a USB serial port appears, capture its
  output. 6. Optionally flash the recovery UF2 and confirm the console returns to the
  tutorial ROM.
- Record: every on-screen message and where it stopped if it did (a photo of any text),
  whether Doom appears, any tearing/rolling/wrong colours, serial output, in `HARDWARE-LOG.md`.
- Result: the current candidate displays Doom on NES-001. USB serial reports
  steady v2 `count=15554`, increasing converted frames, and no further DMA
  stops after startup. See `HARDWARE-LOG.md` for the transcript and conversion
  timing. Visual quality, extended stability, and input remain to be checked.
- Diagnostic 1: flash `/tmp/fcpico-hw-doom/fcpico_diagnostic_usb_bus.uf2`
  (SHA-256 `94fbf9d92b7cfb704781e3abee0edc24d32bc1fb130f21b813af146dd0fb5630`).
  With the NES off, connect USB and check whether a serial port enumerates and
  emits `FC PICO diagnostic: USB and bus initialized, uptime=N s` every second.
  This build initializes the same USB and cartridge bus but deliberately skips
  the Doom engine. Record whether the board LED blinks and the exact Device
  Manager status. Keep the NES off during this diagnostic; it serves the Doom
  ROM but does not run the game or complete a reflash.
- Diagnostic result: with the NES off, USB serial enumerates, the uptime line
  advances, and the LED blinks. This isolates the USB failure to engine startup
  or subsequent execution.
- Diagnostic 2: flash
  `/tmp/fcpico-hw-doom/fcpico_diagnostic_engine_delay_whx.uf2` (SHA-256
  `589f43b3535f8cca5c346a217d56cdac157a32160a6640cec46ddcdf2f31742b`).
  With the NES off, open serial during the 30-second countdown. Capture all
  output before and after `entering D_DoomMain`, and report whether the COM
  port disappears or Device Manager shows a descriptor error. This build
  includes WHX and runs the normal Doom path after the countdown.
- Diagnostic result: USB serial captured startup through `ST_Init: Init status
  bar.`, then `*** PANIC *** double support is disabled`; the COM port remained
  available although the terminal application crashed. `fcvideo_build_tables()`
  calls double operations on the first frame while the old target linked
  `pico_double_none`.
- Fix candidate: flash `/tmp/fcpico-hw-doom/fcpico_doom_doublefix_delay_whx.uf2`
  (SHA-256 `1214bc40ed2e01fe5c93296a77f8340edcb3e5d1c28996a0ba1429797d3b297b`).
  With the NES off, capture serial past `ST_Init`; note any further panic,
  the last line printed, and whether USB remains enumerated. With no NES
  heartbeats, the frame-publication counter may stay below its print threshold.
  This build selects the RP2350 Pico double implementation and retains the
  30-second startup countdown.
- Fix candidate result: with the NES off, startup passed `ST_Init` without a
  panic; the COM port remained available and the terminal stayed responsive.
  Next, boot the NES with this same UF2 after the countdown completes and
  capture every on-screen message and serial output through the first Doom
  frame or failure. Do not interrupt `ROM ERACE`/`ROM UPDATE` if they appear.
- Console result: reflash loops. After `ROM UPDATE END`, the console's local
  stamp is `0DOOM-02-0001` versus the cartridge's `20DOOM-02-0001`; it erases
  `$8000` through `$E000` and retries. Serial stays available but prints no
  later engine output. See `HARDWARE-LOG.md`. The fix bank makes 257 `$2007`
  fetches per 256-byte page, whereas the device supplied exactly 256 bytes;
  a one-word DMA tail is the next candidate to test.
- ROM-tail candidate: flash `/tmp/fcpico-hw-doom/fcpico_doom_romtail_delay_whx.uf2`
  (SHA-256 `19ae9617a39e720c7eeb9cf13f3c8521a8ba84877dd00aa7f9a978350254ca6f`).
  With the NES off, wait for serial startup to pass `ST_Init`, then power on
  the NES. Record whether `ROM UPDATE` finishes once, the local and cartridge
  stamps after the memory test, and whether Doom appears. Let any erase/update
  pass finish before powering off. This image retains the 30-second countdown
  and the double-support fix; ROM page replies now include one extra DMA word.
- ROM-tail result: the NES completed one erase and update, reset, then showed
  a black screen. A fresh NES power cycle showed `MEMORY 2048B OK`, a brief
  flash of horizontal lines, then black, without another reflash. USB serial
  stayed connected with no output beyond `ST_Init`; the RP2350 countdown did
  not restart when only the NES power was cycled. Post-update stamps were not
  observed.
- Post-boot diagnostic, if a fresh power cycle remains black: flash
  `/tmp/fcpico-hw-doom/fcpico_doom_postboot_diag_whx.uf2` (SHA-256
  `258da0e0f263329fc58d80447a953c5e00d497c4af550872d3c1e15639cc0b1b`).
  With the NES off, wait for startup past `ST_Init`, then power on the NES and
  capture all `[DEBUG-hr3]` serial lines for about 10 seconds. This image keeps
  the ROM-tail and double fixes and adds frame-loop and bus counters; report
  whether the console displays `MEMORY`, stamps, a reflash, or black directly.
- Post-boot result: Doom's frame loop ran and one frame was converted. The bus
  initially showed no initialization or heartbeats. Later, initialization
  arrived and heartbeats increased, but every PPU read count was zero and every
  frame stopped DMA. `converted` increased on each NES power cycle or reset,
  independently of cartridge movement. The firmware was resetting the counter
  for each byte of the three-byte v2 heartbeat, so its final byte saw zero.
  Protocol errors while moving the cartridge are not independently diagnostic.
- Heartbeat-counter fix candidate: flash
  `/tmp/fcpico-hw-doom/fcpico_doom_heartbeatfix_delay_whx.uf2` (SHA-256
  `998ccb49b86c12a4df9ec632a3a7d4af50ed0d3e86bffe3b38c5cc69098c1136`).
  Seat the cartridge with the NES off. Keep the NES off while flashing; wait
  until serial startup passes `ST_Init`, then power on the NES. Record whether
  the screen advances past `MEMORY 2048B OK` and the first several `[DEBUG-hr3]`
  lines after `init` or `hb` becomes nonzero, especially `count`, `stops`,
  `resyncs`, and `converted`. This image retains the ROM-tail and double fixes,
  30-second startup delay, and bus diagnostics.
- Heartbeat-counter result: `count` rose from zero to exactly 128 after NES
  power-on, with `init=1` and hundreds of heartbeats, but `stops=hb` and the
  screen remained black. The 128 reads match the boot ROM's mailbox length;
  no qualified rendering reads were counted. The current boot ROM writes
  `$2001=$1E` to enable rendering, so determine whether `/RD` occurs without
  cartridge select or rendering reads are absent.
- Raw-read diagnostic (**superseded by the `0002` candidate above**):
  `/tmp/fcpico-hw-doom/fcpico_doom_rawcount_diag_whx.uf2` (SHA-256
  `d6d3062d135cc0e09db92e27b53f238ae889e38e8f1a8449d56642889701fa98`).
  With the NES off, wait for serial startup past `ST_Init`, then power on the
  NES. After `hb` rises, send two or three `[DEBUG-hr3]` lines containing both
  `count` (CS1-qualified reads) and `raw` (all PPU `/RD` strobes), plus the
  screen result. This build adds a separate PIO1 counter and retains the
  heartbeat, ROM-tail, and double fixes.

## Human actions (not hardware)

### HA-1 Licensing enquiry (task P0-T13)
- Ask impact soft (product page / X: @HD64180, see `docs/pages/references.md`) whether the FC
  PICO sample sources (bus layer, boot ROM) may be redistributed in a GPLv2 combined work, and
  whether they object to a modified boot ROM being installed by a third-party firmware.
- Result: (pending)
