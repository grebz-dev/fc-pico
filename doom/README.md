# FC PICO Doom

Doom -- the shareware `DOOM1.WAD`, via [RP2040 Doom](https://github.com/kilograham/rp2040-doom)
(itself a Chocolate Doom derivative) -- running on the RP2350 inside an
[FC PICO](../README.md) cartridge, displayed by, and played from, an unmodified Famicom or NES.

**Status (2026-09-24): Doom video and basic controller input work on an NES-001.**
The v2 boot ROM runs on the console, the RP2350 converts and streams engine frames, and
the console displays Doom. The reflash path passes S1 co-simulation; a ROM UPDATE was
observed in an earlier hardware session, while the final `0002` stamp was not separately
transcribed. Movement, strafing and menus work with a physical pad. A short B
press still fails to activate **use** in gameplay; the input diagnosis is in
[PROGRESS.md](PROGRESS.md). Conversion takes about 35.6 ms per frame on the tested
firmware, above the [8 ms target](plan/04-video.md#acceptance-criteria-used-by-10-workplan).
The current 200-line image leaves 16 blank NES lines above and 24 below, as specified in
[the video plan](plan/04-video.md#stage-b----horizontal-decimation-320---256).

Host tests, strict test-pattern S0, fixed Doom-frame D0/D1 and reflash S1 co-simulation
pass. Dynamic Doom-frame S2, full controller behavior, performance, saves and audio still
need their own acceptance checks. This is a working hardware prototype, not a completed
release milestone; see [issues/README.md](issues/README.md) for current work and
[HARDWARE-LOG.md](HARDWARE-LOG.md) for observed console results.

Use [PROGRESS.md](PROGRESS.md) for dated verification evidence and environment notes.
[HARDWARE-REQUESTS.md](HARDWARE-REQUESTS.md) tracks requested console checks and
[HARDWARE-LOG.md](HARDWARE-LOG.md) records their results. The plans describe intended
behavior; an observed hardware result takes precedence over an old estimate.

The plan was produced by reading the FC PICO documentation on the
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

## Directory map

| Path | Contents |
|------|----------|
| `plan/` | The documents above. |
| `ci/workflows/` | GitHub Actions workflow templates; activated workflows live in `.github/workflows/`. |
| `rp2040-doom/` | Git submodule: the `grebz-dev/rp2040-doom` fork, branch `claude/doom-fc-pico-nes-2bb1bx`. The Doom engine and its `fcpico` platform layer live there. |
| `fcbus/` | The cartridge bus library: `fcppu.pio`, DMA, frame streaming, mailbox, protocol. Plain C on pico-sdk, with a host backend. Ported from `tutorial_project/tuto1_hw/sys/`. |
| `port/` | Glue that is specific to this repository: CMake superbuild, board config, flash layout, resource packing, the firmware `main()`. |
| `bootrom/` | The Doom boot ROM: a new erasable bank derived from `tutorial_project/BOOTROM/`. The permanent fix bank is reused unchanged. |
| `tools/` | Host tools: PPU stream decoder, music/SFX converters, DPCM packer, flash layout checker, golden-image updater. |
| `sim/` | Simulation harnesses: PPU bus model, pioemu tests, MesenCE mapper and scenarios; full-chip simulation remains optional. |
| `tests/` | Unit and golden tests (ctest + pytest). |

## Two repositories, one branch name

Work happens on branch `claude/doom-fc-pico-nes-2bb1bx` in **both** repositories:

- `grebz-dev/fc-pico` -- this repository: plan, bus library, boot ROM, build, tests, CI.
- `grebz-dev/rp2040-doom` -- the engine fork, consumed here as the submodule `doom/rp2040-doom`.
  Its own notes for the port are in [`FCPICO-PORT.md`](rp2040-doom/FCPICO-PORT.md) at the root of that repository.

Clone with `git clone --recurse-submodules`, or run `git submodule update --init doom/rp2040-doom`
after a plain clone. The engine's own `3rdparty/tinyusb` submodule is **not** needed for the
FC PICO target (no USB host); do not recurse into it unless building the original VGA targets.
The MesenCE co-simulation checkout is a separate submodule at `doom/sim/mesen2/Mesen2`.

## Keeping issues and plans current

Treat [issues/README.md](issues/README.md) as the issue index and
[plan/10-workplan.md](plan/10-workplan.md) as the task checklist. For each change:

1. Update the affected `issues/I-*.md` **Status** section and the corresponding index row.
   Use **open**, **partial**, or **done** consistently. Mark **done** only when that issue's
   acceptance checks have passed; name the remaining checks when it is partial.
2. Update the matching task row in `plan/10-workplan.md`. Correct any affected plan
   specification or decision when implementation or hardware evidence changes it. Keep
   proposed targets distinct from measured results and add a dated plan changelog entry
   for a substantive correction.
3. Put commands, results, measurements, and limits in [PROGRESS.md](PROGRESS.md). Record
   physical observations in [HARDWARE-LOG.md](HARDWARE-LOG.md) and close or revise the
   matching [hardware request](HARDWARE-REQUESTS.md). Link to that evidence from status
   summaries instead of copying long serial logs into several files.
4. Check that the README summary, issue index, issue status, work-plan table, and milestone
   gates agree. Run `python3 doom/tools/check_md_links.py doom` and `git diff --check` before
   committing documentation changes.
