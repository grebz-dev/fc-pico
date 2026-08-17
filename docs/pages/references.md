@page references References and attribution

## Project

- **Product page** — <https://impactsoft.booth.pm/items/7736698>
  Manual, latest firmware and sample project. Also links impact soft's walkthroughs for
  updating the firmware, installing the development environment, and building the
  sample.
- **Support** — impact soft on X: <https://x.com/HD64180>
- **FC PICO 付属マニュアル** (`FC-PICO付属マニュアル.pdf`)
  The manual shipped with the cartridge. Confirms the Pico 2-class microcontroller, the
  BOOT-button UF2 update procedure, Arduino IDE support, and the console-compatibility
  caveat.

@note The reference PDFs are excluded by `.gitignore` and are not in the repository.

## The technique

- **NES DOOM 的な技術の解説** by Norix (`NES-DOOM的技術の解説.pdf`)
  The source for @ref nes_doom. Covers the Famicom's dual-bus layout, the PPU's
  per-scanline fetch pattern, why the address lines can be ignored, the sprite and OAM
  DMA limitations, and the author's own DTA cartridge which solved them with a PIC32MZ
  and a CPLD.
- **NES DOOM** by Andrew Tait, 2019 — Raspberry Pi 3A+ and EZ-USB FX2LP.
  <https://github.com/rasteri/PiPU>
- **NESDev Wiki** — <https://www.nesdev.org/wiki/PPU_rendering> and
  <https://www.nesdev.org/wiki/NTSC_video>
  The authoritative description of PPU fetch order and timing.
- **DESTROY THEM ALL (DTA)** by Norix — the follow-up cartridge described in the PDF.
  Prototype only; not produced in quantity.

## Tools

- **NSD.Lib (NES Sound Driver Library)** by e.itikawa —
  <https://shaw.la.coocan.jp/nsdl/doc/index.html>
  The MML compiler (`nsc`) and 6502 sound driver (`nsd.bin`). See @ref audio_page.
- **NSF specification** by Kevtris — `kevtris.org/nes/nsfspec.txt`
  `rp_nsfplayer.cpp` carries a Japanese translation of the init/play procedure.
- **anago / kazzo** — Famicom cartridge dumper/writer used by `rom/rom.bat`.
- **NESASM** v2.51+autozp — the 6502 assembler, checked in under `BOOTROM*/bin/`.
- **VirtuaNES** — NSF audition, `mml/g.bat`.
- **arduino-pico** by Earle Philhower — <https://github.com/earlephilhower/arduino-pico>
  The Arduino core for RP2040/RP2350.

## Third-party code

@warning This project incorporates GPL v3 code. Check the licence position before
redistributing.

| Component | Files | Author | Licence |
|---|---|---|---|
| 6502 emulator (from FabGL) | `sys/rp_fcemu.h`, `rp_fcemu.cpp` | Fabrizio Di Vittorio | **GPL v3** |
| ArduinoGL | `sys/ArduinoGL.h`, `ArduinoGL.cpp` | Fabio de Albuquerque Dela Antonio | see file header |
| PIO scaffolding | `sys/pio/fcppu.pio` | Raspberry Pi (Trading) Ltd. | BSD-3-Clause |
| NSD.Lib driver | `mml/bin/nsd.bin`, `sound.nsf` | e.itikawa | see NSD.Lib site |
| Everything else | — | impact soft | © 2025 |

FabGL's header notes that a commercial licence is available from the author.

### What the GPL v3 component costs you

`rp_fcemu` is the NSF player's 6502 core, so it is not an optional extra: **any derivative
that keeps the music system inherits GPL v3.** Replacing that one file is what makes a
permissively-licensed derivative possible.

This is not theoretical. FC_PICO_GB (below) dropped `rp_fcemu` along with `ArduinoGL` and
`Obj3d` in a single commit and now ships under MIT, which is the worked precedent for how
much has to go.

## Other implementations

- **FC_PICO_GB** by axsann — <https://github.com/axsann/FC_PICO_GB>
  A Game Boy emulator on the same cartridge, built on
  [Peanut-GB](https://github.com/deltabeard/Peanut-GB) and forked from FC PICO v1.00.
  MIT-licensed.

  It is the second independent implementation against this hardware, which makes it the
  evidence behind the ABI boundary recorded in @ref protocol -- what a different author
  inherited unedited is what is genuinely fixed, and what they deleted is what was only
  ever sample code. Note that it is a **v1.00** fork and therefore speaks the older,
  wire-incompatible 16-byte mailbox protocol.

## Documentation toolchain

- **Doxygen** 1.18.0 — <https://www.doxygen.nl>
- **Graphviz** — <https://graphviz.org>, for the `\dot` diagrams
- `tools/doxygen/doxyfilter.py` — shadows the non-C dialects; see @ref conventions
- `tools/doxygen/check_code_unchanged.py` — verifies a documentation pass changed only
  comments
