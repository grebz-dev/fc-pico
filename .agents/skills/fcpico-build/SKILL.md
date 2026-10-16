---
name: fcpico-build
description: Build FC PICO artifacts reproducibly on Linux - the 6502 boot ROM and fix bank, the tuto1_hw resource archive (res.bin, res_id.h, resdata.c), the Arduino firmware UF2, the host platform libraries and tests - and prove what was built with a manifest. Use this whenever someone wants to rebuild, assemble, package or flash-prepare FC PICO code or data, changes BOOTROM sources, dbdate.h, resources or the protocol header, asks whether a build is reproducible, or needs a UF2 without the Windows tools or the Arduino IDE.
---

# FC PICO build

Builds go through `tools/build/fcpico_build.py`, which pins tools, checks outputs and
writes `build/manifest.json`. Reference: `docs/pages/build-pipeline.md` (the Windows
route and the ROM/firmware coupling) and `docs/pages/reproducible-build.md`.

## Before building

- Linux with a C compiler, CMake, make, git, Python 3.10+. Run
  `python3 tools/build/fcpico_build.py setup` once (add `--device` for firmware).
  The script stops with a message naming any missing system tool; report that to the
  user rather than installing system packages yourself.
- Decide whether the boot ROM should keep its stamp. A new stamp (`--stamp`, or
  editing `dbdate.h`) forces every console to reflash and must ship with firmware
  carrying the same ROM. Keep the committed `dbdate.h` unless the user wants a new
  ROM version.
- If `platform/fcbus/fcbus_protocol.h` changed, run `python3 tools/gen_protocol.py`
  and commit its outputs; the build checks they are fresh.

## Steps

1. Pick the stages that cover the change:
   - boot ROM sources → `rom resources device`
   - firmware sources → `resources device`
   - platform, simulator or tools → `host`
   - everything → `all`
2. Run `python3 tools/build/fcpico_build.py <stages>`. Add `--verify-twice` when the
   question is reproducibility, or before handing an artifact to someone else.
3. Read `build/manifest.json` and the notes it prints.

## What the checks mean

- `rom`: the fix bank must be byte-identical to `BOOTROM/bootrom_fixr.bin`, and with
  the committed `dbdate.h` the ROM must match the distributed MD5. A fix-bank
  mismatch means the change touched the permanent bank: stop and report it, because
  that bank can never be updated on a console.
- `resources`: with an unchanged ROM the archive must equal the committed files.
- `device`: the UF2 must target RP2350 ARM Secure and embed the device ROM. An
  emulator-only copy (private mapper 4093 from `tools/nes/set_mapper.py`) must never
  reach a UF2.
- `--verify-twice`: lists identical and differing artifacts. Report differences
  with their first differing byte rather than ignoring them.

## Done when

The requested stages pass and you can name, for each artifact the user needs, its
path and SHA-256 from `build/manifest.json`, plus whether the ROM stamp changed. If
the build is meant for a console, hand over to `fcpico-hardware-validation`.
