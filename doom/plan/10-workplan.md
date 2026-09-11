# 10 -- Work plan

Phases map to milestones in 00. Tasks are written to be executed one at a time by an agent
following 12; each has a specification pointer, concrete steps, deliverables and an acceptance
test that a machine (or, where marked **HW**, a human with hardware) can run. Sizes: S (< 1
day of focused work), M (1-3 days), L (a week), XL (more). Dependencies are hard unless marked
"soft".

Conventions: paths are relative to `doom/` unless they start with `rp2040-doom/` (the engine
submodule) or `tutorial_project/`. "CI green" means the relevant workflow passes on the branch.

---

## Status (kept current; details in `../PROGRESS.md`)

| Task | State | Evidence |
|------|-------|----------|
| P0-T1 | not started (CI templates exist in `ci/workflows/`; the host-only CMake configuration is in progress) | -- |
| P0-T2, P0-T3 | not started; the host multicore shim (`sim/host_shim/`) is in progress | -- |
| P0-T4 | **done** | `tools/gen_protocol.py --check`; `pytest tests/protocol` (72) |
| P0-T5 | core logic in progress as `fcbus/fcbus_core.c` (host-testable); device backend and test-pattern firmware not started | -- |
| P0-T6 | in progress (`fcbus/fcbus_host.c`) | -- |
| P0-T7 | in progress (`sim/ppubus/`, `tools/ppu_decode.py`, `tools/fcvideo_ref.py`) | -- |
| P0-T8 | **done** (fcppu_dir skipped by design) | `pytest sim/pioemu` (13 passed, 1 skipped) |
| P0-T9, P0-T10 | blocked on hardware (`HARDWARE-REQUESTS.md` HR-1) | -- |
| P0-T11, P0-T12 | not started | -- |
| P0-T13 | partial: `LICENSES.md` drafted; enquiry HA-1 pending | -- |
| P2-T1 (tools part) | `tools/nes/bincut.py`, `bin2c.py`, `check_fixbank.py`, `tools/respack.py` drafted; tests in progress | -- |
| P2-T2 (harness part) | **done** for the tutorial ROM: `tests/bootrom/nmi_harness.py`, measurements in 01 | `pytest tests/bootrom` (17) |
| P4-T3 (partial) | `tools/audio/apus.py`, `dpcm.py`, `sfx2dpcm.py`, `dpcm_pack.py` drafted; tests in progress | -- |
| everything else | not started | -- |

---

## Phase 0 -- Foundations (milestone M0)

### P0-T1 Superbuild skeleton and LED-blink firmware
- Spec: 08. Size: M. Depends: --
- Steps: write `CMakeLists.txt`, `cmake/versions.cmake`, `cmake/boards/fcpico.h` (start from
  pico-sdk's `pico2.h`; add `PICO_FLASH_SIZE_BYTES (4*1024*1024)` and the FC PICO pin macros),
  `port/CMakeLists.txt` with `fcpico_testpattern` (blinks GP25, prints the unique ID and
  `flash_get_size()` guess over USB CDC), `tools/setup_env.sh`.
- Deliverables: UF2 builds locally and in `doom-build.yml` `firmware` job (run manually until
  P0-T12).
- Acceptance: `cmake --build` produces `fcpico_testpattern.uf2`; `picotool info -a` lists the
  binary info; size report artifact exists.

### P0-T2 Engine fork: superbuild guard and empty fcpico platform
- Spec: 08, `rp2040-doom/FCPICO-PORT.md`. Size: M. Depends: P0-T1
- Steps: in the submodule, add the `FCPICO_SUPERBUILD` guards; create `src/fcpico/` with
  `CMakeLists.txt` (`common_fcpico`), stub `i_video_fcpico.c` (no-op `I_InitGraphics`,
  `I_SetPaletteNum`, `pd_*` hooks satisfied by keeping `pd_render.cpp`), `i_input_fcpico.c`
  (UART path only), `i_sound_fcpico.c` (no-op modules); add `add_doom_tiny(_fcpico ...)` with
  the flag set from 08; commit and push the engine branch; advance the submodule pin.
- Acceptance: `fcpico_doom.elf` links for rp2350; `flash_layout_check.py` passes; the firmware
  boots to the serial banner and reaches `D_DoomMain` (prints "Z_Init" etc.) even though nothing
  is displayed.

### P0-T3 Engine host build without SDL
- Spec: 08. Size: M. Depends: P0-T2
- Steps: `sim/host_shim/` (pthread `multicore`, `sem`, `time_us_64`); CMake `PICO_PLATFORM=host`
  path for `fcpico_doom_host`; `--frames`, `--demo`, `--lockstep`, `--dump-8bit` (initially the
  raw `frame_buffer` as PNG) in `src/fcpico/host_main.c`; `cup.sh`-style embedding replaced by
  `--whx <file>` (mmap).
- Acceptance: `fcpico_doom_host --whx doom1.whx --demo 1 --frames 600` exits 0 in < 60 s; two
  runs give identical frame hashes.

### P0-T4 Protocol single source of truth
- Spec: 03. Size: S. Depends: --
- Steps: `fcbus/fcbus_protocol.h` (v1 + v2 constants, mailbox offsets, opcodes, key bits,
  counts); `tools/gen_protocol.py` -> `bootrom/gen/protocol.inc`, `tools/fcpico/protocol.py`;
  `--check`; `tests/protocol/test_v1_matches_tutorial.py` parses `tutorial_project/tuto1_hw/sys/rp_system.h`
  and `tutorial_project/BOOTROM/SysPico.asm` and asserts equality of every v1 value.
- Acceptance: tests pass; `--check` clean.

### P0-T5 `fcbus` device backend and test-pattern firmware
- Spec: 02, 03 (v1 only), `docs/pages/protocol.md`, `tutorial_project/tuto1_hw/sys/rp_system.cpp`, `rp_dma.cpp`, `rp_core0.h`. Size: L. Depends: P0-T1, P0-T4
- Steps: copy `fcppu.pio` verbatim (regenerate the header with `pioasm` at build time --
  `docs/pages/generated-resources.md` warns the checked-in `.pio.h` is stale); implement
  `fcbus_core.c` (pure logic: mailbox builder, rx dispatcher/state machine, sync decision,
  stream addressing, attribute/palette shadows, data-mode responder) and `fcbus_device.c`
  (PIO/DMA/IRQ glue mirroring `rp_system::init()`, `ppu_dma()`, `jobRcvCom()`, `rom_dma()`,
  `ver_dma()`, `drq_ret()`), with the ISR and DMA re-arm in RAM (`__not_in_flash_func`);
  `port/cli.c` (`stats`, `pattern`); test patterns (grey bars, checker, frame counter digits
  from a tiny font); boot ROM image served from `tutorial_project/BOOTROM/rom.NES` for now so
  the tutorial console runs v1 unchanged.
- Deliverables: `fcpico_testpattern.uf2`; `tests/fcbus/` for the core.
- Acceptance: ctest green. **HW**: with a console still carrying the tutorial boot ROM, the
  pattern displays; `stats` shows `ppu_count` = 15490 on every frame and zero resyncs over
  5 minutes.

### P0-T6 `fcbus` host backend
- Spec: 09 (L2/L3/L6). Size: M. Depends: P0-T5
- Steps: `fcbus_host.c`: `fcbus_host_ppu_read()` (next stream byte, count), `fcbus_host_ppu_write(b)`
  (rx dispatcher; heartbeat runs the ISR-equivalent synchronously), `fcbus_host_publish()`;
  deterministic; no threads inside `fcbus` itself.
- Acceptance: the same `tests/fcbus/` pass against the host backend; a test streams 3 frames
  and checks the mailbox lands at byte 15426 of each.

### P0-T7 PPU-bus model and decoder
- Spec: 09 L3, 01. Size: M. Depends: P0-T6
- Steps: `sim/ppubus/ppubus.c/.h` + `ppubus.py`; `tools/ppu_decode.py`; fault injection;
  `tests/ppubus/`.
- Acceptance: with default parameters the model counts 15490 reads per v1 frame; a test
  pattern published through the host backend decodes to the expected PNG; injected slips
  cause exactly one "stopped" frame and then resync.

### P0-T8 PIO program tests
- Spec: 09 L4. Size: S. Depends: --
- Acceptance: `pytest sim/pioemu` green for `fcppu_w`, `fcppu_rna`, `fcppu_r` (patched).

### P0-T9 Hardware trace capture **HW**
- Spec: 09 L7, 01. Size: M. Depends: P0-T5
- Steps: add `trace` to the CLI (PIO1 sampler + DMA, 32K words); `tools/trace_decode.py`
  (edge extraction, per-line qualifying-read counts, per-frame totals, `/RD` low width
  histogram); a human runs it on a real Famicom with the tutorial ROM and commits
  `tests/fixtures/hw_trace_ntsc.json` and the raw dump; also record `picotool info -a`,
  `flash_get_size()`, the console model.
- Acceptance: fixture committed; `01-constraints.md` flash size and CS1 rows updated from
  "(inferred)" to measured.

### P0-T10 Model calibration
- Spec: 01 "unresolved discrepancy", 09 L3. Size: S. Depends: P0-T9
- Steps: fit `reads_per_line`, `prerender_reads`, `cs1_mask` to the trace; record the
  explanation in 01; remove the `UNCALIBRATED` flag.
- Acceptance: model reproduces the fixture's per-line counts and 15490 total; L2/L3 goldens
  regenerated if the layout assumptions changed.

### P0-T11 Mesen2 co-simulation skeleton
- Spec: 09 L6. Size: L. Depends: P0-T6, P0-T7
- Steps: pin a Mesen2 commit as `sim/mesen2/Mesen2` (submodule) or a patch set; mapper
  `FcPico.h/.cpp` + factory registration; `sim/cartmodel/` C API over the host backend with the
  test-pattern generator; `set_mapper.py`; `sim/mesen2/build.sh`; Lua S0; `run_scenario.sh`.
- Acceptance: S0 passes headless on Linux; run time < 2 min after the cached build.

### P0-T12 Activate CI
- Spec: 09. Size: S. Depends: P0-T1..P0-T8, P0-T11 (soft: cosim can be activated later)
- Steps: copy `ci/workflows/*.yml` to `.github/workflows/`, adjust paths, run, fix.
- Acceptance: `doom-build`, `doom-sim`, `doom-docs` green on the branch; `doom-cosim` runs on
  demand.

### P0-T13 Licensing and attribution
- Spec: 11. Size: S. Depends: --
- Steps: `LICENSES.md` listing every component and licence; open the question to impact soft
  (record in `HARDWARE-REQUESTS.md` under "human actions") about redistributing the bus layer
  under a GPLv2-compatible licence; note that `fcppu.pio` carries Raspberry Pi's BSD-3 header;
  note the shareware WAD/WHX terms.
- Acceptance: file exists; no source file lacks a licence header in `doom/`.

---

## Phase 1 -- Grey Doom (milestone M1)

### P1-T1 Stage A: 8-bit frame composition
- Spec: 04 stage A, `rp2040-doom/src/pico/i_video.c`. Size: L. Depends: P0-T2, P0-T3
- Steps: `i_video_fcpico.c` composes 200 lines into `line8` via `fcvideo_line_sink()`; port
  `scanline_func_double/single/wipe/none`, `draw_vpatch` (8-bit), `new_frame_stuff`,
  `new_frame_init_overlays_palette_and_wipe` (palette part becomes `next_pal` capture); host
  `--dump-8bit` now dumps the composed frame.
- Acceptance: `tests/goldens/demo1_8bit/` hashes for frames 0..600 step 10; visually the title
  screen, menu, status bar and wipe match `chocolate-doom` screenshots of the same demo
  frames (human check once, then hashes).

### P1-T2 Stages B and D, grey LUT, letterbox
- Spec: 04. Size: M. Depends: P1-T1, P0-T6
- Steps: `fcvideo_convert_frame()` on host: decimation table, grey LUT (`PLAYPAL` luma ->
  16 dither levels over the 4 greys), Bayer 4x4, pack into `fcbus` stream; letterbox and
  prefetch words; unit tests vs `fcvideo_ref.py`.
- Acceptance: L2 goldens (`demo1/`) with PSNR thresholds; `ppu_decode.py` output visibly Doom.

### P1-T3 Device integration: cores, IRQs, semaphores
- Spec: 02. Size: M. Depends: P1-T2, P0-T5
- Steps: `core1()` installs `fcbus` ISR and `LOW_PRIO_IRQ` converter; `render_frame_ready` /
  `display_frame_freed` handoff; `__not_in_flash_func` audit of everything reachable from the
  ISR and from the converter during `VIDEO_TYPE_SAVING`; `stats` extended with conversion time.
- Acceptance: device boots, `stats` shows frames being published; no `hard_assert` on core 1
  malloc.

### P1-T4 Input v1 (subset)
- Spec: 05. Size: S. Depends: P0-T5
- Steps: raw pad byte -> Up/Down/Left/Right/A(fire)/B(use)/Start(escape)/Select(next weapon)
  edges -> `D_PostEvent`.
- Acceptance: unit test; on host `--pads` walks the player (position changes in the log).

### P1-T5 Sound stubs
- Spec: 06. Size: S. Depends: P0-T2
- Acceptance: engine never blocks in `I_UpdateSound`; `S_StartSound` calls are counted in the
  log (proves the module is wired).

### P1-T6 Flash layout, WHX placement, loading screen
- Spec: 08. Size: S. Depends: P0-T1
- Steps: `flash_layout.h`; `TINY_WAD_ADDR=0x10080000`; `tools/whx2uf2.py`; test pattern
  "FC PICO DOOM / LOADING" until the first engine frame; `flash_get_size()` check.
- Acceptance: layout check in CI; device shows LOADING then Doom.

### P1-T7 Co-simulation with the Doom model
- Spec: 09 L6 S2 (600 frames for now). Size: M. Depends: P0-T11, P1-T2
- Acceptance: S2 passes; screenshots archived.

### P1-T8 Hardware session M1 **HW**
- Size: S (human). Depends: P1-T3, P1-T6
- Checklist: title screen; DEMO1-3 loop; `stats` fps >= 15, conversion max, resyncs = 0 over
  30 min; photo of E1M1 start; log to `HARDWARE-LOG.md`.

### P1-T9 Converter performance pass
- Spec: 04. Size: M. Depends: P1-T8 (measurements)
- Acceptance: conversion max <= 8 ms, avg <= 5 ms at 150 MHz in E1M1 (`stats`).

---

## Phase 2 -- Colour Doom (milestone M2)

### P2-T1 Doom boot ROM source tree and Linux build
- Spec: 07, `docs/pages/build-pipeline.md`. Size: M. Depends: P0-T4
- Steps: `bootrom/src/` from `tutorial_project/BOOTROM/` (trimmed as listed in 07), `gen/`
  includes, `build.sh` (Wine), `tools/nes/bincut.py` + `bin2c.py`; MD5 gate on the tutorial ROM
  (`dbdate.h` pinned copy); RAM map (`defs/ram.inc`) after diffing both `SysEqu.h` files --
  the fix bank's addresses (`$00`-`$1F` scratch, `KEY_*` at `$82`-`$8C`, `PICO_DATA_BUF $400`,
  `FLASH_*_BUF $500`) are fixed; place the v2 mailbox at `$20`-`$5F` plus `$C0`-`$FF`.
- Acceptance: `doom.nes` reproducible; fix-bank bytes identical; CI `bootrom` job green.

### P2-T2 v2 NMI, init, main loop, controller packet
- Spec: 07, 03. Size: L. Depends: P2-T1
- Acceptance: `tests/bootrom/test_nmi_cycles.py`: critical section <= 1900, total <= 2200,
  register order; init sequence writes `FP_COM_INI 0`, `FP_COM_HELLO 2`.

### P2-T3 `fcbus` v2
- Spec: 03. Size: M. Depends: P0-T5, P0-T6, P0-T4
- Acceptance: `tests/fcbus/` v2 cases; host backend serves both; the state machine handles
  HELLO, timeouts, raw-byte-in-v2.

### P2-T4 Reflash path end to end
- Spec: 02 boot sequence, 07. Size: M. Depends: P2-T2, P2-T3, P0-T11
- Steps: firmware embeds `doom.nes` (via `respack.py`/`bin2c.py`) and serves `FP_COM_VER`/`FP_COM_ROM`
  from it; co-sim S1.
- Acceptance: S1 passes. **HW**: a console with the tutorial ROM shows "ROM UPDATE" once, then
  Doom with v2 (`stats` reports protocol 2, count 15554).

### P2-T5 Stages C and E
- Spec: 04. Size: L. Depends: P1-T2, P2-T3
- Acceptance: unit tests; L2 colour goldens; attribute hysteresis test; palette-flash test
  (damage in a scripted host run sets `PAL_VALID` with the red set).

### P2-T6 Palette preset evaluation
- Spec: 04 presets. Size: M. Depends: P2-T5
- Steps: `tools/palette_eval.py`: PSNR and a menu-font legibility metric over 200 demo frames
  for presets A/B and a k-means-derived C; write the report to `assets/palette_eval.md`;
  choose the default.
- Acceptance: report committed; default set in `fcvideo_presets.h`.

### P2-T7 Co-simulation consistency tests
- Spec: 09 S2 full. Size: M. Depends: P2-T4, P2-T5
- Acceptance: 3000-frame S2 with NMI-scanline, VRAM-equality and attribute-alternation checks.

### P2-T8 Hardware session M2 **HW**
- Checklist: reflash observed; colour E1M1; no attribute tearing; fps >= 20; 30-min soak.

---

## Phase 3 -- Playable (milestone M3)

### P3-T1 Full input mapping -- Spec 05. Size M. Depends P2-T3. Acceptance: mapper unit tests; menus navigable on host via `--pads`.
### P3-T2 Save and load -- Spec 08 flash layout, 04 SAVING. Size M. Depends P1-T3. Acceptance: co-sim S5; `FCPICO_AUTO_SAVENAME`; device saves survive power cycle (**HW**).
### P3-T3 Options page and config sector -- Size M. Depends P3-T1. Acceptance: always-run, turn speed, palette preset persisted; unit test of the config codec.
### P3-T4 Cheat sequences (optional) -- Size S. Depends P3-T1.
### P3-T5 Quit, console reset, heartbeat timeout -- Spec 05, 03 state machine. Size S. Depends P2-T3. Acceptance: host fault-injection tests; **HW**: pressing the console's reset returns to the title within 5 s.
### P3-T6 Second controller (optional) -- Size S. Depends P2-T2, P3-T1.
### P3-T7 Co-sim S3 and playtest **HW** -- Size M. Checklist: complete E1M1 with the controller; use every menu; save/load; automap.

---

## Phase 4 -- Audible (milestone M4)

### P4-T1 `fcapu` sequencer core -- Spec 06. Size L. Depends P0-T4. Acceptance: `tests/apu/`.
### P4-T2 Engine sound/music modules -- Spec 06 API table. Size M. Depends P4-T1, P1-T5.
### P4-T3 Audio tools -- Spec 06 pipeline. Size L. Depends --. Acceptance: `tests/tools/` round trips; `mus2apus.py --auto` produces all 13 streams from `doom1.wad` under the cap.
### P4-T4 Arrangements -- Size L (human-assisted). Depends P4-T3. Acceptance: FamiStudio projects for `D_E1M1`, `D_INTRO`, `D_INTROA`, `D_INTER` committed; VGM export reproducible via CLI; others auto.
### P4-T5 DPCM bank and boot ROM v3 -- Spec 06, 07. Size M. Depends P2-T2, P4-T3. Acceptance: `dpcm_pack.py` budget check; stamp bump; NMI cycle test unchanged; co-sim: controller input stays correct while DPCM plays (Mesen emulates the DMC/controller conflict).
### P4-T6 Co-sim S4 and listening session **HW** -- Size S.

---

## Phase 5 -- Release (milestone M5)

### P5-T1 Overclock evaluation -- Spec 01, `rp2040-doom/src/i_main.c`. Size M. Depends P1-T9. Steps: 200/250/270 MHz with `vreg` and QMI timing; PIO bus validation (trace + stats) at each; fps gain table. Acceptance: chosen clock documented; **HW** 1-h soak at that clock.
### P5-T2 Adaptive palettes (optional) -- Size L. Depends P2-T6.
### P5-T3 HUD and text polish (optional custom 256-wide status bar in the letterbox) -- Size L.
### P5-T4 rp2040js full-chip harness (optional) -- Spec 09 L5. Size M.
### P5-T5 Documentation -- Size M. Steps: Doxygen pages `docs/pages/doom-*.md` (architecture, protocol v2, build, user guide, troubleshooting); update `docs/pages/references.md`; user flashing guide with the reflash warning.
### P5-T6 Release -- Size S. Steps: tag; artifacts `fcpico_doom.uf2`, `doom1_whx.uf2`, `doom.nes`, checksums; release notes.
### P5-T7 NES front-loader validation **HW** -- Size S. Adapter with CIC; document what worked.

---

## Milestone gates

| Gate | Must be true |
|------|--------------|
| M0 | P0-T1..T13 done; CI green; fixture committed; calibration explanation in 01 |
| M1 | P1-T1..T8 done; S2(600) green; hardware log entry with fps >= 15 |
| M2 | P2-T1..T8; S1, S2(3000) green; hardware log with reflash + colour + fps >= 20 |
| M3 | P3-T1, T2, T3, T5, T7; S3, S5 green; playtest checklist complete |
| M4 | P4-T1..T6; S4 green; listening checklist |
| M5 | P5-T1, T5, T6 (+ optional); soak 1 h; release artifacts |
