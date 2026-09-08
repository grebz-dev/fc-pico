# FC PICO Doom

Doom -- the shareware `DOOM1.WAD`, via [RP2040 Doom](https://github.com/kilograham/rp2040-doom)
(itself a Chocolate Doom derivative) -- running on the RP2350 inside an
[FC PICO](../README.md) cartridge, displayed by, and played from, an unmodified Famicom or NES.

**Status: planning.** Nothing under `doom/` compiles yet. This directory holds a development
plan written so that an AI coding agent (Claude Opus 5 class) or a human engineer can execute it
task by task, with automated verification at every step and hardware verification at defined
checkpoints. The plan was produced by reading the FC PICO documentation on the
`docs/utf8-and-doxygen` branch, the `tuto1_hw` firmware and `BOOTROM` sources, and the RP2040
Doom sources on its `rp2` (RP2350-capable) branch.

## Read in this order

| # | Document | What it settles |
|---|----------|-----------------|
| 0 | [plan/00-overview.md](plan/00-overview.md) | Goals, non-goals, milestones, decision log |
| 1 | [plan/01-constraints.md](plan/01-constraints.md) | Hard numbers: RAM, flash, CPU, NMI cycles, mailbox bytes, empirical bus constants |
| 2 | [plan/02-architecture.md](plan/02-architecture.md) | How the two code bases fuse; one frame end to end; core assignment; memory map |
| 3 | [plan/03-protocol-v2.md](plan/03-protocol-v2.md) | Wire protocol extension for Doom (mailbox v2, attributes, palette, controller packet) |
| 4 | [plan/04-video.md](plan/04-video.md) | 320x200x256 colours to 256x200x(2 bits + block palette) |
| 5 | [plan/05-input.md](plan/05-input.md) | Eight buttons for a game that wants a keyboard |
| 6 | [plan/06-audio.md](plan/06-audio.md) | Music and effects on the console's own APU |
| 7 | [plan/07-bootrom.md](plan/07-bootrom.md) | The 6502 side: a new erasable bank, an NMI that fits in vblank |
| 8 | [plan/08-build.md](plan/08-build.md) | CMake superbuild, submodule, toolchain pins, flash layout |
| 9 | [plan/09-testing-ci.md](plan/09-testing-ci.md) | Test pyramid: host unit tests, PPU-bus model, pioemu, full-chip sim, Mesen2 co-simulation, hardware rig, GitHub Actions |
| 10 | [plan/10-workplan.md](plan/10-workplan.md) | Phases and tasks with acceptance criteria -- **the execution checklist** |
| 11 | [plan/11-risks.md](plan/11-risks.md) | Risk register, open questions, decisions that need a human |
| 12 | [plan/12-agent-playbook.md](plan/12-agent-playbook.md) | Operating rules for an AI agent executing this plan |

## Directory map (planned)

| Path | Contents |
|------|----------|
| `plan/` | The documents above. |
| `ci/workflows/` | GitHub Actions workflow templates; copied into `.github/workflows/` when Phase 0 lands. |
| `rp2040-doom/` | Git submodule: the `grebz-dev/rp2040-doom` fork, branch `claude/doom-fc-pico-nes-2bb1bx`. The Doom engine and its `fcpico` platform layer live there. |
| `fcbus/` | The cartridge bus library: `fcppu.pio`, DMA, frame streaming, mailbox, protocol. Plain C on pico-sdk, with a host backend. Ported from `tutorial_project/tuto1_hw/sys/`. |
| `port/` | Glue that is specific to this repository: CMake superbuild, board config, flash layout, resource packing, the firmware `main()`. |
| `bootrom/` | The Doom boot ROM: a new erasable bank derived from `tutorial_project/BOOTROM/`. The permanent fix bank is reused unchanged. |
| `tools/` | Host tools: PPU stream decoder, music/SFX converters, DPCM packer, flash layout checker, golden-image updater. |
| `sim/` | Simulation harnesses: PPU bus model, pioemu tests, Mesen2 mapper shim, full-chip simulator scripts. |
| `tests/` | Unit and golden tests (ctest + pytest). |

## Two repositories, one branch name

Work happens on branch `claude/doom-fc-pico-nes-2bb1bx` in **both** repositories:

- `grebz-dev/fc-pico` -- this repository: plan, bus library, boot ROM, build, tests, CI.
- `grebz-dev/rp2040-doom` -- the engine fork, consumed here as the submodule `doom/rp2040-doom`.
  Its own notes for the port are in `FCPICO-PORT.md` at the root of that repository.

Clone with `git clone --recurse-submodules`, or run `git submodule update --init doom/rp2040-doom`
after a plain clone. The engine's own `3rdparty/tinyusb` submodule is **not** needed for the
FC PICO target (no USB host); do not recurse into it unless building the original VGA targets.
