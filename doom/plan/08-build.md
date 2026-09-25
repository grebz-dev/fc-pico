# 08 -- Build system, repositories, flash layout

## Hardware milestone build

The 2026-09-25 shadow-detail/B-use firmware works on the user's NES setup. Rebuild
from the repository root:

```bash
doom/tools/build_shadow_detail.sh --bootstrap-tools
```

Requires CMake, Ninja, a host C/C++ compiler, make, git, Python 3, the project-local
`arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi`, and a recursive Pico SDK 2.1.1
checkout (default `$HOME/.local/fcpico/pico-sdk`). The bootstrap option downloads
pinned native picotool 2.1.1 and NESASM CE if missing, building under `.build/tools`.
It does not install system packages. Later builds can omit `--bootstrap-tools`.
For existing installations:

```bash
PICOTOOL_DIR=/path/to/install/lib/cmake/picotool \
NESASM_BIN=/path/to/nesasm \
PICO_SDK_PATH=/path/to/pico-sdk \
doom/tools/build_shadow_detail.sh
```

`BUILD_DIR`, `TOOLS_DIR`, `ARM_TOOLCHAIN_PATH` and `JOBS` are optional overrides.
Outputs live in `.build/doom-shadow/artifacts/`: the merged
`fcpico_doom_shadow_detail_delay_whx.uf2`, firmware-only UF2 and ELF, `doom.nes`,
`SHA256SUMS`, `CMakeCache.txt`, revision/tool information and the tracked-source diff.
Commit source changes before a release build; untracked source files are not included
in the diff artifact. Build paths/dates/configuration can affect binary hashes.

The script explicitly enables `FCPICO_DIAGNOSTIC_ENGINE_DELAY=ON`: the 30-second
startup countdown, bus statistics and raw read counter were part of the successful
hardware profile. Removing that profile caused USB/boot failures in two rebuilds;
restoring it works. Do not remove it until the dependency is diagnosed and verified
on hardware. During flashing keep the NES off, allow USB startup/countdown to finish,
then power the NES on. The firmware accepts `bootsel` over USB serial.

Use picotool to create firmware UF2s. The bundled `tutorial_project/bin/picotool.exe`
works through WSL Windows interop and reports v2.3.0; its conversion was byte-identical
to native v2.1.1 for this ELF. The script uses native v2.1.1 for SDK CMake integration.
If Windows interop is sandbox-blocked, obtain permission for that command or use the
native tool. Do not substitute a hand-written raw-binary UF2 wrapper.

The hardware-tested merged file has SHA-256
`3d298001d93f028dd6a3e9f37287e8d77762704b0c1295485c8fd22275f88dbc`,
8158 blocks and 287836 firmware bytes. Its observed conversion average/max is
38208/38251 us. See [hardware evidence](../HARDWARE-LOG.md).

## Repositories and branches

| Repository | Branch | Role |
|------------|--------|------|
| `grebz-dev/fc-pico` | `claude/doom-fc-pico-nes-2bb1bx` (based on `docs/utf8-and-doxygen`) | product: `doom/` |
| `grebz-dev/rp2040-doom` | `claude/doom-fc-pico-nes-2bb1bx` (based on `rp2`, the RP2350-capable branch) | engine: new platform layer `src/fcpico/` |

`fc-pico/doom/rp2040-doom` is a git submodule pinned to a commit on the engine branch. Engine
changes are made in the submodule checkout, committed and pushed there first, then the pin is
advanced in `fc-pico`. The engine's own submodule `3rdparty/tinyusb` is not initialised for the
FC PICO target.

## Directory layout (fc-pico)

```
doom/
  CMakeLists.txt            superbuild root (pico-sdk import, options, add_subdirectory of everything)
  cmake/                    toolchain pins, board file fcpico.h, helper functions
  fcbus/                    bus library: fcppu.pio, fcbus.c/.h, fcbus_protocol.h, backends device/ host/
  port/                     main.c, flash_layout.h, board bring-up, serial CLI, resource archive
  port/apu/                 fcapu sequencer (engine-independent core)
  bootrom/                  6502 sources, gen/ (generated includes), build.sh, doom.nes output ignored
  tools/                    Python tools (see 09) + nes/ (bincut, bin2c, respack), audio/, gen_protocol.py
  tests/                    ctest C tests, pytest Python tests, goldens/
  sim/                      ppubus/ model, pioemu/ tests, mesen2/ mapper + lua, fullchip/ scripts
  assets/                   music projects (.fms), sfx overrides, palette presets, test frames
  ci/workflows/             GitHub Actions templates (copied to .github/workflows/ in P0-T12)
  rp2040-doom/              submodule
```

## Superbuild

`doom/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
set(PICO_BOARD fcpico CACHE STRING "")              # doom/cmake/boards/fcpico.h: pico2 + 4 MB flash + pins
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "")     # or "host" for the simulation build
include(rp2040-doom/pico_sdk_import.cmake)          # honours PICO_SDK_PATH / FETCH_FROM_GIT
project(fcpico_doom C CXX ASM)
pico_sdk_init()
set(FCPICO_SUPERBUILD 1)                            # tells the engine tree not to init the SDK again
add_subdirectory(fcbus)
add_subdirectory(port/apu)
add_subdirectory(rp2040-doom EXCLUDE_FROM_ALL)      # engine libraries + doom_tiny_fcpico target
add_subdirectory(port)                              # fcpico_doom (UF2), fcpico_testpattern (UF2)
if (FCPICO_BUILD_TESTS) enable_testing(); add_subdirectory(tests) endif()
```

Engine-side changes needed for this to work (documented in the fork's [`FCPICO-PORT.md`](../rp2040-doom/FCPICO-PORT.md)):

- top-level `CMakeLists.txt`: `if (NOT FCPICO_SUPERBUILD)` around `pico_sdk_import`,
  `pico_extras_import`, `project()`, `pico_sdk_init()`; `pico-extras` becomes optional (only the
  VGA targets need `pico_scanvideo_dpi` and `pico_audio_i2s`).
- `src/CMakeLists.txt`: `add_doom_tiny(_fcpico render_newhope)` guarded by `FCPICO_SUPERBUILD`,
  linking `fcbus_device` (or `fcbus_host`), `fcapu`, `fcinput`, `fcvideo` and
  `common_fcpico` (a sibling of `common_pico` with the new
  `i_video/i_input/i_sound` files), and defining `FCPICO=1`, `NO_USE_ENDDOOM=1`, `USE_PICO_NET=0`,
  `USB_SUPPORT=0`, `TINY_WAD_ADDR=0x10080000`, `PICO_CORE1_STACK_SIZE=0x1000`,
  `DEMO1_ONLY` **off** (all demos; the RP2350 build fits and boots).
- `src/pico/` stays as is for the VGA targets; `src/fcpico/` reuses `i_system.c`, `i_timer.c`,
  `picoflash.c`, `w_file_static.c`, `stubs.c`, `blit.S` from it by listing them explicitly.

## Toolchain pins

| Tool | Version | Why |
|------|---------|-----|
| pico-sdk | 2.1.1 (tag) -- or the newest 2.x at Phase 0 if it builds cleanly; record it in `cmake/versions.cmake` | RP2350 support, picotool 2.1.1 pairing |
| arm-none-eabi-gcc | 13.2.Rel1 | RP2040 Doom's README: size-sensitive, 10.x known bad |
| picotool | 2.1.1 | UF2 + `load -o` for the WHX |
| CMake / Ninja | >= 3.20 / any | |
| Python | 3.11+ with `pytest`, `numpy`, `pillow`, `py65`, `rp2040-pio-emulator` | tools and tests |
| host gcc | 13.x | host builds, `whd_gen`, `chocolate-doom` (needs SDL2 dev packages) |
| NESASM CE | pinned `6fc41cda` | native Linux boot ROM build and tutorial MD5 gate; Wine is a fallback |
| .NET SDK 10 | for the pinned MesenCE fork | co-simulation only |
| Node 20 | optional | `rp2040js` full-chip sim (RP2040 build only) |

The active GitHub workflows and `doom/tools/setup_env.sh` record the commands for
their respective lanes. Templates in `ci/workflows/` describe later lanes; see
[09](09-testing-ci.md#github-actions) for active versus planned workflows.

## Targets

| Target | Platform | Output | Purpose |
|--------|----------|--------|---------|
| `fcpico_testpattern` | rp2350 | `.uf2` | M0: `fcbus` + a moving test pattern + serial CLI; no engine |
| `doom_tiny_fcpico` | rp2350 | `fcpico_doom.uf2`, `.elf`, `.map`, size report | Doom firmware |
| `doom_tiny_fcpico` | host | `fcpico_doom_host` executable | SDL-free engine + fcvideo; runs demos headless, dumps streams/PNGs and accepts `--pads` |
| `fcbus_host`, `fcvideo_host`, `fcapu_host` | host | static libs | used by tests and `sim/` |
| `whd_gen`, `mus2mid` | host | executables | from the engine tree (`chocolate-doom` native build) |
| `bootrom` | -- | `doom.nes`, `doom_bootrom.h` | via `bootrom/build.sh` (pinned native NESASM CE); CMake custom target |
| `resources` | -- | `fcres.h` | boot ROM image + music + SFX archive as a C array |

The original engine host build relies on `pico_host_sdl` for `scanvideo`; the fcpico
host target now builds without SDL. Inspection of pico-sdk 2.1.1 (`src/host/`): the base host
platform provides `pico_stdlib`, `pico_time` (via `pico_time_adapter`), `hardware_sync`,
`hardware_gpio`, `hardware_irq`, `hardware_timer`, `hardware_divider`, `pico_printf` and the
**headers only** of `pico_multicore` (`multicore_launch_core1`, the inter-core FIFO, lockout);
its README says the implementations of multicore, alarms and audio/scanvideo come from
`pico-host-sdl`. There is no PIO or DMA on the host, which is why `fcbus` has a separate host
backend. `sim/host_shim/` therefore implements, with pthreads: `multicore_launch_core1`
(thread), the FIFO (mutex + condvar queue), `multicore_lockout_*` (no-ops), and whatever
alarm-pool function the engine reaches (`I_GetTime` uses `time_us_64()`, which the base host
provides). Semaphores (`pico/sem.h`) and spin locks are common code and work on the host as is.
[P0-T3](10-workplan.md#p0-t3-engine-host-build-without-sdl) confirmed the shim by linking
and running deterministic host engine tests, including scripted `--pads` input.

## Flash layout (`port/flash_layout.h`, checked by `tools/flash_layout_check.py`)

| Offset | Size | Contents |
|--------|------|----------|
| `0x10000000` | <= 512 KB | firmware (`fcpico_doom.uf2`): code, boot ROM image, music, SFX scripts, DPCM originals not needed |
| `0x10080000` | 1,800,344 B (`doom1.whx`) | WHX, loaded separately: `picotool load -v -t bin doom1.whx -o 0x10080000` |
| `0x10300000` | <= 512 KB | optional asset archive (reserved; empty in v1) |
| `0x10380000` - `0x10400000` | 512 KB | save slots (RP2040 Doom's `get_end_of_flash()` scheme, 7 slots x 64 KB max) |

The `flash_get_size()` result is printed at boot and compared with the layout; a mismatch
disables saving and shows a warning on the title screen rather than corrupting anything.

Flashing sequence for users: (1) hold BOOT, connect USB-C, copy `fcpico_doom.uf2`; (2)
`picotool load -t bin doom1.whx -o 0x10080000` (or a second UF2 produced by
`tools/whx2uf2.py` so that step 2 is also drag-and-drop); (3) insert into the console; the
console reflashes its boot ROM on first boot ("ROM UPDATE"), then Doom starts.

## Reproducibility and size

- No timestamps anywhere: the boot ROM stamp comes from `version.inc`; `__DATE__` is not used.
- `-DCMAKE_BUILD_TYPE=MinSizeRel` for device builds (the engine warns otherwise).
- The size report (`arm-none-eabi-size` + `tools/flash_layout_check.py`) is uploaded by CI and
  the build fails if `.text+.rodata+.data` in flash exceed `TINY_WAD_ADDR - 0x10000000`.
- `picotool info -a` output is part of the CI artifacts (pins, binary info).

## What is not touched

`tutorial_project/` (the Arduino firmware, the tutorial boot ROM, the Windows tools) remains
buildable exactly as documented in [`docs/pages/build-pipeline.md`](../../docs/pages/build-pipeline.md). The Doom boot ROM copies
files out of it at Phase 2 rather than modifying them; the fix bank binary is copied, never
rebuilt.
