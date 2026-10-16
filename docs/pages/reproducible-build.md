@page reproducible_build Reproducible Linux build

`tools/build/fcpico_build.py` builds the platform on Linux with pinned tools and
records what it built. It is optional: the Windows tools and the Arduino IDE
described in @ref build_pipeline and @ref dev_setup keep working unchanged.

```sh
python3 tools/build/fcpico_build.py setup --device       # .venv, NESASM CE, Arduino toolchain
python3 tools/build/fcpico_build.py host rom resources device
python3 tools/build/fcpico_build.py demo                  # host tests + emulator demo
python3 tools/build/fcpico_build.py all --verify-twice
```

| Stage | Produces | Checks |
|---|---|---|
| `setup` | `.venv` with `tools/` installed; `.cache/nesasm`; with `--device`, `.cache/arduino` | Downloads match pinned checksums (`tools/build/arduino.json`) |
| `host` | `build/platform` with the native library | All C and Python tests; generated protocol files fresh |
| `rom` | `build/rom/rom.NES`, `bootrom_fixr.bin`, listings, symbols | Permanent bank unchanged; MD5 of the distributed `rom.NES` (@ref build_pipeline) |
| `resources` | `res.bin`, `res_id.h`, `resdata.c` for `tuto1_hw`, built from the rebuilt ROM with `tools/nes/respack.py` and `bin2c.py` | Identical to the committed files when the ROM is unchanged |
| `device` | `build/device/out/tuto1_hw.ino.uf2` from arduino-cli | UF2 targets RP2350 ARM Secure and embeds the device boot ROM, never an emulator-only header copy |
| `emulator` | MesenCE with the cartridge model (@ref emulator_cartridge) | Mapper check and the S0, S1 and demo scenarios |
| `demo` | `host`, then the emulator's demo scenario | One command for a fresh checkout |

Each run writes `build/manifest.json`: the source commit and whether the tree was
dirty, compiler, CMake, NESASM, Arduino and .NET versions, the pinned inputs, and
the SHA-256 of every artifact.

`--verify-twice` copies the source tree twice, builds the selected stages in each
copy with the shared tool caches, and reports which artifacts are identical and the
first differing byte of any that are not (`build/verify/report.json`). The ROM, the
resources, the native library and the firmware are identical across copies:
source paths are mapped out of the firmware with `-ffile-prefix-map`.

## Pinned versions

| Tool | Version |
|---|---|
| NESASM CE | `6fc41cda37b934aa29aa2639d0baa74424268e31` |
| arduino-cli | 1.5.1 (SHA-256 in `tools/build/arduino.json`) |
| arduino-pico core | `rp2040:rp2040@6.0.0`, board `rpipico2` (the core the distributed UF2s were built with) |
| BackgroundAudio | 1.4.4 |
| MesenCE | the commit and archive checksum in `sim/mesen/manifest.json` |

A firmware built here is not byte-identical to the distributed
`upload/tuto1_hw/tuto1_hw.ino.uf2`: that file embeds the absolute Windows paths of
the machine it was built on. It has the same size.

## Requirements

A C compiler, CMake, make, git and Python 3.10 or later; `patch`, SDL2 development
files and the .NET SDK for the emulator. The script installs nothing system-wide and
stops with a message naming any missing tool. `--mesen-make-arg` passes extra make
arguments to the emulator build, for example an `SDL2LIB=...` override.
