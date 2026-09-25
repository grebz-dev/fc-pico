# Hardware log

Appended by whoever runs a console session. Newest first.

```
## <date> -- <milestone / request id>
- Console: <Famicom / AV Famicom / NES-001 + adapter ...>, region
- Cartridge firmware: <uf2 name, git commit>
- Console ROM: <stamp string shown by `stats` or from the ROM UPDATE screen>
- Results: ...
- Attachments: <paths>
```

## 2026-09-24 -- HR-4 controller input and screen photographs
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_input_whx.uf2` (HR-4 candidate)
- Results: input works for movement, strafing and menus. Tapping B does not
  activate use, although B works as a strafe modifier and in menus. The
  supplied Doom photograph shows the game and status bar within the active
  picture, with unused space beneath. A Super Mario Bros. photograph on the
  same display uses more of the vertical picture area.
- Serial: v2 `count=15554` through `hb=2221`; `stops=1`, `resyncs=1`,
  `timeouts=0`, `errors=0` remained fixed. `converted` reached 590. Startup
  drops settled at 856, then rose to 871 before remaining fixed in the later
  capture. Conversion averaged 35574 us with 35612 us maximum at 540 frames.
- Diagnosis: the mapper emits B-use keydown and keyup in the same engine tic.
  A host probe with a scripted B tap observed both matching-space events and
  zero tic commands with `BT_USE`. The converter deliberately places 200 Doom
  lines at NES lines 16..215, leaving 24 lines below; this is a layout choice.
- Attachments: photographs and serial transcript supplied in chat. The local
  image files are `FCPICO_DOOM_HW_PHOTO.jpg` and `MARIO_HW_PHOTO.jpg` (not committed).

## 2026-09-24 -- HR-3 Doom playback on NES-001
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_streamfix_delay_bootsel_whx.uf2` (current HR-3 candidate)
- Console ROM: Doom v2 stamp `20DOOM-02-0002` embedded in the candidate; on-screen
  post-update stamp was not separately transcribed.
- Results: Doom is visibly playing back on the console. USB serial reached
  `D_RunFrame` and `TryRunTics`. Before console initialization, one frame was
  converted and pending while `dropped` climbed to 1973. Once the console
  initialized, `init=1`, v2 heartbeat count rose from 20 to 385, and every
  reported `count` was 15554. `stops=1`, `resyncs=1`, `timeouts=0`, and
  `errors=0` stayed fixed across the captured steady interval. `converted`
  increased from 6 to 120; `dropped=2020` stopped rising. At 120 converted
  frames, conversion time was 35579 us average / 35614 us maximum. The
  captured output does not establish the visual quality or input behavior.
- Attachments: serial transcript supplied in chat

## 2026-09-24 -- HR-3 heartbeat-counter fix, hardware result
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_heartbeatfix_delay_whx.uf2`
- Results: after `MEMORY 2048B OK`, the display stayed black. Doom entered its
  frame loop. Once the NES was powered on, `init=1`, heartbeats rose from 10 to
  857, and the PIO read count held at exactly 128. Every heartbeat still stopped
  DMA (`stops=hb`), with `resyncs=0`; only two frames were converted. The fixed
  counter is now measuring the boot ROM's 128 mailbox reads but no qualified
  rendering fetches. An independent raw `/RD` counter is the next diagnostic.
- Attachments: serial transcript supplied in chat

## 2026-09-24 -- HR-3 post-boot bus diagnostic
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_postboot_diag_whx.uf2`
- Results: console showed `MEMORY 2048B OK`, then black. Doom entered
  `D_RunFrame` and returned from `TryRunTics`. Initially the bus showed
  `init=0 hb=0`, with one converted frame and its publication pending.
  Initialization and heartbeats eventually arrived; heartbeats climbed to 909,
  but every reported PPU read count was zero and every heartbeat stopped DMA.
  `converted` rose again on each NES power cycle or reset, independently of
  cartridge movement. Protocol errors appeared while the cartridge was moved.
- Diagnosis: `fcbus_device.c` sampled and reset the PIO read counter before
  every received byte. A v2 heartbeat is three bytes; its final byte therefore
  saw a freshly reset count of zero. The device now samples only the byte that
  completes a heartbeat. The initial lack of bus commands remains unexplained.
- Attachments: serial transcript and clarification supplied in chat

## 2026-09-24 -- HR-3 ROM-tail first console boot
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_romtail_delay_whx.uf2`
- Results: after `ST_Init` appeared on serial, the NES was powered on and the
  cartridge reseated until the display appeared. One complete `ROM ERACE` and
  `ROM UPDATE` occurred, followed by a reset and a black screen. A subsequent
  NES power cycle with the same firmware displayed `MEMORY 2048B OK`, a brief
  flash of horizontal lines, then black. No new reflash was observed. USB serial
  remained connected, with `ST_Init: Init status bar.` as its last line.
  Power-cycling only the NES did not restart the RP2350's 30-second countdown.
  The post-update stamp was not observed.
- Attachments: report supplied in chat

## 2026-09-24 -- HR-3 console reflash loop with double fix
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_doublefix_delay_whx.uf2`
- Results: console repeats `ROM UPDATE`, `ROM UPDATE END`, then
  `MEMORY 2048B OK` with local stamp `0DOOM-02-0001` and cartridge stamp
  `20DOOM-02-0001`. Each pass erases sectors from `ROM ERACE 8000` through
  `ROM ERACE E000` and `ROM ERACE END` before updating again. USB serial remains
  available but has no further output after `ST_Init: Init status bar.`
- Attachments: on-screen transcript supplied in chat

## 2026-09-24 -- HR-3 double-support fix, USB-only validation
- Console: NES powered off
- Cartridge firmware: `fcpico_doom_doublefix_delay_whx.uf2`
- Results: serial startup passed `ST_Init: Init status bar.` without another
  panic. The COM port remained available and the terminal stayed responsive.
- Attachments: serial transcript supplied in chat

## 2026-09-24 -- HR-3 delayed Doom startup
- Console: NES powered off
- Cartridge firmware: `fcpico_diagnostic_engine_delay_whx.uf2`
- Results: USB serial captured the Doom startup through `ST_Init: Init status bar.`;
  firmware then printed `*** PANIC ***` and `double support is disabled`.
  The terminal application crashed, but the COM port remained available.
  The linked target explicitly selected Pico SDK's `pico_double_none`, while
  `fcvideo_build_tables()` uses double precision on its first frame.
- Attachments: serial transcript supplied in chat

## 2026-09-24 -- HR-3 USB and bus isolation
- Console: NES powered off
- Cartridge firmware: `fcpico_diagnostic_usb_bus.uf2`
- Results: USB serial enumerates and prints `USB and bus initialized` with a
  rising uptime counter; the board LED blinks. This build pauses before Doom.
- Attachments: none

## 2026-09-24 -- HR-3 first Doom boot
- Console: NES-001 (per HR-3; region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_whx.uf2` (user-reported flash)
- Console ROM: tutorial stamp `2026-0205-1239` remains visible; Doom stamp
  `20DOOM-02-0001` appeared briefly
- Results: `MEMORY 2048B OK`, then `2026-0205-1239` and `20DOOM-02-0001`.
  The second stamp immediately changed to `DEFGHIJKLMNOPQRS`, followed by
  `PICO NOT FOUND`. No `ROM ERACE`, `ROM UPDATE`, or `ROM UPDATE END` appeared;
  Doom did not boot. With this firmware flashed, Windows Device Manager reports
  `Device Descriptor Request Failed` for USB even with the NES powered off, so no
  serial port or log is available. After flashing the recovery test-pattern UF2,
  USB serial is available again.
- Attachments: none yet

## 2026-09-22 -- I-10 replacement firmware validation
- Console: NES-001, NTSC
- Cartridge firmware: `fcpico_testpattern.uf2` from parent commit `73a6df2`, SHA-256
  `be283708ba21e4ffa75ad4dec665461cd52db9220888d86888096f8051389cbf`
- Console ROM: tutorial boot ROM
- Results: success; the test patterns appear after the replacement firmware added the
  post-`FP_COM_INI` data-mode handoff. This confirms the previous blank screen was the
  physical adapter's lost init event, not bad pattern, palette, or attribute data.
- Attachments: `tests/fixtures/hw_trace_ntsc/`; replacement short traces pending

## 2026-09-22 -- HR-1 initial count and display run
- Console: NES-001, NTSC
- Cartridge firmware: `fcpico_testpattern.uf2`, pre-init-handoff fix
- Console ROM: tutorial boot ROM; displayed `MEMORY 2048B OK`
- Results: 5495/5507 measured frames reported exactly 15490 reads; 12 low outliers caused
  DMA stops.  LED and 56.44 Hz heartbeats continued, but no pattern appeared because the
  physical test-pattern path did not react to `FP_COM_INI` by requesting data mode.  The
  palette and attribute buffers were correct.  Three raw traces were incomplete.
- Attachments: `tests/fixtures/hw_trace_ntsc/`
