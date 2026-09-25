# Progress log

One entry per task from [`plan/10-workplan.md`](plan/10-workplan.md), newest first. Format:

```
## <task id> -- <title>
- Commits: <range or list>
- Verified: <command> -> <result summary>
- Measurements: <fps / cycles / bytes / ms, with the command>
- Left out: <what and why>
- Plan changes: <documents touched>
```

## HR-4 -- physical input result and B-use diagnosis (2026-09-24)
- NES-001 input works for movement, strafing and menus. A B tap does not
  activate use. A host probe replayed a B tap through `--pads`: pad frames
  included press and release, the adapter emitted matching space keydown/up,
  but `G_BuildTiccmd()` never set `BT_USE`. Both events arrive before it reads
  the final key state. The probe was removed after diagnosis; firmware fixes
  await the requested advice on the display layout.
- The supplied photograph's lower gap matches the planned 200-line Doom image
  at NES lines 16..215, leaving 24 blank lines beneath. The 256x240 output
  stream has room to scale the image vertically; see [`HARDWARE-LOG.md`](HARDWARE-LOG.md).
- Serial remained stable through `hb=2221` at `count=15554` with no new DMA
  stops or resyncs. Conversion average/max at 540 frames was 35574/35612 us.

## P1-T4 -- NES pad input wired to Doom (2026-09-24)
- The RP2350 bus now retains up to 31 consecutive pad-1 snapshots from v2
  heartbeats. `I_StartTic()` drains them into the existing mapper and posts
  Doom key events; short presses between tics survive. The host runner accepts
  one numeric pad byte per tic via `--pads`, and `--warp` starts a level for
  reproducible movement checks. Start opens the menu and A/B map to menu
  accept/back while it is active.
- Verified: GCC 13.2 RP2350 `doom_tiny_fcpico` build and flash layout check
  passed (287724 firmware bytes, 236564 free before WHX). Host CTest 10/10;
  engine host CTest 4/4, including an E1M1 scripted Up run whose player
  position differs from an idle run. The merged input UF2 is
  `/tmp/fcpico-hw-doom/fcpico_doom_input_whx.uf2` (8157 blocks), SHA-256
  `82405a0823193a0d7ea7c6b8f5c053450e7d049399fa23d1d7f21973caf5fbd8`.
- Left out: physical pad check ([HR-4](HARDWARE-REQUESTS.md#hr-4-controller-input-on-doom-task-p1-t4)), pad 2, and context-specific automap
  controls. The measured 35.6 ms conversion time still exceeds the plan's
  performance target and needs a separate optimization pass.

## HR-3 -- Doom visible on NES-001 (2026-09-24)
- Hardware result: the user reports Doom playing back on screen with the
  stream-fix/BOOTSEL image. Serial showed stable v2 count 15554 from heartbeat
  20 through 385, converted frames rising from 6 to 120, and no additional
  DMA stops or resyncs after the initial one of each. Timeouts and protocol
  errors remained zero. The 2020 dropped frames accrued around startup and
  did not grow during the captured steady interval.
- Measurements: `conversion_us=35579/35614` average/max at 120 converted
  frames. Image quality, longer stability, and physical controller input have
  not yet been reported; details are in [`HARDWARE-LOG.md`](HARDWARE-LOG.md).

## HR-3 -- USB serial BOOTSEL command in Doom firmware (2026-09-24)
- The device main core now accepts `bootsel` followed by Enter on USB serial and
  calls the RP2350 ROM USB-boot reset. It polls during the 30-second diagnostic
  delay and at the end of each rendered frame, so later UF2 updates can start
  with the cartridge installed.
- Verified: `cmake --build /tmp/fcpico-review/device --target doom_tiny_fcpico -j 4`
  passed with GCC 13.2; `flash_layout_check.py` passed with 286516 firmware bytes
  and 237772 bytes free before the WHX address. `whx2uf2.py` produced the merged
  candidate at `/tmp/fcpico-hw-doom/fcpico_doom_streamfix_delay_bootsel_whx.uf2`
  (8153 blocks), SHA-256
  `1221d341ac281b28cdd0712410aefa7b91b3fab99e7f0a15c76ad8e07e593127`.
- Left out: physical USB command validation; [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6) hardware validation is pending.

## HR-3 / I-14 / I-15 -- pre-hardware review: stream selection and PPU address fixes (2026-09-24)
- Findings: the Doom setup filled nametable 0 with tile `$00`, unlike the working
  tutorial's `$80`. This targets `$0000` instead of the `$0800` stream port. The
  previous Mesen mapper selected all `$0000-$0FFF` reads and discarded fetches at
  specific cycles to force the measured count, hiding the setup error. Selecting
  `(addr & $F800) == $0800`, with no cycle filtering, reproduces the existing
  hardware failure exactly: black screen, 129 physical mailbox reads / count=128,
  zero selected rendering reads, and one DMA stop per heartbeat. The same decode
  naturally reproduces the tutorial's measured 66/64 cadence and existing S0 golden.
- Second finding: correcting tiles alone yields count=15550 (62 pre-render reads),
  still outside the v2 sync window. The three-byte heartbeat leaves the PPU's current
  address at `$0803`; `$2005` updates temporary scroll state, so it does not undo this.
  Restoring `$2006` to `$0801` after the packet restores the tutorial's pre-render phase,
  producing count=15554 without changing any protocol/count constant.
- Changes: setup uses tile `$80` in nametable 0, clears attributes and adjacent nametable
  1 to zero, and the NMI restores `$0801` before control/scroll registers. ROM stamp is
  `20DOOM-02-0002` so the fixed bank installs the change. Mesen now decodes addresses;
  wider masks require diagnostic mode. The Doom runner checks every completed steady
  heartbeat for the exact count and no new DMA stops, including mailbox-only failures.
- Regression loop: `doom/.venv/bin/python -m pytest doom/tests/bootrom/test_doom_setup.py -q`
  failed on the old assembled ROM (`background fetches ... [0]`), then passed after the
  fix. The heartbeat-order test also failed before the address restore. With the rebuilt
  mapper, `run_doom_frame.py ... --rom /tmp/fcpico-review/before/doom.nes` now fails with
  `count=128, expected=15554, new DMA stops=1`; the intermediate tile-only run recorded
  count=15550. Captures/logs are under `/tmp/fcpico-review/`.
- Verified: pinned NESASM CE tutorial MD5 gate passed; unchanged fix-bank check passed.
  `cmake --build build-host && ctest --test-dir build-host --output-on-failure` -> 10/10.
  `doom/.venv/bin/python -m pytest doom/tests doom/sim/pioemu -q` -> 386 passed, 1 skipped.
  New PIO tests confirm the raw diagnostic counts both selected and unselected `/RD`
  pulses once, while the existing counter counts only selected pulses; both report N-1.
  The earlier heartbeat sampling and ROM-tail changes remain covered by host tests.
- Co-simulation: `DOTNET_ROOT=/home/josh/.dotnet doom/sim/mesen2/run_scenario.sh S0
  --frames 180 --output /tmp/fcpico-review/S0` -> strict pass, count=15490, 15426 selected
  rendering reads, unchanged screenshot golden. Real host DEMO1 frame 100 through D0/D1
  -> correlation 0.9942, exact palette/attributes/mailbox. S1 tutorial -> `0002`, broken
  Doom `0001` -> `0002`, and recovery `0002` -> tutorial each passed 3000-frame runs with
  byte-exact final PRG and 30 stable final heartbeats. v2 count=15554, v1 count=15490;
  no steady DMA stops. v2 NMI exits at scanline 255; py65 worst case 1614 critical /
  2021 total cycles, below 1900 / 2200 limits.
- Device: GCC 13.2 diagnostic and recovery builds passed; 286108 firmware flash bytes,
  238180 bytes free before WHX, 36052 post-zone RAM bytes (minimum 24576). Merged UF2:
  `/tmp/fcpico-hw-doom/fcpico_doom_streamfix_delay_whx.uf2`, 8151 blocks, SHA-256
  `17f60cda5ea6905a9885bb39796156a66da7721c4f62906cd597f329b12127c7`.
  Reconstructed UF2 payload contains the exact assembled `0002` ROM and byte-exact WHX.
  It retains the 30-second delay, raw counter, heartbeat fix, ROM tail and double support.
- Limits: the address decode is supported by tutorial code, measured cadence and exact
  reproduction of the hardware symptom; this is not a new electrical measurement or a
  hardware validation of the fixed ROM. Physical boot/display and dynamic frame sequences
  remain unverified. The old raw-count-only request is superseded in [`HARDWARE-REQUESTS.md`](HARDWARE-REQUESTS.md).

## P2-T4 / P1-T6 -- S1 reflash co-simulation, stamp fix, first Doom hardware UF2 (2026-09-23)
- What landed: the Mesen FC PICO mapper now emulates the cartridge PRG flash (JEDEC
  unlock on A10-A0, 4 KiB sector erase, bit-clearing program, a short DQ7/DQ6/DQ3
  status phase that the fix bank polls). The cart model keeps its own copy of the
  served ROM (`FCPICO_SERVE_ROM` may name a different one) and, like the device,
  defaults to protocol v2 when serving the Doom ROM. `run_doom_frame.py --serve-rom
  --frames` runs S1 and byte-compares the console's final PRG with the served image.
  `tools/whx2uf2.py` merges `doom1.whx` at `0x10080000` into the firmware UF2.
- Bug found: the fix bank's `CHK_ROMVER` (`$F2B2` in `bootrom_fixr.bin`) reflashes after
  five mismatches only if the cartridge stamp starts with `"20"`; otherwise it prints
  PICO NOT FOUND and halts. The previous `DOOM-02-000001` stamp would never have
  reflashed a tutorial console; S1 reproduces that (PRG unchanged, 4,703 bytes differ).
  The stamp is now `20DOOM-02-0001`, a bootrom test pins the prefix, and plan 03's
  claim that the check does not exist is corrected.
- Verified: S1 `run_doom_frame.py frame000100.bin --rom tutorial rom.NES --serve-rom
  doom.nes --frames 3000` -> console PRG equals `doom.nes`; correlation 0.9942;
  16/16 palette, 64/64 attributes and 128-byte mailbox equal; 30 NMI exits on
  scanline 255. Reverse recovery (Doom console, tutorial served) -> PRG equals
  `rom.NES`. D0, D1 and strict S0 still pass. Host CTest 10/10; pytest 373 passed,
  1 skipped; bootrom 24 passed; protocol and link checks clean. GCC 13.2 device ELF
  uses 282,488 flash bytes (241,800 free before WHX). Merged UF2
  `/tmp/fcpico-hw-doom/fcpico_doom_whx.uf2` (8,137 blocks) is [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6).
- Left out: timing of the real flash chip (the model completes erase/program
  immediately after a short status phase); hardware boot, display and conversion
  timing are [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6). Dynamic multi-frame co-simulation, input and sound remain open.

## I-13 / I-14 / I-15 -- v2 console ROM and fixed-frame D1 gate (2026-09-23)
- What landed: a display-first 32 KB Doom v2 NES ROM with the unchanged fix
  bank, 128-byte mailbox NMI, per-frame palette and attribute writes, explicit
  controller heartbeat, and 15-pair APU replay. Native NESASM CE at commit
  `6fc41cda` builds it; normalization of two NES 2.0 header bytes makes a
  tutorial reassembly reproduce MD5 `b6cd675342b6c8ad79e537e2c9860579`.
  The device build now embeds the v2 ROM, and a dedicated boot-ROM workflow
  assembles it twice, checks the fix bank, and runs the py65 tests.
- Verified: worst-case py65 NMI uses 1602 critical / 2009 total cycles
  (limits 1900 / 2200), 129 `$2007` reads, and all 15 APU writes. D1 Mesen
  frame 100 has 0.9942 visible correlation; 16/16 palette, 64/64 attributes,
  and the normalized 128-byte mailbox agree. 30 post-startup NMI exits occur
  on scanline 255. New RP2350 ELF/UF2 links with the v2 ROM, uses 282,488
  firmware bytes (241,800 before WHX), and leaves 36,368 post-zone RAM bytes.
- Left out: S1 erase/reflash cannot be measured with the current Mesen mapper,
  which does not emulate PRG flash writes. D1 replays a fixed frame; dynamic
  frame sequences, pad 2, v1 bulk data mode in the new ROM, hardware boot,
  conversion timing and sound production remain open. Do not flash this UF2
  without an explicit hardware reflash procedure and recovery plan.

## I-17 / P1-T3 -- Doom picture in Mesen, device publication candidate (2026-09-23)
- What landed: D0 replays an actual DEMO1 frame through the tutorial-ROM Mesen PPU.
  The screenshot agrees with the independent PPU reconstruction, and console
  palette/attribute RAM equals the stream mailbox byte-for-byte. A v1 32-byte
  bulk palette upload now mirrors its second half so the NES palette aliases do
  not overwrite the backdrop; Mesen's attribute bulk response accounts for the
  tutorial ROM's extra dummy read. The RP2350 adapter stages scanlines directly
  into `fcvideo`, publishes converted frames to `fcbus`, and embeds the tutorial
  ROM. The UF2 is a candidate, not yet hardware-approved.
- Verified: D0 frame 100 visible correlation 0.9942 and 16/16 palette plus
  64/64 attribute bytes equal; screenshot under `/tmp/fcpico-d0-final/run/`.
  Strict S0 is green with a newly reviewed black-backdrop golden, 30/30 valid
  post-startup mailboxes, count 15490 and no post-startup DMA stops. The staged
  converter matches the bulk API's stream exactly. Host 128 KB zone replay:
  3/3 CTests including two 600-frame runs. RP2350 ELF/UF2 links; flash uses
  282,500 bytes, leaving 241,788 before WHX; static RAM leaves 36,368 bytes
  after the configured 128 KB Doom zone (24 KB minimum gate).
- Left out: hardware boot/display and measured conversion timing; `doom1.whx`
  is not in the UF2 and still needs loading at `0x10080000`. D0 replays a fixed
  stream through tutorial v1, not the Doom v2 ROM. S1 reflash, full S2 sequence,
  palette/attribute updates per dynamic frame and v2 NMI timing remain open.

## I-12 / I-17 -- host Doom-to-NES stream output (2026-09-23)
- What landed: the SDL-free host runner's `--dump-stream DIR` converts each composed
  320x200 indexed frame to a 17,408-byte v2 cartridge stream using the default
  shared-white preset, the actual WHX PLAYPAL, attribute hysteresis and the current
  engine palette-flash set. Dumps are named `frame000000.bin`, etc.
- Verified: two 600-frame DEMO1 runs produce identical indexed frames and identical
  streams (CTest `engine_host_determinism`). The independent Python oracle matches
  C output byte-for-byte on real frames 0, 100 and 101, preserving the previous
  attribute table across all intervening frames. A v2-mailbox NES PPU-model decode
  of frame 100 shows the game view and status bar at 256x240; preview:
  `/tmp/fcpico-doom-nes-frame100.png`.
- Left out: device bus publication, measured conversion time, controller pad input,
  and Mesen S2. The current Mesen runner supports tutorial-ROM S0 only, so the
  preview is not yet an emulated-console acceptance result.

## I-16 / P1-T1 -- indexed full-frame composition (2026-09-23)
- What landed: the FC PICO adapter now composes all 320x200 scanlines from the renderer's
  view buffers and packed overlay list, including title/menu/HUD/status patches and the
  melt-wipe state. It sends lines through `fcvideo_line_sink`; the host sink writes full
  raw frames and PLAYPAL-indexed PNG previews every 100th frame. The device build also
  executes composition and launches the renderer's core-1 worker, but its sink remains a
  no-op until stream conversion/publication is integrated.
- Verified: SDL-free host CTest 3/3, including two identical 600-frame captures, 60 golden
  hashes, six PNG previews, and separate title/menu goldens. Visual samples show the title,
  menu, E1M1 status bar and wipe. Python suite: 365 passed, 1 skipped. GCC 13.2 RP2350
  engine ELF links at 242,612 flash bytes, leaving 281,676 before the WHX partition.
  A 600-frame AddressSanitizer replay passes after correcting the host runner's one-byte
  `singletics` type and the renderer's visplane-row lookahead. UBSan still reports 133
  warnings in existing engine arithmetic/indexing paths; none are in the new compositor.
- Left out: independent Chocolate Doom pixel comparison, hardware display, and NES stream
  output. Preview captures are under `/tmp/fcpico-fullframe-b/`, title under
  `/tmp/fcpico-title-frames-current/`, and menu under `/tmp/fcpico-menu-frames/`.

## I-12 / P0-T3 -- SDL-free host engine runner (2026-09-23)
- What landed: `PICO_PLATFORM=host` now builds the whole FC PICO engine and pthread shim
  without SDL. The runner reads the checked-in WHX by path, starts DEMO1, captures indexed
  320x168 view frames and stops at `--frames`. The host render barrier and wipe progression
  are lockstep; no scanline thread is required for this intermediate view capture.
- Verified: default host `cmake --build /tmp/fcpico-engine-host-12 -j 4` passes;
  `ctest --test-dir /tmp/fcpico-engine-host-12 --output-on-failure` -> 2/2, including
  two byte-identical, changing 600-frame DEMO1 captures. GCC 13.2 RP2350 engine ELF and
  test-pattern targets still link. Raw output from manual runs is under
  `/tmp/fcpico-host-frames-a/` and `-b/` (600 files each, 53,760 bytes per view).
- Left out: the raw view omits status/menu/wipe composition. `--dump-stream` and `--pads`
  report unsupported until [I-16](issues/I-16-stage-a-composition.md)/[I-17](issues/I-17-fcvideo-impl.md) and the input adapter land; [I-12](issues/I-12-engine-host-build.md) acceptance remains
  partial. This is not yet a co-simulation-ready Doom video stream.

## I-11 / P0-T2 -- RP2350 engine platform skeleton (2026-09-23)
- Commits: engine submodule `0c27c1ce` and this parent integration commit.
- What landed: optional parent superbuild integration, isolated `common_fcpico` platform
  sources, a silent/no-display engine adapter, a USB serial bring-up marker, and a
  `doom_tiny_fcpico` target producing `fcpico_doom.elf`. The FC PICO build omits VGA/I2S
  libraries; shared engine changes only guard behavior that assumed Pico networking,
  ENDOOM or the VGA audio module. The device bus target now compiles its Pico SDK sources
  once per firmware, which also preserves the test-pattern build.
- Verified: GCC 13.2.Rel1 `cmake --build /tmp/fcpico-engine-device-13 --target
  doom_tiny_fcpico -j 4` links; `flash_layout_check.py` passes with 237,896 bytes used
  and 286,392 free before WHX. The existing RP2350 test-pattern target rebuilds, all
  10 host C suites pass, and standalone Pico host engine configuration succeeds.
- Left out: on-console boot to `D_DoomMain` is unverified. The adapters do not yet
  initialise the cartridge bus, compose 8-bit frames, read controllers or play sound.
  Native `chocolate-doom` configuration needs SDL2 development packages not installed here.

## I-19 / P0-T9 / P0-T10 / P0-T11 -- hardware-calibrated PPU path (2026-09-22)
- Commits: Mesen submodule calibration commit and this commit.
- Hardware evidence: complete NES-001 trace 6 decodes to 66 selected reads on pre-render
  and 64 on each of 14 complete visible lines. `/RD` low widths are 160-200 ns for 2,705
  of 2,706 pulses. Traces 4 and 5 are incomplete by 28 and 21 packed words and are retained
  only as diagnostics.
- Explanation: `66 + 240*64 + 65 NMI reads = 15491` physical reads; `fcppu_rna` captures
  `!X` before decrementing X and therefore reports the zero-based index 15490. The original
  `convVram()` source index is flat, so its 34-iteration batches are not 34-word physical
  line strides; hardware advances 32 words per visible line. A normal re-arm has no extra
  four-zero-byte stream prefix.
- What landed: trace-aware decoder segmentation and fixture regression, calibrated L3 model,
  linear 32-word C/Python/test-pattern stream conversion, zero-based host counter emulation,
  hardware-timed Mesen selection, reviewed S0 ARGB golden, and the missing Mesen static-link
  dependency on the shared test-pattern controller.
- Verified: `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` -> 365 passed,
  1 skipped; host C 10/10; generated protocol check and 51-document link check pass.
  `run_scenario.sh S0 --frames 180` passes strict S0 twice with
  `counts_after_startup=[15490]`, 30/30 valid mailboxes, zero post-startup DMA stops,
  15,426 selected rendering reads and identical debugger-peek traces/screenshots. The
  RP2350 test-pattern UF2 builds and passes the flash-layout check (68,564 bytes used,
  455,724 free); SHA-256 `a1f129761e70c0c3f6aa65b02e7e69cfe9e93cd244da08ddcb1bc66d25d7b2e9`.
- Left out: the corrected stream geometry needs the replacement hardware UF2 visual check;
  engine frame composition/publication remains [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md)/[I-16](issues/I-16-stage-a-composition.md)/[I-17](issues/I-17-fcvideo-impl.md).

## I-10 / I-19 -- first NTSC measurement and test-pattern init repair (2026-09-22)
- Commits: this commit.
- What landed: the first NES-001 run validates the existing v1 hardware count at 15490
  (5495/5507 exact frames; 12 low outliers). The blank screen exposed a separate adapter
  divergence: Mesen reacted to `FP_COM_INI` with `PF_COM_DMOD`, while the physical main loop
  could not observe the init after the IRQ backend drained it. A shared backend-neutral
  test-pattern controller now owns pattern/table publication and init-time data mode for
  both adapters. Init count/stage are retained in stats and printed by the CLI.
- Capture changes: the initial three 65,536-word traces were incomplete over USB/terminal
  capture. The sampler now emits 8,192 words, about 2.0 ms or 31 NTSC scanlines, retaining
  the required within-line prefetch evidence while reducing text output below 256 KiB.
- Verified: focused red-to-green checks produced `1332` fcbus checks, `7` test-pattern
  checks and `149` cartmodel checks; all 10 host C suites pass. The RP2350 UF2 builds and
  `flash_layout_check.py` reports 68,564 bytes used with 455,724 bytes free.
- Hardware follow-up: the replacement UF2 displays its test patterns successfully on the
  same NES-001, confirming the init-event diagnosis and completing [I-10](issues/I-10-testpattern-firmware.md). Three decoder-valid
  traces remain [I-19](issues/I-19-hw-trace.md)'s last hardware input. Strict Mesen S0 remains intentionally
  uncalibrated until those traces determine which strobes the PIO counter omits.
- Plan changes: [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9), [I-10](issues/I-10-testpattern-firmware.md), the hardware log, and the new hardware fixture directory record
  the session and exact artifact.

## I-17 -- NES presets, PLAYPAL tables and flash sets (2026-09-21)
- Commits: `21aa70e`, this commit.
- What landed: build-selectable default and named A/B/C sub-palette presets, including the
  PiPU set, plus allocation-free C generation of the 1 KB error table, 16 KB dither LUT,
  and 16-byte NES palette from PLAYPAL palette 0. The RGB palette is the same NESDev example
  table used by the Python reference. It is called once at setup, not in the frame/ISR path.
  All 14 mailbox palette sets match the engine's integer red/yellow/green tint arithmetic;
  set 0 is the exact preset, and switching sets does not rebuild `err`/`lut`.
- Verified: `ctest --test-dir /tmp/fcpico-video-host --output-on-failure` -> 9/9 passed;
  `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/fcpico-video-asan --output-on-failure`
  -> 9/9 passed; `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` ->
  365 passed, 1 skipped.
  New differential cases compare all 3 preset tables to `fcvideo_ref.py` and compare a full
  converted frame using C-generated tables to the Python stream oracle. Further cases cover
  all 3 presets and 14 flash sets.
- Left out: engine frame input, device publication and timing, and real
  Mesen pixels remain open. Strict S0 is still gated by [I-19](issues/I-19-hw-trace.md)'s hardware count calibration.

## I-17 -- host video stream core (2026-09-21)
- Commits: this commit.
- What landed: a caller-owned pure-C converter for 320x200 index frames. It decimates to
  256 columns, letterboxes to 240 lines, chooses 16x16 sub-palettes with 12% hysteresis,
  dithers through supplied lookup tables, packs the exact 34-word/line stream including
  next-line prefetch, and writes the v2 palette/attribute mailbox. The tables are supplied
  by the caller; preset/PLAYPAL table generation remains to be implemented.
- Verified from the repository root:

  ```
  cmake --build build-host && ctest --test-dir build-host --output-on-failure
  # 9/9 passed
  cmake --build build-asan && ASAN_OPTIONS=detect_leaks=0 \
    ctest --test-dir build-asan --output-on-failure
  # 9/9 passed
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 358 passed, 1 skipped
  ```

- The differential Python test compiles the same C source as a host shared library and
  compares complete streams and attributes against `fcvideo_ref.py` for gradient,
  checkerboard and random frames, plus a repeated frame with hysteresis and a palette flash.
  Its 34-word/line layout matches the independently measured Mesen fetch-order gate below.
- Left out: device timing/publication, engine frame composition, table generation and
  Mesen pixel goldens. S0 DMA remains stopped pending [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); no displayed-pixel
  correctness claim is made from its current open-bus screenshot.

## I-15 -- Mesen PPU fetch-order gate and display-first order (2026-09-21)
- Commits: Mesen fork change and this parent commit.
- What landed: an optional per-frame PPU rendering fetch trace in the Mesen mapper and a
  default frame-121 trace in the S0 runner. The runner validates the scanline/cycle pattern,
  low/high bitplane address pairs and sprite-read count, then checks that debugger peeks do
  not change the trace. The workplan and issue index now put PPU emulation and host video
  conversion ahead of the remaining audio assets.
- Verified from the repository root, using the pinned local .NET/SDL toolchain:

  ```
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" doom/sim/mesen2/build.sh
  # 8/8 C tests pass; MesenCE builds
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180
  # exit 0; deterministic; 241 x 68 = 16388 background reads
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180 --cs1-mask 0xe000
  # exit 0; deterministic; 16388 background + 3856 sprite = 20244 reads
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 357 passed, 1 skipped
  python3 doom/tools/check_md_links.py doom
  # 50 files, 38 links, 0 broken
  python3 doom/tools/gen_protocol.py --check
  # exit 0
  ```

- Both fetch traces contain `$FF` for every rendering read: the measured heartbeat counts
  are still 16453 and 20309 against v1's 15490. Strict S0 therefore remains failing, and
  neither trace is a pixel golden or a hardware calibration. [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) must settle the
  counter before the display path can make those claims.
- Next: implement [I-17](issues/I-17-fcvideo-impl.md)'s host converter stages B-E against the Python reference, then feed
  engine frames through it after [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md)/[I-16](issues/I-16-stage-a-composition.md). Keep co-simulation as the picture gate.

## I-04 / P4-T1 -- host APU sequencer (2026-09-21)
- Commits: this commit.
- What landed: an allocation-free APUS reader with loop points inside silence runs,
  music and three SFX voices, priority-based voice replacement, DPCM triggers, pulse/noise
  steals and restoration, pause/resume, four-level music volume, the note-retrigger rule,
  and a 64-entry deferred write queue. A caller-supplied callback owns mailbox writes and
  its terminator. The sequencer limits itself to `APU_PAIRS_MAX_V2` pairs per heartbeat.
- Verified from the repository root:

  ```
  cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON
  cmake --build build-host
  ctest --test-dir build-host --output-on-failure
  # 8/8 passed, including test_sequencer (179 checks)
  cmake -S doom -B build-asan -G Ninja -DFCPICO_HOST_ONLY=ON \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g"
  cmake --build build-asan
  ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
  # 8/8 passed
  ```

- The unmodified sanitizer command ran every test body successfully but LeakSanitizer
  failed at process exit under this environment's ptrace restriction. Disabling only leak
  detection gave the clean ASan/UBSan run above; this module performs no allocation.
- Left out: engine sound/music adapters and asset conversion remain [P4-T2](plan/10-workplan.md#p4-t2-engine-soundmusic-modules----spec-06-api-table-size-m-depends-p4-t1-p1-t5)/[P4-T3](plan/10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap). No
  device or listening claim is made here.

## I-08 / I-09 / I-10 -- resumed acceptance verification (2026-09-21)
- Commits: this commit; resumed verification and the device-map artifact fix.
- Acceptance: finish the interrupted host, Python, generated-file, link and workflow checks,
  and verify the device build produces the map at the workflow's corrected path.
- Verified from the repository root:

  ```
  cmake -S doom -B /tmp/bh -G Ninja -DFCPICO_HOST_ONLY=ON -DFCPICO_BUILD_COSIM=ON
  cmake --build /tmp/bh
  ctest --test-dir /tmp/bh --output-on-failure
  # 100% tests passed, 0 tests failed out of 7
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 352 passed, 1 skipped in 3.59s
  python3 doom/tools/gen_protocol.py --check
  # exit 0
  python3 doom/tools/check_md_links.py doom
  # 50 file(s), 38 link(s) checked, 0 broken
  diff doom/ci/workflows/doom-device.yml .github/workflows/doom-device.yml
  # exit 0, identical
  ```

- Device verification, using the existing local picotool installation to avoid the SDK's
  network fetch (the first configure attempt failed because GitHub DNS was unavailable):

  ```
  PATH="$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH" \
  PICO_SDK_PATH="$HOME/.local/fcpico/pico-sdk" \
  cmake -S doom -B /tmp/fcpico-doom-device-resume -G Ninja \
      -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s \
      -Dpicotool_DIR=/tmp/fc-picotool-2.1.1/picotool
  cmake --build /tmp/fcpico-doom-device-resume --target fcpico_testpattern
  python3 doom/tools/flash_layout_check.py /tmp/fcpico-doom-device-resume/port/fcpico_testpattern.elf
  ```

  -> configure and all 125 build steps passed; UF2 and
  `/tmp/fcpico-doom-device-resume/fcpico_testpattern.elf.map` exist.
- Measurements: 64880 bytes, 87.6% flash budget free; `text 64880, data 0, bss 301868`,
  matching the earlier independent builds.
- Documentation: corrected the CI README introduction to acknowledge both active lanes.
- Left out: hardware acceptance still needs [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); strict S0 remains unresolved.
  [I-04](issues/I-04-fcapu-core.md) remains the next independent implementation task.

## I-15 -- co-simulation diagnostic histogram fix (2026-09-21)
- Commits: this commit.
- What changed: the S0 runner now filters its steady-state histogram to v1 heartbeat rows
  (65 CPU reads and a nonzero rendering-read count). Mesen logs ordinary CPU writes and
  startup transitions in the same trace, which previously polluted the histogram with
  zero and cumulative counter values.
- Verified:

  ```
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180
  ```

  -> deterministic diagnostic run; the steady histogram is `[16453]` for the `$F000` CS1
  decode, while DMA stops remain as expected because the hardware count discrepancy is not
  resolved. The focused regression test and full suite pass: `353 passed, 1 skipped`.
- A second diagnostic with `--cs1-mask 0xe000` reports `[20309]`, with 20,244 rendering
  reads and the same 65 CPU mailbox reads. Both masks remain deterministic under the
  per-frame debugger-peek run; neither can produce the firmware's expected 15,490 count.
- Left out: strict S0 remains gated on [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); no protocol or count constant was changed.

## I-08 / I-09 / I-10 -- device build acceptance (2026-09-20)
- Commits: this commit (build fix and records); the implementation landed earlier in
  `201ce2d` and `c9c4e12`.
- Verified, with `arm-gnu-toolchain-13.2.Rel1` from the repository root and pico-sdk 2.1.1:

  ```
  export PATH="$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH"
  cmake -S doom -B build-rp2350 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DPICO_SDK_PATH=<pico-sdk 2.1.1> -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s
  cmake --build build-rp2350 --target fcpico_testpattern
  python3 doom/tools/flash_layout_check.py build-rp2350/port/fcpico_testpattern.elf
  ```

  -> clean configure and build from an empty directory, `-Wall -Wextra -Werror`, and
  `fcpico_testpattern.elf/.bin/.hex/.uf2` plus `fcpico_testpattern.elf.map` produced.
  The host lane is unaffected: 7/7 ctests and 352 passed / 1 skipped as before.
- Measurements: firmware 64880 bytes of the 524288-byte budget below `FLASH_WHX_ADDR`,
  87.6% free; `text 64880, data 0, bss 301868`. Identical across two independent clean
  builds.
- Defect found and fixed: `port/CMakeLists.txt` set `-Wl,-Map` through `LINK_FLAGS`, but
  `pico_add_extra_outputs()` appends its own `-Map` afterwards and GNU ld honours the last
  one. The requested `port/fcpico_testpattern.map` was therefore never written, and the
  device workflow had been uploading an artifact path that could not exist. The redundant
  flag is gone and both copies of `doom-device.yml` now name the file the SDK actually
  writes, `build-rp2350/fcpico_testpattern.elf.map`.
- Correction to the status reconciliation entry below: it records that "the installed ARM
  compiler is 10.3.1, not the required 13.2.Rel1" and that "the device workflow exists only
  at `ci/workflows/doom-device.yml`". Both are wrong. `arm-gnu-toolchain-13.2.Rel1` has been
  in the repository root all along (it is covered by the root `.gitignore`, which is why a
  tree listing does not show it), `/usr/bin/arm-none-eabi-gcc` 10.3.1 is merely what is on
  `PATH`, and `.github/workflows/doom-device.yml` is present and byte-identical to its
  template. **Check the repository root before concluding a tool is missing.**
- Left out: this is build acceptance only. Nothing here has run on an RP2350 or a Famicom,
  so [I-08](issues/I-08-device-superbuild.md), [I-09](issues/I-09-fcbus-device.md) and [I-10](issues/I-10-testpattern-firmware.md) stay short of their hardware-facing claims and [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) still
  gates the bus model. The device lane was already active in `.github/workflows`.
- Plan changes: `port/CMakeLists.txt`, both `doom-device.yml` copies, [`ci/README.md`](ci/README.md),
  [`issues/README.md`](issues/README.md).

## P0-T11 / I-15 -- co-simulation runs, and it contradicts the plan (2026-09-20)
- Commits: `87862d0` (scope), this commit; fork `grebz-dev/MesenCE-FC-PICO` at `5c2de02e`.
- Verified: `doom/sim/mesen2/build.sh` -> MesenCE built with the FC PICO mapper, host
  cartmodel 7/7 ctests passed; `doom/sim/mesen2/run_scenario.sh S0 --diagnostic` -> exit 0,
  `debug_peeks_do_not_change_results: true`; `doom/sim/mesen2/run_scenario.sh S0` -> exit 1
  with the four strict errors listed below. Re-run a third time from a clean output
  directory: byte-identical hashes.
- Measurements (steady state, frames 121-300, one heartbeat per frame, 170 of 179
  post-startup heartbeats identical):

  | quantity | `$0000`-`$0FFF` decode | `$0000`-`$1FFF` decode |
  |---|---|---|
  | selected PPU reads per frame | 16388 = 241 x 68 | 20244 = 241 x 84 |
  | `$2007` reads per frame | 65 | 65 |
  | reported count | 16453 | 20309 |
  | expected `PPU_COUNT_VAL_V1` | 15490 | 15490 |
  | heartbeats / DMA stops | 2220 / 2220 | 4264 / 4264 |

  Run time: 3.5 s for the pair of determinism runs, against a < 2 min target. The MesenCE
  build is the whole cost of the lane.
- What this settles. The bridge is real: the unmodified tutorial PRG boots, `FP_COM_INI` is
  answered over the tutorial's own `PF_COM_DMOD` -> `DRQ` -> `DLD` bulk transfer, and
  debugger peeks across `$0000`-`$2FFF` on every frame change nothing, so the mapper's
  read predicate is correct. The `$2007` side matches the specification exactly: 65 reads =
  the 64-byte mailbox (`FC_COM_BUF_SIZE_V1`) plus the buffered-read dummy, which is also
  what the boot-ROM cycle harness measured independently.
- What this breaks. `PPU_PICTURE_COUNT` = 15426 cannot be the number of CS1-qualified PPU
  reads in a frame under any decode: widening the mask adds sprite fetches (16 per line,
  which is how the second column proves the tutorial's sprite pattern table is `$1000`),
  and narrowing it cannot remove background fetches without removing the picture. 68 is
  32 visible tiles plus the 2 tiles prefetched for the next line, 2 bytes each; 241 is 240
  visible lines plus the pre-render line. 15426 is exactly `241 x 64 + 2`, the same 241
  lines counting only the in-picture tiles, with the `+2` inside `PPU_COUNT_WINDOW`. So the
  counter constant and the 34-word stream line disagree by precisely the 4 prefetch bytes
  per line: 964 per frame, against an observed 962. This is the September 11 prior-art
  contradiction (`241 x 68 = 16388` consumed bytes versus `15426` counted picture reads)
  with a number attached.
- Two of the three candidate explanations in `plan/01` can now be closed off the bench, which
  is recorded as a sharpened [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9) rather than as an edit to `plan/01` ([I-19](issues/I-19-hw-trace.md) owns that file).
  The counter's qualifier is not in doubt: the tutorial configures `fcppu_rna` with
  `sm_config_set_jmp_pin(&cn, PI_CS1_BIT)`, so the count is "CS1-low reads per frame" exactly.
  And the cheap CS1 hypothesis is dead as an explanation of the count: a narrow `$0000`-`$0FFF`
  decode is still needed for the 68-byte line layout, and the `$0000`-`$1FFF` column proves the
  sprite fetches really are at `$1000`, but no address mask reaches 15426. What remains is
  whether the PIO counter misses the dots 321-336 prefetch strobes, or the frame is not
  consumed as 241 lines -- and one trace covering dots 241-340 of a single line distinguishes
  them.
- Left out, deliberately: no protocol constant was changed and no read was discarded to make
  the count check pass. Consequently the DMA stops on every heartbeat, every selected read
  returns open bus, and S0's screenshot is a white screen -- so no screenshot golden was
  frozen either. Strict S0 stays failing until [I-19](issues/I-19-hw-trace.md) answers the counter question.
  Doom-mode cartridge and scenarios S1-S6 remain future work behind [I-12](issues/I-12-engine-host-build.md).
- Local toolchain, no root required (WSL2 Ubuntu 22.04, the sandbox has no sudo):
  `curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0` installs the
  SDK into `~/.dotnet`; `apt-get download libsdl2-dev` plus `dpkg -x` into
  `~/.local/fcpico/sdl2` supplies the headers, after pointing that copy's `sdl2-config`
  `prefix` at itself, adding `-I.../include` and `-I.../include/x86_64-linux-gnu` to its
  `--cflags` (the Debian headers indirect through `SDL2/_real_SDL_config.h`) and `-L` to its
  `--libs`, and repointing `libSDL2.so` at the system runtime `libSDL2-2.0.so.0.18.2`, which
  was already installed. Then
  `PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet"`.
- Plan changes: [`plan/09-testing-ci.md`](plan/09-testing-ci.md) gains "S0 as measured" and a corrected CI paragraph;
  `ci/workflows/doom-cosim.yml` is rewritten against the commands that actually work and
  stays inactive; [`ci/README.md`](ci/README.md), `issues/I-15`, [`issues/README.md`](issues/README.md) and
  [`plan/10-workplan.md`](plan/10-workplan.md) updated. `plan/01` still carries the uncorrected prior-art
  paragraph; it is owned by neither this issue nor its author, and the measurement above is
  the input a future correction needs.

## Python development environment -- 2026-09-20
- Created `doom/.venv` with Python 3.12.11 and installed `tools/requirements.txt`, including
  pytest 9.1.1. The environment is ignored by Git.
- Verified: `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` ->
  348 passed, 1 skipped (the expected PIO IRQ emulation skip).
- Activate from the repository root with `source doom/.venv/bin/activate`, or invoke
  `doom/.venv/bin/python` directly. This resolves the missing-pytest limitation below;
  the ARM toolchain requirement remains 13.2.Rel1.

## Status reconciliation -- 2026-09-20
- Scope: documentation review and cleanup only; no implementation changes.
- Reviewed: plans, agent playbook, issue index, engine-side notes, hardware requests/log,
  current sources and commits through `e9526b9`. Engine submodule remains at `d8a20ca`;
  its FC PICO platform contains planning notes only.
- Verified: `cmake -S doom -B /tmp/fcpico-doom-status-host -G Ninja
  -DFCPICO_HOST_ONLY=ON && cmake --build /tmp/fcpico-doom-status-host &&
  ctest --test-dir /tmp/fcpico-doom-status-host --output-on-failure` -> 6/6 passed;
  `python3 doom/tools/gen_protocol.py --check` -> clean;
  `python3 doom/tools/check_md_links.py doom` -> no broken links.
- Verification limits: `python3 -m pytest doom/tests doom/sim -q` cannot run because
  pytest is not installed. The installed ARM compiler is 10.3.1, not the required
  13.2.Rel1; Wine and .NET are absent from PATH. Earlier test counts below are historical.
- Previously unlogged implementation: `201ce2d` added device CMake configuration,
  the RP2350 bus backend, test-pattern firmware, serial CLI, trace capture/decoder and
  synthetic trace tests ([I-08](issues/I-08-device-superbuild.md)/[I-09](issues/I-09-fcbus-device.md)/[I-10](issues/I-10-testpattern-firmware.md)). `c9c4e12` corrected DMA rearming and trace word
  alignment. `e9526b9` exposed boot-ROM emulator memory and added protocol assertions.
  These commits do not record device build acceptance. The device workflow exists only
  at `ci/workflows/doom-device.yml`, so [I-08](issues/I-08-device-superbuild.md)/[I-09](issues/I-09-fcbus-device.md)/[I-10](issues/I-10-testpattern-firmware.md) remain partial pending validation
  and review against their acceptance criteria.
- Milestone: M0 remains incomplete. No hardware session is recorded; [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) still gates
  bus-model calibration and board facts. Licensing enquiry [HA-1](HARDWARE-REQUESTS.md#ha-1-licensing-enquiry-task-p0-t13)/[I-18](issues/I-18-licensing.md) is also pending.
- Next: restore Python test dependencies and run the suite; finish device build/CI
  acceptance for [I-08](issues/I-08-device-superbuild.md) through [I-10](issues/I-10-testpattern-firmware.md). [I-04](issues/I-04-fcapu-core.md) (APU sequencer) is independent host work;
  [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md) begin the engine integration path.
- Cleanup: removed the superseded resume and September 11 review summaries. Their applied
  corrections remain in the specifications and the historical progress entry below;
  unresolved questions remain in [`plan/11-risks.md`](plan/11-risks.md) and the hardware requests.

## P1-T4 / P3-T1 -- host-testable controller mapper
- Commits: this commit
- Verified: `cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build
  build-host && ctest --test-dir build-host --output-on-failure` -> 6/6 passed.
- What landed: an allocation-free frame latch and tic poller, caller-owned event ring,
  configurable engine key codes, direction/action mapping, always-run, B tap-versus-strafe,
  Select tap-versus-automap, Select+Start pause, sticky presses, and an optional cheat matcher.
- Left out: engine event adapter and context-specific menu mappings remain integration work in
  the engine fork; the mapper deliberately has no dependency on Doom headers.
- Plan changes: none; issue [I-07](issues/I-07-input-mapper.md) is complete.

## P0-T12 -- activate the host CI lane
- Commits: this commit
- Verified: the host CMake build and 5 C tests passed; the ASan/UBSan build and the same 5
  tests passed; `python3 -m pytest doom/tests doom/sim/pioemu -q` -> 339 passed, 1 skipped;
  generated protocol and Markdown link checks passed.
- What landed: one active, host-only GitHub Actions workflow with pinned Ubuntu, Python and
  action major versions, pip caching, ordinary C tests, sanitizer C tests, Python and PIO
  tests, generated-file validation, Markdown link validation, and an exact template diff.
- Left out: device, boot-ROM, full-chip, co-simulation and documentation workflows remain
  inactive until the issue named for each lane can produce a green job.
- Plan changes: none; issue [I-06](issues/I-06-ci-host-lane.md) is complete.

## P1-T2 (reference part) -- reference video pipeline tests
- Commits: this commit
- Verified: `python3 -m pytest doom/tests/tools -q` -> 237 passed.
- Measurements: the deterministic 320x200 greyscale-gradient conversion decodes at 13.02 dB
  PSNR against its decimated and letterboxed source; the regression floor is 13.0 dB.
- What landed: stage B decimation and letterboxing tests; stage E exact-colour and midpoint
  dither tests; stage C palette selection and hysteresis boundary tests; a stage D Bayer-index
  test; and an end-to-end v2 stream conversion decoded through `ppu_decode.py`.
- Left out: the device converter remains issue [I-17](issues/I-17-fcvideo-impl.md); this issue only establishes the tested
  reference against which that implementation will be compared.
- Plan changes: none; issue [I-03](issues/I-03-fcvideo-ref-tests.md) is complete.

## Plan review and issue set -- 2026-09-11
- Commits: this commit
- Verified: every derivable figure in `plan/` recomputed from the primary sources (the
  tutorial's `rp_system.cpp`, `SysPico.asm`, `fcppu.pio`, the PiPU sources) with a throwaway
  Python script; `python3 tools/check_md_links.py` -> 54 files, 35 links, 0 broken.
- Result: ten arithmetic and consistency defects found and corrected in 01, 02, 03, 04, 06,
  07 and 09, plus three design gaps. The separate review summary was retired on 2026-09-20;
  its original audit trail remains in Git history. The
  largest correction replaces the prior-art counting paragraph in 01 with an honest statement
  that `241 x 68 = 16388` consumed bytes and `15426` counted picture reads cannot both be
  read as bytes-per-line arithmetic, and names the cheapest hypothesis to test (CS1 may
  decode only `$0000`-`$0FFF`, excluding sprite fetches). That question is now gated on the
  hardware trace, issue [I-19](issues/I-19-hw-trace.md), and no plan figure derived from it should be trusted until then.
- Also: 07 gained an init step that parks every OAM Y byte at `$EF` once, because sprites are
  deliberately left enabled in PPUMASK for fetch-pattern fidelity while cold-boot OAM is
  zeroed, which would draw 64 sprites in the corner.
- Deliverable: `../issues/` -- 19 self-contained issues plus an index, each with an Owns file
  list so two workers never collide, and acceptance stated as an exact command. Lane A ([I-01](issues/I-01-ppubus-tests.md)
  to [I-07](issues/I-07-input-mapper.md)) is verifiable in this sandbox today; lane B ([I-08](issues/I-08-device-superbuild.md) to [I-17](issues/I-17-fcvideo-impl.md)) cannot be (no
  `arm-none-eabi-gcc`, no Wine, no .NET, and the proxy returns 403 for the ARM toolchain), so
  each of those issues carries the CI job that proves it; lane C ([I-18](issues/I-18-licensing.md), [I-19](issues/I-19-hw-trace.md)) needs a person.
- Left out: filing these as GitHub issues. They are files on the branch, not tracker entries.
- Plan changes: 01, 02, 03, 04, 06, 07, 09, 10 (status table now points at [`../issues/README.md`](issues/README.md)).

## P0-T5 (core), P0-T6, P0-T3 (shim) -- the bus, host-testable end to end
- Commits: `ed582e6`
- Verified: `cmake -S doom -B build -DFCPICO_HOST_ONLY=ON && cmake --build build && ctest
  --test-dir build` -> 5/5 passed (`test_mailbox`, `test_rx`, `test_sync`, `test_stream`,
  `test_host_roundtrip`), and the same suite ASan-clean.
- What landed: `fcbus/fcbus_core.c` (mailbox builder, rx dispatcher, sync decision, stream
  addressing, attribute/palette shadows, data-mode responder -- no device dependency at all),
  `fcbus/fcbus_host.c`, and `sim/host_shim/` with pthread implementations of all 18 missing
  `multicore_*` symbols plus the 4 alarm functions. That answers plan question Q3: pico-sdk's
  host platform provides none of them, and it disables alarm pools outright via
  `PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1`. The `get_core_num()` override links because the
  SDK marks it weak.
- Two defects worth recording, both found by tests rather than by reading:
  the core kept a single attribute/palette shadow, so a bulk v2 upload sent only the four
  bytes per frame that the v1 poke trickle had synced; the tutorial keeps `m_ATR_W` and
  `m_ATR_W_old` separately, so the shadow is now split into `*_want` / `*_sent`.
  And the host backend did not count reads taken while the DMA was stopped, so a stopped
  frame reported count 0 forever and the link could never regain phase; the real `fcppu_rna`
  counts regardless of whether `fcppu_r` is running, which is precisely how phase recovers.
- Left out: the device backend (issue [I-09](issues/I-09-fcbus-device.md)) and the test-pattern firmware ([I-10](issues/I-10-testpattern-firmware.md)). Neither can
  be compiled here.

## P2-T1, P4-T3 (tools parts) -- tests for the drafted NES and audio tools
- Commits: `ed582e6`
- Verified: `python3 -m pytest doom/tests/tools -q` -> 171 passed.
- What landed: the tool drafts from `9e4450d` now have tests -- `tools/audio/apus.py` and
  `dpcm.py` (register sequences, DPCM encode/decode round trips, the retrigger-avoidance rule
  the FC PICO GB port documents), `tools/nes/bincut.py`, `bin2c.py`, `check_fixbank.py`,
  `tools/respack.py`, and `tools/fcpico/stream.py`. `check_fixbank.py` asserts the permanent
  `$F000`-`$FFFF` bank is byte-identical, which is the one mistake that bricks a cartridge.
- Left out: `tools/fcvideo_ref.py` and `tools/ppu_decode.py` are drafted and still untested
  (issues [I-03](issues/I-03-fcvideo-ref-tests.md) and [I-02](issues/I-02-ppu-decode-tests.md)), and `sim/ppubus/ppubus.py` is untested and uncalibrated ([I-01](issues/I-01-ppubus-tests.md)).

## P2-T2 (harness part) -- NMI cycle-count harness, measured on the tutorial ROM
- Commits: see `git log -- doom/tests/bootrom`
- Verified: `python3 -m pytest doom/tests/bootrom -q` -> 17 passed
- Measurements (`doom/tests/bootrom/tutorial_nmi_cycles.json`, py65 1.2.0, NTSC vblank 2273):
  tutorial NMI critical section 577 cycles; totals 1149 / 1341 / 1533 / 1724 for 0 / 8 / 16 / 24
  APU pairs with sprite DMA (+513 modelled); 65 `$2007` reads; 23.7 cycles per APU pair.
- Left out: the Doom ROM does not exist yet; the harness is parameterised for its 128-byte
  mailbox (`$20`-`$5F` + `$C0`-`$FF`) but RAM presets for it are TBD (see the README).
- Plan changes: 01 (measured NMI row, APU cost 24 cycles/pair), 07 (validation note).

## Planning

Planning completed; implementation tasks proceed per [`plan/10-workplan.md`](plan/10-workplan.md).
