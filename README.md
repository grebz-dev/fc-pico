# FC PICO

FC PICO is a Famicom/NES cartridge with a Raspberry Pi Pico 2-class microcontroller
(RP2350) inside it. The microcontroller renders each frame itself and feeds it to the
console's PPU, producing graphics the hardware cannot generate on its own: a 256x240
framebuffer, software 3D with dithered shading, and per-8x1-pixel colour selection.

The cartridge is produced by [impact soft](https://impactsoft.booth.pm/items/7736698).
This repository holds the sample project distributed with it: the RP2350 firmware, the
6502 boot ROM that runs on the console, and the toolchain that builds and flashes both.

## The one-paragraph version

The cartridge does not appear on the CPU bus. It replaces the **CHR-ROM on the PPU
bus**, and answers the PPU's pattern fetches with a pre-rendered byte stream. Because
the PPU fetches in the same fixed order every frame, position within the frame can be
tracked by counting read strobes alone -- so the PPU address lines are left
unconnected. A 64-byte mailbox rides on the tail of each frame's stream, carrying
commands and APU register writes back to the 6502, which replies one byte at a time by
writing to `$2007`. That is the entire link: one 8-bit bus, two strobes, a chip select,
and no acknowledgement in either direction.

This technique is inherited from NES DOOM; @ref nes_doom explains where it came from
and what FC PICO changes.

## Repository map

| Path | Contents |
|------|----------|
| `tutorial_project/tuto1_hw/` | RP2350 firmware (Arduino sketch). `sys/` is the platform layer, `ap_*` the sample application. |
| `tutorial_project/tuto1_hw/sys/pio/` | PIO programs implementing the cartridge bus. |
| `tutorial_project/BOOTROM/` | 6502 boot ROM, erasable bank (`$8000`-`$EFFF`). |
| `tutorial_project/BOOTROM_FIX/` | 6502 boot ROM, permanent bank (`$F000`-`$FFFF`). Owns the reset vectors and the flash routines. |
| `tutorial_project/mml/` | Music sources (NSD.Lib MML) and the compiler that turns them into an NSF. |
| `tutorial_project/rom/` | Cartridge flashing via `anago`. |
| `tools/doxygen/` | Documentation tooling. |

## Where to start

New to the project, in order:

1. @ref architecture -- how the two processors divide the work.
2. @ref nes_doom -- why the graphics trick works at all.
3. @ref protocol -- the wire format, in full.
4. @ref hardware -- pinout and electrical notes.

Then, by task:

- Changing what is drawn -> @ref graphics_page
- Changing the music -> @ref audio_page
- Building or flashing -> @ref build_pipeline, @ref flashing
- Updating the boot ROM on the console -> @ref boot_reflash
- Regenerating the checked-in image and sound data -> @ref generated_resources
- Setting up a toolchain -> @ref dev_setup
- Editing the sources -> @ref conventions

## Building

```
build_docs.bat          :: this documentation -> docs/html/index.html
```

Firmware and ROM builds are described in @ref build_pipeline. Note that the boot ROM
and the firmware are version-coupled through a build stamp -- rebuilding one without
the other makes the console try to reflash itself on the next boot.

## Credits and licensing

The project is © 2025 impact soft. It incorporates third-party code under its own
terms, including GPL v3 components. See @ref references for the full attribution list.
