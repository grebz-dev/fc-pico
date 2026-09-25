# 10 -- Work plan

Phases map to milestones in 00. Tasks are written to be executed one at a time by an agent
following 12; each has a specification pointer, concrete steps, deliverables and an acceptance
test that a machine (or, where marked **HW**, a human with hardware) can run. Sizes: S (< 1
day of focused work), M (1-3 days), L (a week), XL (more). Dependencies are hard unless marked
"soft".

Conventions: paths are relative to `doom/` unless they start with `rp2040-doom/` (the engine
submodule) or `tutorial_project/`. "CI green" means the relevant workflow passes on the branch.

## Current execution priority (2026-09-23)

The display and PPU path is the next integration priority. Milestone IDs below still describe
deliverables, but the actionable order is now:

1. Keep the now-calibrated strict S0 count, mailbox, fetch and screenshot gates green while
   integrating the display path. [HR-1](../HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](../issues/I-19-hw-trace.md) measured the RP2350/board behavior on NES-001.
2. Complete [I-11](../issues/I-11-engine-skeleton.md)/[I-12](../issues/I-12-engine-host-build.md)/[I-16](../issues/I-16-stage-a-composition.md) so the engine produces composed 8-bit frames. [I-17](../issues/I-17-fcvideo-impl.md)'s host
   converter tables, presets and synthetic flash palettes are now implemented and tested;
   feed engine frames through that converter next.
3. Integrate converter publication to `fcbus` and validate displayed frames in Mesen S2.
   Freeze screenshot goldens only after strict S0 has established the PPU path.
4. Resume the remaining audio assets and other downstream work after the display path has a
   passing co-simulation gate. [I-04](../issues/I-04-fcapu-core.md)/[P4-T1](10-workplan.md#p4-t1-fcapu-sequencer-core----spec-06-size-l-depends-p0-t4-acceptance-testsapu) is already complete and needs no further core work.

The remaining hardware visual check for the corrected stream is [HR-2](../HARDWARE-REQUESTS.md#hr-2-corrected-stream-geometry-visual-check-task-i-17). A
fixed-frame Doom stream now passes D0 (tutorial v1) and D1 (Doom v2) Mesen
gates, including v2 palette/attribute RAM and NMI exit timing. S1 now models the
fix bank's flash erase/reprogram and passes in both directions (tutorial -> Doom,
Doom -> tutorial); it found and fixed a stamp that would have halted every
tutorial console at PICO NOT FOUND. The first Doom hardware boot is [HR-3](../HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6); dynamic
frame-sequence co-simulation is the next software gate.

---

## Status (kept current; details in `../PROGRESS.md`)

The remaining work is cut into individually assignable pieces in [`../issues/README.md`](../issues/README.md); that
index, not this table, is the entry point for picking up work. The table below records what
has actually landed on the branch.

| Task | State | Evidence |
|------|-------|----------|
| [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware) | partial: device configuration and test-pattern target landed; device workflow is a template, build acceptance pending ([I-08](../issues/I-08-device-superbuild.md)) | `201ce2d`; [`../PROGRESS.md`](../PROGRESS.md) |
| [P0-T2](10-workplan.md#p0-t2-engine-fork-superbuild-guard-and-empty-fcpico-platform) | partial: RP2350 engine skeleton links; hardware boot remains ([I-11](../issues/I-11-engine-skeleton.md)) | GCC 13.2.Rel1 `fcpico_doom.elf` and flash-layout check |
| [P0-T3](10-workplan.md#p0-t3-engine-host-build-without-sdl) | host multicore/alarm shim implemented (`sim/host_shim/`, 22 symbols); the engine host build itself is issue [I-12](../issues/I-12-engine-host-build.md) | standalone shim harness; not included in the six host-only C tests |
| [P0-T4](10-workplan.md#p0-t4-protocol-single-source-of-truth) | **done** | `tools/gen_protocol.py --check`; `pytest tests/protocol` (72) |
| [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware) | core logic **done**; device backend and test-pattern firmware landed, device acceptance pending ([I-09](../issues/I-09-fcbus-device.md)/[I-10](../issues/I-10-testpattern-firmware.md)) | five bus C tests; `201ce2d`, `c9c4e12` |
| [P0-T6](10-workplan.md#p0-t6-fcbus-host-backend) | **done** | `ctest` target `test_host_roundtrip`; ASan-clean |
| [P0-T7](10-workplan.md#p0-t7-ppu-bus-model-and-decoder) | **done**, calibrated model and decoder | `tests/tools/test_ppubus.py`, `tests/tools/test_trace_decode.py`, hardware trace 6 |
| [P0-T8](10-workplan.md#p0-t8-pio-program-tests) | **done** (`fcppu_dir` skipped by design: pioemu has no IRQ support) | `pytest sim/pioemu` (13 passed, 1 skipped) |
| [P0-T9](10-workplan.md#p0-t9-hardware-trace-capture-hw), [P0-T10](10-workplan.md#p0-t10-model-calibration) | **done** ([HR-1](../HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](../issues/I-19-hw-trace.md)) | `tests/fixtures/hw_trace_ntsc/trace_6.*`; strict S0 |
| [P0-T11](10-workplan.md#p0-t11-mesen2-co-simulation-skeleton) | **done**; strict calibrated S0 passes | `sim/mesen2/build.sh`, `run_scenario.sh S0`; reviewed ARGB golden |
| [P0-T12](10-workplan.md#p0-t12-activate-ci) | host CI lane **done** ([I-06](../issues/I-06-ci-host-lane.md)); device and other lanes remain templates | `.github/workflows/doom-host.yml` |
| [P0-T13](10-workplan.md#p0-t13-licensing-and-attribution) | partial: [`LICENSES.md`](../LICENSES.md) drafted; the enquiry is issue [I-18](../issues/I-18-licensing.md) | -- |
| [P2-T1](10-workplan.md#p2-t1-doom-boot-rom-source-tree-and-linux-build) (tools part) | `tools/nes/bincut.py`, `bin2c.py`, `check_fixbank.py`, `tools/respack.py` **done with tests** | `pytest tests/nes` |
| [P2-T2](10-workplan.md#p2-t2-v2-nmi-init-main-loop-controller-packet) (harness part) | **done** for the tutorial ROM; memory inspection assertions added in `e9526b9` | historical measurements in [01](01-constraints.md); current Python rerun pending dependencies |
| [P2-T2](10-workplan.md#p2-t2-v2-nmi-init-main-loop-controller-packet) (Doom NMI) | not started (issues [I-13](../issues/I-13-bootrom-tree.md), [I-14](../issues/I-14-bootrom-nmi.md)) | -- |
| [P4-T1](10-workplan.md#p4-t1-fcapu-sequencer-core----spec-06-size-l-depends-p0-t4-acceptance-testsapu) | **done** ([I-04](../issues/I-04-fcapu-core.md), host APU sequencer) | 8/8 host and 8/8 sanitizer C tests; [`../PROGRESS.md`](../PROGRESS.md) |
| [P4-T3](10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap) (tools part) | `tools/audio/apus.py`, `dpcm.py`, `sfx2dpcm.py`, `dpcm_pack.py` **done with tests**; remaining conversion tooling is [I-05](../issues/I-05-audio-tools.md) | `pytest tests/audio` |
| [P1-T2](10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox) (reference part) | **done** ([I-03](../issues/I-03-fcvideo-ref-tests.md)); device converter remains [I-17](../issues/I-17-fcvideo-impl.md) | reference pipeline tests; [`../PROGRESS.md`](../PROGRESS.md) |
| [P1-T2](10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox) / [P2-T5](10-workplan.md#p2-t5-stages-c-and-e) (host converter part) | partial: C conversion, presets, table generation and flash palettes match the Python reference; engine frames and device integration remain [I-17](../issues/I-17-fcvideo-impl.md) | 9/9 host and sanitizer C tests; Python differential tests; [`../PROGRESS.md`](../PROGRESS.md) |
| [P1-T4](10-workplan.md#p1-t4-input-v1-subset) / [P3-T1](10-workplan.md#p3-t1-full-input-mapping----spec-05-size-m-depends-p2-t3-acceptance-mapper-unit-tests-menus-navigable-on-host-via---pads) (input) | [P1-T4](10-workplan.md#p1-t4-input-v1-subset) software **done**: mapper, RP2350 adapter, and host `--pads` movement test; physical pad validation is [HR-4](../HARDWARE-REQUESTS.md#hr-4-controller-input-on-doom-task-p1-t4). [P3-T1](10-workplan.md#p3-t1-full-input-mapping----spec-05-size-m-depends-p2-t3-acceptance-mapper-unit-tests-menus-navigable-on-host-via---pads) context mappings remain | `test_mapper`, `engine_input_walk`; [`../PROGRESS.md`](../PROGRESS.md) |
| everything else | not started; see [`../issues/README.md`](../issues/README.md) for the assignable subset | -- |

Verification on 2026-09-20 (full evidence and environment limits in [`../PROGRESS.md`](../PROGRESS.md)):

```
cmake -S doom -B /tmp/fcpico-doom-status-host -G Ninja -DFCPICO_HOST_ONLY=ON
cmake --build /tmp/fcpico-doom-status-host
ctest --test-dir /tmp/fcpico-doom-status-host --output-on-failure -> 6/6 passed
doom/.venv/bin/python -m pytest doom/tests doom/sim -q -> 348 passed, 1 skipped
python3 doom/tools/gen_protocol.py --check -> clean
python3 doom/tools/check_md_links.py doom -> no broken links
```

---

## Phase 0 -- Foundations (milestone M0)

### P0-T1 Superbuild skeleton and LED-blink firmware
- Spec: [08](08-build.md). Size: M. Depends: --
- Steps: write `CMakeLists.txt`, `cmake/versions.cmake`, `cmake/boards/fcpico.h` (start from
  pico-sdk's `pico2.h`; add `PICO_FLASH_SIZE_BYTES (4*1024*1024)` and the FC PICO pin macros),
  `port/CMakeLists.txt` with `fcpico_testpattern` (blinks GP25, prints the unique ID and
  `flash_get_size()` guess over USB CDC), `tools/setup_env.sh`.
- Deliverables: UF2 builds locally and in `doom-build.yml` `firmware` job (run manually until
  [P0-T12](10-workplan.md#p0-t12-activate-ci)).
- Acceptance: `cmake --build` produces `fcpico_testpattern.uf2`; `picotool info -a` lists the
  binary info; size report artifact exists.

### P0-T2 Engine fork: superbuild guard and empty fcpico platform
- Spec: [08](08-build.md), [`rp2040-doom/FCPICO-PORT.md`](../rp2040-doom/FCPICO-PORT.md). Size: M. Depends: [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware)
- Steps: in the submodule, add the `FCPICO_SUPERBUILD` guards; create `src/fcpico/` with
  `CMakeLists.txt` (`common_fcpico`), stub `i_video_fcpico.c` (no-op `I_InitGraphics`,
  `I_SetPaletteNum`, `pd_*` hooks satisfied by keeping `pd_render.cpp`), `i_input_fcpico.c`
  (UART path only), `i_sound_fcpico.c` (no-op modules); add `add_doom_tiny(_fcpico ...)` with
  the flag set from [08](08-build.md); commit and push the engine branch; advance the submodule pin.
- Acceptance: `fcpico_doom.elf` links for rp2350; `flash_layout_check.py` passes; the firmware
  boots to the serial banner and reaches `D_DoomMain` (prints "Z_Init" etc.) even though nothing
  is displayed.

### P0-T3 Engine host build without SDL
- Spec: [08](08-build.md). Size: M. Depends: [P0-T2](10-workplan.md#p0-t2-engine-fork-superbuild-guard-and-empty-fcpico-platform)
- Steps: `sim/host_shim/` (pthread `multicore`, `sem`, `time_us_64`); CMake `PICO_PLATFORM=host`
  path for `fcpico_doom_host`; `--frames`, `--demo`, `--lockstep`, `--dump-8bit` (initially the
  raw `frame_buffer` as PNG) in `src/fcpico/host_main.c`; `cup.sh`-style embedding replaced by
  `--whx <file>` (mmap).
- Acceptance: `fcpico_doom_host --whx doom1.whx --demo 1 --frames 600` exits 0 in < 60 s; two
  runs give identical frame hashes.

### P0-T4 Protocol single source of truth
- Spec: [03](03-protocol-v2.md). Size: S. Depends: --
- Steps: `fcbus/fcbus_protocol.h` (v1 + v2 constants, mailbox offsets, opcodes, key bits,
  counts); `tools/gen_protocol.py` -> `bootrom/gen/protocol.inc`, `tools/fcpico/protocol.py`;
  `--check`; `tests/protocol/test_v1_matches_tutorial.py` parses `tutorial_project/tuto1_hw/sys/rp_system.h`
  and `tutorial_project/BOOTROM/SysPico.asm` and asserts equality of every v1 value.
- Acceptance: tests pass; `--check` clean.

### P0-T5 `fcbus` device backend and test-pattern firmware
- Spec: [02](02-architecture.md), [03](03-protocol-v2.md) (v1 only), [`docs/pages/protocol.md`](../../docs/pages/protocol.md), `tutorial_project/tuto1_hw/sys/rp_system.cpp`, `rp_dma.cpp`, `rp_core0.h`. Size: L. Depends: [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware), [P0-T4](10-workplan.md#p0-t4-protocol-single-source-of-truth)
- Steps: copy `fcppu.pio` verbatim (regenerate the header with `pioasm` at build time --
  [`docs/pages/generated-resources.md`](../../docs/pages/generated-resources.md) warns the checked-in `.pio.h` is stale); implement
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
- Spec: [09](09-testing-ci.md) (L2/L3/L6). Size: M. Depends: [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware)
- Steps: `fcbus_host.c`: `fcbus_host_ppu_read()` (next stream byte, count), `fcbus_host_ppu_write(b)`
  (rx dispatcher; heartbeat runs the ISR-equivalent synchronously), `fcbus_host_publish()`;
  deterministic; no threads inside `fcbus` itself.
- Acceptance: the same `tests/fcbus/` pass against the host backend; a test streams 3 frames
  and checks the mailbox lands at byte 15426 of each.

### P0-T7 PPU-bus model and decoder
- Spec: [09](09-testing-ci.md) L3, [01](01-constraints.md). Size: M. Depends: [P0-T6](10-workplan.md#p0-t6-fcbus-host-backend)
- Steps: `sim/ppubus/ppubus.c/.h` + `ppubus.py`; `tools/ppu_decode.py`; fault injection;
  `tests/ppubus/`.
- Acceptance: with default parameters the model counts 15490 reads per v1 frame; a test
  pattern published through the host backend decodes to the expected PNG; injected slips
  cause exactly one "stopped" frame and then resync.

### P0-T8 PIO program tests
- Spec: [09](09-testing-ci.md) L4. Size: S. Depends: --
- Acceptance: `pytest sim/pioemu` green for `fcppu_w`, `fcppu_rna`, `fcppu_r` (patched).

### P0-T9 Hardware trace capture **HW**
- Spec: [09](09-testing-ci.md) L7, [01](01-constraints.md). Size: M. Depends: [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware)
- Steps: add `trace` to the CLI (PIO1 sampler + DMA, 32K words); `tools/trace_decode.py`
  (edge extraction, per-line qualifying-read counts, per-frame totals, `/RD` low width
  histogram); a human runs it on a real Famicom with the tutorial ROM and commits
  `tests/fixtures/hw_trace_ntsc.json` and the raw dump; also record `picotool info -a`,
  `flash_get_size()`, the console model.
- Acceptance: fixture committed; [`01-constraints.md`](01-constraints.md) flash size and CS1 rows updated from
  "(inferred)" to measured.

### P0-T10 Model calibration
- Spec: [01](01-constraints.md) hardware calibration, [09](09-testing-ci.md) L3. Size: S. Depends: [P0-T9](10-workplan.md#p0-t9-hardware-trace-capture-hw)
- Steps: fit `reads_per_line`, `prerender_reads`, `cs1_mask` to the trace; record the
  explanation in [01](01-constraints.md) and make the trace-backed values the model defaults.
- Acceptance: model reproduces the fixture's per-line counts and 15490 total; L2/L3 goldens
  regenerated if the layout assumptions changed.

### P0-T11 Mesen2 co-simulation skeleton
- Spec: [09](09-testing-ci.md) L6. Size: L. Depends: [P0-T6](10-workplan.md#p0-t6-fcbus-host-backend), [P0-T7](10-workplan.md#p0-t7-ppu-bus-model-and-decoder)
- Steps: pin `grebz-dev/MesenCE-FC-PICO` as `sim/mesen2/Mesen2` (submodule); mapper
  `FcPico.h/.cpp` + factory registration; `sim/cartmodel/` C API over the host backend with the
  test-pattern generator; `set_mapper.py`; `sim/mesen2/build.sh`; Lua S0; `run_scenario.sh`.
- Acceptance: S0 passes headless on Linux; run time < 2 min after the cached build.
  A diagnostic-only run is a bring-up checkpoint, not satisfaction of S0 or hardware calibration.

### P0-T12 Activate CI
- Spec: [09](09-testing-ci.md). Size: S. Depends: [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware)..[P0-T8](10-workplan.md#p0-t8-pio-program-tests), [P0-T11](10-workplan.md#p0-t11-mesen2-co-simulation-skeleton) (soft: cosim can be activated later)
- Steps: copy `ci/workflows/*.yml` to `.github/workflows/`, adjust paths, run, fix.
- Acceptance: `doom-build`, `doom-sim`, `doom-docs` green on the branch; `doom-cosim` runs on
  demand.

### P0-T13 Licensing and attribution
- Spec: [11](11-risks.md). Size: S. Depends: --
- Steps: [`LICENSES.md`](../LICENSES.md) listing every component and licence; open the question to impact soft
  (record in [`HARDWARE-REQUESTS.md`](../HARDWARE-REQUESTS.md) under "human actions") about redistributing the bus layer
  under a GPLv2-compatible licence; note that `fcppu.pio` carries Raspberry Pi's BSD-3 header;
  note the shareware WAD/WHX terms.
- Acceptance: file exists; no source file lacks a licence header in `doom/`.

---

## Phase 1 -- Grey Doom (milestone M1)

### P1-T1 Stage A: 8-bit frame composition
- Spec: [04](04-video.md) stage A, `rp2040-doom/src/pico/i_video.c`. Size: L. Depends: [P0-T2](10-workplan.md#p0-t2-engine-fork-superbuild-guard-and-empty-fcpico-platform), [P0-T3](10-workplan.md#p0-t3-engine-host-build-without-sdl)
- Steps: `i_video_fcpico.c` composes 200 lines into `line8` via `fcvideo_line_sink()`; port
  `scanline_func_double/single/wipe/none`, `draw_vpatch` (8-bit), `new_frame_stuff`,
  `new_frame_init_overlays_palette_and_wipe` (palette part becomes `next_pal` capture); host
  `--dump-8bit` now dumps the composed frame.
- Acceptance: `tests/goldens/demo1_8bit/` hashes for frames 0..600 step 10; visually the title
  screen, menu, status bar and wipe match `chocolate-doom` screenshots of the same demo
  frames (human check once, then hashes).

### P1-T2 Stages B and D, grey LUT, letterbox
- Spec: [04](04-video.md). Size: M. Depends: [P1-T1](10-workplan.md#p1-t1-stage-a-8-bit-frame-composition), [P0-T6](10-workplan.md#p0-t6-fcbus-host-backend)
- Steps: `fcvideo_convert_frame()` on host: decimation table, grey LUT (`PLAYPAL` luma ->
  16 dither levels over the 4 greys), Bayer 4x4, pack into `fcbus` stream; letterbox and
  prefetch words; unit tests vs `fcvideo_ref.py`.
- Acceptance: L2 goldens (`demo1/`) with PSNR thresholds; `ppu_decode.py` output visibly Doom.

### P1-T3 Device integration: cores, IRQs, semaphores
- Spec: [02](02-architecture.md). Size: M. Depends: [P1-T2](10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox), [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware)
- Steps: `core1()` installs `fcbus` ISR and `LOW_PRIO_IRQ` converter; `render_frame_ready` /
  `display_frame_freed` handoff; `__not_in_flash_func` audit of everything reachable from the
  ISR and from the converter during `VIDEO_TYPE_SAVING`; `stats` extended with conversion time.
- Acceptance: device boots, `stats` shows frames being published; no `hard_assert` on core 1
  malloc.

### P1-T4 Input v1 (subset)
- Spec: [05](05-input.md). Size: S. Depends: [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware)
- Steps: raw pad byte -> Up/Down/Left/Right/A(fire)/B(use)/Start(escape)/Select(next weapon)
  edges -> `D_PostEvent`.
- Acceptance: unit test; on host `--pads` walks the player (position changes in the log).

### P1-T5 Sound stubs
- Spec: [06](06-audio.md). Size: S. Depends: [P0-T2](10-workplan.md#p0-t2-engine-fork-superbuild-guard-and-empty-fcpico-platform)
- Acceptance: engine never blocks in `I_UpdateSound`; `S_StartSound` calls are counted in the
  log (proves the module is wired).

### P1-T6 Flash layout, WHX placement, loading screen
- Spec: [08](08-build.md). Size: S. Depends: [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware)
- Steps: `flash_layout.h`; `TINY_WAD_ADDR=0x10080000`; `tools/whx2uf2.py`; test pattern
  "FC PICO DOOM / LOADING" until the first engine frame; `flash_get_size()` check.
- Acceptance: layout check in CI; device shows LOADING then Doom.

### P1-T7 Co-simulation with the Doom model
- Spec: [09](09-testing-ci.md) L6 S2 (600 frames for now). Size: M. Depends: [P0-T11](10-workplan.md#p0-t11-mesen2-co-simulation-skeleton), [P1-T2](10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox)
- Acceptance: S2 passes; screenshots archived.

### P1-T8 Hardware session M1 **HW**
- Size: S (human). Depends: [P1-T3](10-workplan.md#p1-t3-device-integration-cores-irqs-semaphores), [P1-T6](10-workplan.md#p1-t6-flash-layout-whx-placement-loading-screen)
- Checklist: title screen; DEMO1-3 loop; `stats` fps >= 15, conversion max, resyncs = 0 over
  30 min; photo of E1M1 start; log to [`HARDWARE-LOG.md`](../HARDWARE-LOG.md).

### P1-T9 Converter performance pass
- Spec: [04](04-video.md). Size: M. Depends: [P1-T8](10-workplan.md#p1-t8-hardware-session-m1-hw) (measurements)
- Acceptance: conversion max <= 8 ms, avg <= 5 ms at 150 MHz in E1M1 (`stats`).

---

## Phase 2 -- Colour Doom (milestone M2)

### P2-T1 Doom boot ROM source tree and Linux build
- Spec: [07](07-bootrom.md), [`docs/pages/build-pipeline.md`](../../docs/pages/build-pipeline.md). Size: M. Depends: [P0-T4](10-workplan.md#p0-t4-protocol-single-source-of-truth)
- Steps: `bootrom/src/` from `tutorial_project/BOOTROM/` (trimmed as listed in [07](07-bootrom.md)), `gen/`
  includes, `build.sh` (Wine), `tools/nes/bincut.py` + `bin2c.py`; MD5 gate on the tutorial ROM
  (`dbdate.h` pinned copy); RAM map (`defs/ram.inc`) after diffing both `SysEqu.h` files --
  the fix bank's addresses (`$00`-`$1F` scratch, `KEY_*` at `$82`-`$8C`, `PICO_DATA_BUF $400`,
  `FLASH_*_BUF $500`) are fixed; place the v2 mailbox at `$20`-`$5F` plus `$C0`-`$FF`.
- Acceptance: `doom.nes` reproducible; fix-bank bytes identical; CI `bootrom` job green.

### P2-T2 v2 NMI, init, main loop, controller packet
- Spec: [07](07-bootrom.md), [03](03-protocol-v2.md). Size: L. Depends: [P2-T1](10-workplan.md#p2-t1-doom-boot-rom-source-tree-and-linux-build)
- Acceptance: `tests/bootrom/test_nmi_cycles.py`: critical section <= 1900, total <= 2200,
  register order; init sequence writes `FP_COM_INI 0`, `FP_COM_HELLO 2`.

### P2-T3 `fcbus` v2
- Spec: [03](03-protocol-v2.md). Size: M. Depends: [P0-T5](10-workplan.md#p0-t5-fcbus-device-backend-and-test-pattern-firmware), [P0-T6](10-workplan.md#p0-t6-fcbus-host-backend), [P0-T4](10-workplan.md#p0-t4-protocol-single-source-of-truth)
- Acceptance: `tests/fcbus/` v2 cases; host backend serves both; the state machine handles
  HELLO, timeouts, raw-byte-in-v2.

### P2-T4 Reflash path end to end
- Spec: [02](02-architecture.md) boot sequence, [07](07-bootrom.md). Size: M. Depends: [P2-T2](10-workplan.md#p2-t2-v2-nmi-init-main-loop-controller-packet), [P2-T3](10-workplan.md#p2-t3-fcbus-v2), [P0-T11](10-workplan.md#p0-t11-mesen2-co-simulation-skeleton)
- Steps: firmware embeds `doom.nes` (via `respack.py`/`bin2c.py`) and serves `FP_COM_VER`/`FP_COM_ROM`
  from it; co-sim S1.
- Acceptance: S1 passes. **HW**: a console with the tutorial ROM shows "ROM UPDATE" once, then
  Doom with v2 (`stats` reports protocol 2, count 15554).

### P2-T5 Stages C and E
- Spec: [04](04-video.md). Size: L. Depends: [P1-T2](10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox), [P2-T3](10-workplan.md#p2-t3-fcbus-v2)
- Acceptance: unit tests; L2 colour goldens; attribute hysteresis test; palette-flash test
  (damage in a scripted host run sets `PAL_VALID` with the red set).

### P2-T6 Palette preset evaluation
- Spec: [04](04-video.md) presets. Size: M. Depends: [P2-T5](10-workplan.md#p2-t5-stages-c-and-e)
- Steps: `tools/palette_eval.py`: PSNR and a menu-font legibility metric over 200 demo frames
  for presets A/B and a k-means-derived C; write the report to `assets/palette_eval.md`;
  choose the default.
- Acceptance: report committed; default set in `fcvideo_presets.h`.

### P2-T7 Co-simulation consistency tests
- Spec: [09](09-testing-ci.md) S2 full. Size: M. Depends: [P2-T4](10-workplan.md#p2-t4-reflash-path-end-to-end), [P2-T5](10-workplan.md#p2-t5-stages-c-and-e)
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
| M0 | [P0-T1](10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware)..T13 done; CI green; fixture committed; calibration explanation in [01](01-constraints.md) |
| M1 | [P1-T1](10-workplan.md#p1-t1-stage-a-8-bit-frame-composition)..T8 done; S2(600) green; hardware log entry with fps >= 15 |
| M2 | [P2-T1](10-workplan.md#p2-t1-doom-boot-rom-source-tree-and-linux-build)..T8; S1, S2(3000) green; hardware log with reflash + colour + fps >= 20 |
| M3 | [P3-T1](10-workplan.md#p3-t1-full-input-mapping----spec-05-size-m-depends-p2-t3-acceptance-mapper-unit-tests-menus-navigable-on-host-via---pads), T2, T3, T5, T7; S3, S5 green; playtest checklist complete |
| M4 | [P4-T1](10-workplan.md#p4-t1-fcapu-sequencer-core----spec-06-size-l-depends-p0-t4-acceptance-testsapu)..T6; S4 green; listening checklist |
| M5 | [P5-T1](10-workplan.md#p5-t1-overclock-evaluation----spec-01-rp2040-doomsrci_mainc-size-m-depends-p1-t9-steps-200250270-mhz-with-vreg-and-qmi-timing-pio-bus-validation-trace--stats-at-each-fps-gain-table-acceptance-chosen-clock-documented-hw-1-h-soak-at-that-clock), T5, T6 (+ optional); soak 1 h; release artifacts |

## Changelog

- 2026-09-20: reconciled landed work and current verification limits with source and progress.
