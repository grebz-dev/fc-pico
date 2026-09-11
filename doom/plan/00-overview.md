# 00 -- Overview

## Goal

A UF2 for the FC PICO cartridge, plus a matching 6502 boot ROM that the cartridge installs on
the console through its existing self-reflash path, such that inserting the cartridge into a
Famicom (or an NES via a 60-to-72-pin adapter) boots into the shareware Doom episode
*Knee-Deep in the Dead* and it is **playable with the console's controller**, with sound from
the console's APU.

"Playable" means: 20 fps or better in typical E1M1 views, controls that feel deliberate rather
than laggy, readable menus, save and load, the attract-mode demos running correctly, and no
desynchronisation of the cartridge-to-console frame stream during an hour of play.

## What we are combining

| From | We take | We leave behind |
|------|---------|-----------------|
| **FC PICO** (`tutorial_project/tuto1_hw`, `BOOTROM`, `BOOTROM_FIX`) | The PIO bus programs (`fcppu.pio`), the DMA frame streaming, the mailbox protocol and its 6502 counterpart, the self-reflash mechanism, the frame-sync rule (`PPU_COUNT_VAL` +/-2), the hardware knowledge in `docs/pages/`. | The Arduino IDE unity build, `ArduinoGL`/`Obj3d`/`Canvas`, the GPL-v3 6502 emulator and NSD.Lib music path, the EEPROM save layer, the tutorial application. |
| **RP2040 Doom** (`rp2` branch, RP2350-capable) | The whole engine: WHX-compressed WAD in flash, the column-list renderer (`pd_render.cpp`), the zone allocator over all RAM, save games in flash, demo playback, the `chocolate-doom` host build for verification, `whd_gen`. | `scanvideo` VGA output, I2S audio, OPL2 music emulation, ADPCM effect mixing, USB keyboard, I2C networking. |

The seam between them is a **frame sink**: RP2040 Doom produces a 320x200 8-bit paletted frame
(it already assembles one per frame for its scanline generator); the FC PICO side turns that
into the PPU byte stream, an attribute table and a palette, and ships them to the console.

## Non-goals (for the first release)

- Ultimate Doom, Doom II, PWADs. Flash is 4 MB (inferred; see 01); only the shareware WHX fits
  alongside the firmware. The build must not preclude larger flash parts later.
- Network play. The cartridge has no second bus.
- Faithful OPL2 music. The console's APU has five channels; music is *arranged* for it.
- NES sprites. Everything is background pattern data, as in NES DOOM.
- Clone-console compatibility beyond what the stock FC PICO firmware achieves.
- PAL consoles. Should work in principle (see 11), untested.

## Milestones

| ID | Name | Demonstrates | Verified by |
|----|------|--------------|-------------|
| **M0** | Foundations | pico-sdk build of an `fcbus` test-pattern firmware; host build of the bus model; boot ROM assembles on Linux CI; PPU-bus model calibrated against a hardware trace; CI green. | CI + one hardware session (trace capture, test pattern on a real console). |
| **M1** | Grey Doom | DEMO1 plays on the console in greyscale (one sub-palette, no attribute updates), at >= 15 fps, using the **unmodified v1 protocol and tutorial boot ROM** where possible. | Mesen2 co-simulation golden frames + hardware session. |
| **M2** | Colour Doom | Protocol v2 boot ROM installed by self-reflash; per-block palettes; palette flashes; >= 20 fps. | NMI cycle budget test, co-sim goldens, hardware. |
| **M3** | Playable | Full controller scheme, menus, save/load, cheats, options. | Scripted input tests in co-sim; hardware playtest checklist. |
| **M4** | Audible | Music and effects on the APU, DPCM weapon sounds from the boot ROM bank. | APU write-budget tests; NSF/register-log playback in an emulator; hardware. |
| **M5** | Release | Overclock, adaptive palettes, HUD polish, docs, flashing guide, tagged UF2 + boot ROM. | Full CI matrix + soak test (1 h) on hardware. |

## Decision log

Decisions already taken by this plan. Each may be revisited, but revisiting one means updating
every document that depends on it (listed in brackets).

| ID | Decision | Rationale |
|----|----------|-----------|
| D1 | **Build root is `fc-pico/doom` on pico-sdk CMake; RP2040 Doom is a git submodule; the Arduino IDE is not used for the Doom firmware.** [08] | RP2040 Doom needs its own linker script, `MinSizeRel`, `malloc` wrapping into the zone, and a host build. None of that fits the Arduino unity build. The tutorial firmware keeps its Arduino build untouched. |
| D2 | **The bus layer is re-implemented as a plain-C library (`fcbus`) from the FC PICO sources and documentation, wire-compatible with the shipped v1 protocol, with a host backend.** [02, 03, 09] | The C++/Arduino `rp_system` cannot be linked into a C pico-sdk program cleanly, and the host backend is what makes co-simulation possible. `fcppu.pio` is reused verbatim. |
| D3 | **Video: drop every fifth column (320 -> 256), keep 200 lines, letterbox 20 lines top and bottom, 4x4 ordered dither, one of four fixed sub-palettes per 16x16 block, conversion on core 1.** [04] | Cheapest path to M1; every later refinement (box filter, adaptive palettes, custom HUD) is additive. |
| D4 | **Protocol v2 extends the v1 mailbox to a fixed 128-byte layout carrying the attribute table and BG palette every frame, and adds an explicit controller packet.** [03, 07] | Four VRAM pokes per frame cannot carry 64 attribute bytes; raw controller bytes alias seven opcodes. A fixed length keeps `PPU_COUNT_VAL` constant. |
| D5 | **Audio is an APU register sequencer written in C, fed by offline-converted music and effect scripts, plus DPCM samples stored in the boot ROM bank.** [06, 07] | No 6502 emulator on the cartridge (drops the GPL-v3 dependency the FC PICO docs flag), fully host-testable, deterministic write budget. |
| D6 | **The permanent fix bank (`$F000`-`$FFFF`) is never modified; the Doom boot ROM is a new erasable bank that honours the fixed entry points.** [07] | The fix bank is what performs the reflash. Changing it needs a kazzo programmer and forfeits field updates. |
| D7 | **Frame pacing: the console's vblank is the heartbeat; Doom ticks at 35 Hz from the RP2350 timer; a converted frame is published at most once per heartbeat and the previous stream is repeated when none is ready.** [02] | Exactly the FC PICO model; RP2040 Doom already decouples render rate from tic rate. |
| D8 | **Testing is layered: host unit tests, a calibrated PPU-bus model with golden frames, pioemu tests for the PIO programs, an optional full-chip simulator run, and Mesen2 co-simulation with a custom mapper as the pre-hardware gate.** [09] | Hardware sessions are scarce; every failure class must be catchable before one. |
| D9 | **Protocol constants live in exactly one header and are generated into the 6502 include and the Python tools.** [03, 12] | The FC PICO docs list three hand-synchronised copies with no check; Doom adds two more consumers. |
| D10 | **Initial clock 150 MHz; overclock is a Phase 5 task with its own validation.** [01, 10] | FC PICO GB runs this hardware at 276 MHz, RP2040 Doom expects 270 MHz, but the PIO bus timing must be re-validated after any change. |

## How to use this plan

An executor starts at [12-agent-playbook.md](12-agent-playbook.md), then works
[10-workplan.md](10-workplan.md) top to bottom. Every task names the documents that are its
specification. Documents 01-09 are reference material and should be corrected, not
contradicted, when execution reveals they are wrong; the playbook says how.
