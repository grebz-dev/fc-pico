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

## 2026-10-01 -- 9016 native episode and options candidate
- Console: no physical run yet.
- Cartridge firmware: unflashed candidate at
  `/tmp/fcpico-native-menus-9016/device-first/artifacts/fcpico_doom_native_status_whx.uf2`,
  SHA-256 `44228ccb98282e2f35decfea7123cdd35e8e7044910cea258f5b4037d73e6667`.
  Embedded ROM stamp `20DOOM-04-9016`.
- Finding: the native options overlay had a MOUSE row absent from the
  `NO_USE_MOUSE=1` compiled engine menu; selecting its index activated Sound.
  Mesen now shows End Game, Message, Sound at their matching selection slots.
  Episode names render in full-width, centered, single-line background font.
- Validation: [episode/options captures](assets/menu_native_9016/README.md),
  34 Python tests, two host C tests, five Mesen scenes, 30 stable heartbeats
  at count 15122, and byte-identical ROM/UF2 builds in two independent trees.
  Physical NES result remains pending.

## 2026-10-01 -- HUD menu-close regression, Mesen 9015 candidate
- Console: no new physical run; the user's latest issue report did not identify
  the display source.
- Cartridge firmware: unflashed 9015 candidate,
  `/tmp/fcpico-native-hud-repro-9015/device-first/artifacts/fcpico_doom_native_status_whx.uf2`,
  SHA-256 `ee303541dbbbc89d1b644e3105a86f027035bdb3b8b74d027249ae1b9cb248ba`.
  Embedded ROM stamp `20DOOM-04-9015`.
- Finding: Mesen reproduced alternating face-row offsets and a missing selected
  yellow ARMS plate after the paused menu closed. Logo OAM X/tile/attribute
  bytes survived the transition. Restoring the gameplay OAM template fixes
  both defects. The paused logo is now 64×32 and blue/blue/yellow.
- Validation: Mesen before/open/after captures and OAM dumps in
  [`assets/hud_live_plate/menu_close_trace/`](assets/hud_live_plate/menu_close_trace/README.md).
  Thirty stable heartbeats at count 15122, no additional DMA stop; 30 Python
  tests and two host C tests pass. Two independent builds produced identical
  ROM and merged UF2 bytes. Physical NES output for 9015 is pending.

## 2026-09-28 -- HR-14 paused UI and face findings
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: HR-14 `20DOOM-04-9011` candidate from the prior request;
  console stamp was not separately transcribed.
- Results: outside menus, a small yellow corner of the paused DOOM logo
  remains visible. Doomguy's upper-right and middle-right face sprites are
  missing. The difficulty menu displays abbreviated `EASY`, `NORMAL`, `HARD`,
  `ULTRA`, `NIGHT` instead of Doom's original difficulty patches. Main labels
  are too small and read `READ` rather than `READ ME`. The paused logo could
  be larger and needs more visible color separation. No serial excerpt or
  additional photo was supplied. The follow-up is being validated in Mesen
  without a new UF2 while the visuals are iterated.
- Attachments: observations supplied in chat.

## 2026-09-28 -- HR-13 styled sprites, paused-menu finding
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: HR-13 `20DOOM-04-9010` candidate from the prior request;
  console stamp was not separately transcribed.
- Results: the physical photo shows the native red menu text and styled gray
  status panel, but the top status values cross the panel's upper border and
  the Doomguy face crosses the lower divider. The source-derived DOOM sprite
  logo is absent on the paused game menu. This is consistent with the 9010
  implementation, which only places that logo on the title menu; the photo
  does not establish whether it appears on the title. Alignment and a compact
  paused-menu logo are addressed in the 9011 follow-up candidate. Episode
  labels and cold-boot CHR residency were not reported in this photo.
- Attachments: [`STYLED_SPRITES_HW_TEST.jpg`](STYLED_SPRITES_HW_TEST.jpg),
  observations supplied in chat.

## 2026-09-28 -- HR-12 transparent menu, gray status and special faces
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: `fcpico_doom_native_status_whx.uf2`, SHA-256
  `9b78b727cb30c381776d1332ac598deb7bec51ee9372b81f1d99172002f00520`.
  Embedded ROM stamp: `20DOOM-04-9008` (not transcribed from the console).
- Results: physical run successful. Native text is red and mostly more legible
  than before; the status backing is gray. The ouch and pickup grin faces both
  work. The native episode menu uses generic `Episode 1`, `Episode 2`, and
  `Episode 3` labels instead of the game's episode names. No new serial log or
  photos were supplied. The transparent menu backdrop was part of this
  candidate, but the report did not separately describe its appearance.
- Attachments: observations supplied in chat.

## 2026-09-27 -- HR-11 menu persistence and idle face
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: `fcpico_doom_native_status_whx.uf2`, SHA-256
  `f0204d307efd0da6ff3346488df4545be581ddfe6d6cd59253c551d4b32f1a7a`.
  Embedded ROM stamp: `20DOOM-04-9007` (not transcribed from the console).
- Results: the native status stays active with the menu open, and the healthy
  face changes while idle. The menu still has a black backing and white
  native text; the panel covers the original converted labels.
  The face lacks distinct short hit and pickup expressions. These are the
  HR-12 changes; no new serial excerpt or photos were supplied for HR-11.
- Attachments: observations supplied in chat.

## 2026-09-27 -- HR-10 native status and face, pause finding
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: `fcpico_doom_native_status_whx.uf2`, SHA-256
  `1f6130e97ad5e00fa8c7ea3cec7833db5c8e4202c0618cd059fc75f1cbbcb8f4`.
  Embedded ROM stamp: `20DOOM-04-9006` (not transcribed from the console).
- Results: native sprite status text and the real Doomguy face look good during
  demo and playback. Pressing Start hides the native sprites and reveals the
  original converted status bar. The face changes across damage levels but
  does not show the idle straight-face variants. These observations led to
  the HR-11 menu and idle-face candidate.
- Serial: `proto=4`, `count=15122`, `raw=41114`, `init=1`, `ready=1` and
  `pending=1` remain stable through `hb=4135`; stops/resyncs stay at 1 and
  timeouts/errors stay at 0. `converted` rises 27 to 976. Drops start at
  1168, rise to 1177 around frame 600, then stay flat through frame 960.
  Conversion average/max reaches 39148/39202 us. The initial drop count is
  startup history, not evidence of continuing bus errors.
- Attachments: serial excerpt and gameplay observations supplied in chat.

## 2026-09-27 -- HR-9 v4 text and live health on NES-001
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: `fcpico_doom_bg_text_probe_whx.uf2`, SHA-256
  `86f68ed099a860767c5d6a9bef2a15a10154d9813188947190f62fe6511f9fa1`.
  Embedded ROM stamp: `20DOOM-04-9001` (not transcribed from the console).
- Results: after the startup countdown, Doom shows `NATIVE BACKGROUND TEXT
  WORKS` in the bottom row and a live `H100` health display over the converted
  background. The former replaces a chunk of the legacy status. Neither A nor
  diamond is present, as this v4 ROM contains no sprite diagnostic art.
- Serial: early `proto=2` heartbeats have count 15554 while the previous ROM
  runs; later `proto=4` heartbeats from 7037 through 9773 hold count 15122 and
  raw 41114. `stops=1`, `resyncs=1`, `timeouts=0`, and `errors=0` remain fixed.
  `converted` rises 1352 to 1876; `dropped=102` remains fixed in the v4 excerpt.
  Reported average/max conversion reaches 39141/39375 us by frame 1860.
  The early proto=2 section is not a failed v4 negotiation; the text appears
  only when the new Doom ROM has booted after its countdown.
- Attachments: serial excerpt and on-screen observations supplied in chat.

## 2026-09-27 -- HR-8 duplicate sprite diagnostic, good boot
- Console: NES-001 per current session; region not reconfirmed.
- Cartridge firmware: `fcpico_doom_sprite_diagnostic_whx.uf2`, SHA-256
  `d23459faad3c7c83771473a12d0f9a22870d175a66e97fe33cdae789669442d4`.
  Embedded ROM stamp: `20DOOM-02-9002` (not transcribed from the console).
- Results: before Doom and during Doom, all six A/diamond symbols are visible:
  the original top pair, same-tile second-row pair, and copied-art second-row
  pair. The two adjacent rectangles and checker also remain visible. These
  photos document a good boot; they do not reproduce the intermittent missing
  top pair and therefore cannot distinguish OAM ordering from CHR residency.
- Attachments: [`SPRITES_BEFORE_DOOM_2_LINES.jpg`](SPRITES_BEFORE_DOOM_2_LINES.jpg),
  [`SPRITES_DURING_DOOM_2_LINES.jpg`](SPRITES_DURING_DOOM_2_LINES.jpg) and the
  user's six-symbol transcription supplied in chat.

## 2026-09-27 -- HR-7 sprite probe on NES-001
- Console: NES-001 per HR-7 session; region not reconfirmed in this report.
- Cartridge firmware: `fcpico_doom_sprite_probe_whx.uf2`, SHA-256
  `a33f2ef0780d2fa49b487225636a7989e891e3e5a791892aba0f89c0b5b3e022`.
  Embedded ROM stamp: `20DOOM-02-9001` (not transcribed from the console).
- Results: the reflash completed; controls and Doom video continued to work. The
  white `A`, diamond, 16×16 outline and checker tile are visible before Doom
  loads and over the first Doom scene. After two NES power cycles, the user
  reports that the `A` and diamond are missing while the two side by side
  rectangle placeholder and checker remain visible. The `A` and diamond return
  after another power cycle, so the failure is intermittent. The rectangles
  test a 16×16 four-tile shape; they are not final Doomguy face art. The
  previous Doom UF2 has not yet been reflashed, so reverse recovery has not
  been exercised. This intermittent result leaves the resident atlas gate open.
- Serial: from `hb=10173` through `hb=12616`, every reported `count` is 15554;
  `stops=1`, `resyncs=1`, `timeouts=0`, `errors=0`, `ready=1`, `proto=2` and
  `dropped=1207` remain fixed. `converted` increases from 2452 to 2932.
  `conversion_us` averages 38219 and peaks at 38307 in this excerpt.
  Interactive `stats` was unavailable while Doom ran; these heartbeat lines
  provide the same relevant counters. This establishes visible storage for
  the particular `$1800..$185F` and `$1FF0..$1FFF` probe tiles, not every
  possible atlas address or an alias map.
- Attachments: [`SPRITES_BEFORE_DOOM.jpg`](SPRITES_BEFORE_DOOM.jpg),
  [`SPRITES.jpg`](SPRITES.jpg), [`SPRITES_DETAIL.jpg`](SPRITES_DETAIL.jpg),
  [`HW_PHOTO_2_POWER_CYCLES.jpg`](HW_PHOTO_2_POWER_CYCLES.jpg) and heartbeat
  excerpt and corrected post-cycle observations supplied in chat.

## 2026-09-25 -- HR-6 shadow-detail and B-use milestone
- Console: existing NES setup; model/region not reconfirmed in this report.
- Cartridge firmware: `fcpico_doom_shadow_detail_delay_whx.uf2`, SHA-256
  `3d298001d93f028dd6a3e9f37287e8d77762704b0c1295485c8fd22275f88dbc`.
- User result: “Flashed, works great! Contrast is much better and B button works.”
  USB serial works. This validates playback, the contrast improvement and the
  B-button fix with `FCPICO_DIAGNOSTIC_ENGINE_DELAY=ON` restored.
- Serial: before console initialization, `init=0 hb=0 converted=1 pending=1`
  and drops rise to 657 while the first published frame waits. At `hb=115`,
  `init=1 count=15554 raw=41098 stops=1 resyncs=1`. These counts remain stable
  through `hb=1829`; timeouts and protocol errors stay zero. Drops settle at
  691 by `hb=359` and do not increase in the remaining capture.
- Measurements: at 60 converted frames, average/max conversion is 38143/38183 us;
  at 480 frames it is 38208/38251 us. This is approximately 2.63 ms slower than
  the earlier 200-line firmware's 35.579 ms average. Both geometry and palette
  changed, so this comparison does not isolate a cause. The <=8 ms target remains
  unmet; conversion time is not an end-to-end gameplay FPS measurement.
- Build finding: restoring the countdown/bus diagnostic profile resolves the
  reported boot/USB failure on hardware. Which part of that profile is necessary
  has not been isolated; preserve it in rebuilds. Native and bundled Windows
  picotool emit identical firmware UF2s for the same ELF.
- Remaining: detailed geometry, individual B chord/menu cases, photographs,
  scene-specific color/HUD review and the longer stability checklist.
- Attachments: serial excerpt and user confirmation supplied in chat.

## 2026-09-25 -- HR-6 rebuild boot and USB failures
- Console: existing NES setup; model/region not reconfirmed in this report.
- First rebuild: `fcpico_doom_shadow_detail_whx.uf2`, SHA-256
  `df1ca0f02dd20641fa372a6b42e6071c6d30dabe3325fa36167e831d1f703b3f`.
  User reports BOOTSEL on USB connection and `PICO NOT FOUND` on the NES.
- Second rebuild: same filename, SHA-256
  `c94f3aafad23bdc60a6d11e21bebef935d1fb99f0d563230654d99b42822c852`.
  Firmware UF2 was emitted by native picotool 2.1.1. User reports USB device
  descriptor failures and, after about 30 seconds, `MEMORY 2048B OK
  20DOOM-02-002 DEFGHIJKLMNOPQRS PICO NOT FOUND` (stamp transcribed as reported).
- Build comparison: both rebuilds omitted `FCPICO_DIAGNOSTIC_ENGINE_DELAY=ON`,
  retained by the previously working hardware candidates. It enables the
  30-second startup countdown, bus statistics and raw PIO read counter.
  Double support remains linked. Restoring this option produces 287836 firmware
  bytes and an 8158-block merged image. Runtime causation is not yet established.
- Tool comparison: the bundled `tutorial_project/bin/picotool.exe` runs under
  WSL with Windows interop and reports v2.3.0. Its UF2 conversion of the restored
  build's ELF is byte-identical to native picotool 2.1.1 output. Tool selection
  therefore does not explain a difference for this ELF.
- Next candidate: `/tmp/fcpico_doom_shadow_detail_delay_whx.uf2`, SHA-256
  `3d298001d93f028dd6a3e9f37287e8d77762704b0c1295485c8fd22275f88dbc`.
  Flash layout, picotool metadata, embedded ROM and WHX comparisons passed.
  Pending: with NES off, capture USB serial during the countdown and through
  `entering D_DoomMain`; then power the NES on after startup. This separates
  early USB/bus initialization failure from failure during engine execution.
- Attachments: user reports supplied in chat.

## 2026-09-24 -- HR-4 controller input and screen photographs
- Console: NES-001 (region not reconfirmed in this report)
- Cartridge firmware: `fcpico_doom_input_whx.uf2` ([HR-4](HARDWARE-REQUESTS.md#hr-4-controller-input-on-doom-task-p1-t4) candidate)
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
- Cartridge firmware: `fcpico_doom_streamfix_delay_bootsel_whx.uf2` (current [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6) candidate)
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
- Console: NES-001 (per [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6); region not reconfirmed in this report)
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
