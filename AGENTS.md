# FC PICO: notes for coding agents

FC PICO is a Famicom/NES cartridge with an RP2350 that streams pattern data to the
console's PPU. The console runs a small 6502 boot ROM; the cartridge firmware is an
Arduino sketch. This file is the starting point for agents; the documentation in
`docs/pages/` is the reference and is built with Doxygen.

## Repository map

| Path | Contents |
|---|---|
| `tutorial_project/BOOTROM_FIX/`, `BOOTROM/` | Permanent fix bank and erasable boot ROM (6502, nesasm) |
| `tutorial_project/tuto1_hw/` | Tutorial firmware (Arduino sketch); `sys/rp_system.cpp` is the bus logic |
| `tutorial_project/sample_game/`, `tuto3_3d/` | Further sketches |
| `tutorial_project/bin/`, `upload/` | Windows tools and distributed UF2s |
| `platform/fcbus/` | Portable bus core, host backend, protocol header and generated files |
| `sim/cartmodel/` | Host cartridge model for emulators, with application providers |
| `sim/mesen/` | MesenCE integration: pinned source, patch series, scenarios |
| `tools/` | `build/` (native ROM, reproducible build), `nes/` (ROM tools), `fcpico/` (Python package), `gen_protocol.py` |
| `tests/`, `platform/tests/` | Python and C tests |
| `docs/pages/` | Documentation (English). `docs_jp/` is maintained by the upstream author: do not edit it |
| `.agents/skills/` | Task workflows for agents (see below) |

## Commands

Linux, from the repository root:

```sh
python3 tools/build/fcpico_build.py setup                   # .venv and pinned NESASM CE
python3 tools/build/fcpico_build.py host                    # platform build, C and Python tests
python3 tools/build/fcpico_build.py rom resources           # boot ROM and tuto1_hw resources
python3 tools/build/fcpico_build.py demo                    # host tests + emulator demo
python3 tools/gen_protocol.py --check                       # generated protocol files are fresh
```

Firmware with arduino-cli: `setup --device`, then the `device` stage. The emulator
needs SDL2 and the .NET SDK: `python3 sim/mesen/mesen.py build`, then
`python3 sim/mesen/scenario.py run S0`. Windows users keep the `.bat` scripts and
the Arduino IDE (`docs/pages/build-pipeline.md`, `dev-setup.md`).

## Rules that are easy to break

- **The fix bank is permanent.** `BOOTROM/bootrom_fixr.bin` (`$F000`-`$FFFF`) is
  never rewritten on a console. Any boot ROM change must leave those 4096 bytes
  identical; `tools/build/build_rom.sh` checks this.
- **The ROM stamp couples ROM and firmware.** `BOOTROM/m.BAT` and
  `build_rom.sh --stamp` write a new `dbdate.h`. A ROM with a new stamp makes every
  console reflash itself until the firmware carries the same ROM, so rebuild the
  resources and the firmware with it, or keep the committed `dbdate.h`.
- **Generated files are not edited by hand.** `platform/fcbus/fcbus_protocol.h` is
  the only source of protocol constants; run `tools/gen_protocol.py` after changing
  it and commit the outputs. `tuto1_hw/res/res.bin`, `res_id.h` and `resdata.c` come
  from `conv.bat` or `tools/nes/respack.py` and `bin2c.py`.
- **The tutorial protocol does not change silently.** Wire values must stay those of
  `rp_system.h` and `SysPico.asm`; `platform/tests/test_protocol_v1.py` checks them.
- **The bus path is interrupt-driven and time-critical.** `rp_system::jobRcvCom()`
  runs in the PIO interrupt; do not add serial output, allocation or blocking there
  (`docs/pages/dev-setup.md`).
- **Sources are UTF-8** with Japanese comments preserved; do not re-encode files.
- **Dependencies are tracked here, not in submodules.** MesenCE changes are patches
  in `sim/mesen/patches/` against the pinned source in `sim/mesen/manifest.json`;
  tool versions are pinned in `tools/build/arduino.json` and the scripts.
- **The emulator models the protocol, not the hardware.** A passing scenario does not
  prove timing. Record hardware results only from a real console, and say which
  checks were not run (`docs/pages/platform-findings.md`).

## Where to look

| Task | Read first | Skill |
|---|---|---|
| Build the ROM, resources, firmware or the whole platform | `docs/pages/build-pipeline.md`, `reproducible-build.md` | `fcpico-build` |
| Run or test something in the emulator | `docs/pages/emulator-cartridge.md` | `fcpico-cosim` |
| Read counts, heartbeats, DMA stops, PIO or mailbox problems | `docs/pages/protocol.md`, `platform-findings.md` | `fcpico-bus-debug` |
| Tiles, palettes, attributes, sprites | `docs/pages/graphics.md`, `platform-findings.md` | `fcpico-native-graphics` |
| Preparing or recording a test on a console | `docs/pages/flashing.md`, `boot-and-reflash.md` | `fcpico-hardware-validation` |
| Preparing a change for upstream | `docs/pages/conventions.md` | `fcpico-upstream-change` |
| Bus core or host model internals | `docs/pages/shared-bus-core.md` | |
| Python inspection | `docs/pages/python-tools.md` | |

Skills live in `.agents/skills/<name>/SKILL.md` (Agent Skills format).
