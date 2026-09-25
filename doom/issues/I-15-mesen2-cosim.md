<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-15 Mesen2 co-simulation skeleton

| | |
|---|---|
| **Lane** | B -- host emulator build, local and CI |
| **Size** | L |
| **Depends on** | [I-06](I-06-ci-host-lane.md); [I-12](I-12-engine-host-build.md) only for Doom scenarios, not S0 |
| **Work plan task** | [P0-T11](../plan/10-workplan.md#p0-t11-mesen2-co-simulation-skeleton) |

## Goal

Co-simulation runs the real 6502 boot ROM and NES PPU against the host cartridge model.
Start S0 with the tutorial ROM and a test pattern; the engine host build is not a prerequisite.
This checks protocol integration, not ARM instruction execution or physical PIO/DMA timing.
Build locally where dependencies are available and provide a reproducible CI runner.

## Hardware-calibrated S0 result (2026-09-22)

Trace 6 resolves the earlier failed count gate. The mapper selects 66 pre-render and 64
visible-line reads; the host reports the same zero-based `ppu_count=15490` as the device.
Strict S0 now passes with 30/30 valid post-startup mailboxes, no post-startup DMA stops,
deterministic debugger-peek output and a reviewed test-pattern picture golden. The section
below records the earlier diagnostic state and the failed 68-read hypothesis. Doom-mode
scenarios still depend on the engine and v2 ROM work.

## Current execution (2026-09-20)

- Selected fork: `https://github.com/grebz-dev/MesenCE-FC-PICO`, pinned at
  `5c2de02e` under `sim/mesen2/Mesen2`; the mapper lives in the fork at
  `Core/NES/Mappers/Homebrew/FcPico.h` behind the optional `FCPICO_ROOT` makefile flag.
- Built and run locally. The toolchain is .NET SDK 10, SDL2 and a C++17 compiler; neither
  .NET nor SDL2 needs root (see [`../PROGRESS.md`](../PROGRESS.md) for the exact no-sudo recipe).
- **The bridge works.** The unmodified tutorial PRG boots, its `$2007` writes reach the real
  `fcbus_host` dispatcher, `FP_COM_INI` is answered through the tutorial's own
  `PF_COM_DMOD` -> `DRQ` -> `DLD` bulk transfer, and in steady state the run is exactly one
  heartbeat per frame. Two runs, one with per-frame debugger peeks across `$0000`-`$2FFF`
  and one without, produce byte-identical traces, mailboxes and screenshots: debug reads do
  not consume cartridge bytes, which was the main thing the mapper had to get right.
- **Strict S0 fails, and the reason is a plan defect, not a bug in this issue.** The measured
  per-frame selected read count is 16388 (`241 x 68`) under a `$0000`-`$0FFF` decode and
  20244 (`241 x 84`) under `$0000`-`$1FFF`, against `PPU_COUNT_VAL_V1`'s 15426 + 64. The
  arithmetic and what it implies are written up in [`plan/09-testing-ci.md` under "S0 calibrated to hardware"](../plan/09-testing-ci.md#s0-calibrated-to-hardware-2026-09-22).
  No protocol constant was changed and no read was discarded to make the check pass; the
  count check fails every heartbeat, the DMA stops, and the screenshot is white.
- Resolving the contradiction needs the hardware trace, which is issue [I-19](I-19-hw-trace.md). Everything this
  issue can prove about the bus without hardware is now proven. A focused PPU rendering trace
  also validates the emulator fetch timeline: 241 lines x 68 background reads for the narrow
  decode, and 16 additional sprite reads per line for the wide decode. The returned bytes
  remain open bus, so this does not validate displayed pixels. What remains is strict S0 and
  the Doom-mode adapter/scenarios S1-S6, which need [I-12](I-12-engine-host-build.md).

## Specification

[`plan/09-testing-ci.md` section "L6 -- Mesen2 co-simulation"](../plan/09-testing-ci.md#l6----mesen2-co-simulation-simmesen2). The mapper sketch there has been
corrected against the built fork, and "S0 as measured" records what the first working run
found.

## Owns (create or modify only these)

`sim/mesen2/*` (fork submodule, build script, scenario runner, Lua), `sim/cartmodel/*`,
`tests/cartmodel/*`, `tools/nes/set_mapper.py`, `.gitmodules`, the host integration in
`CMakeLists.txt`, `ci/workflows/doom-cosim.yml`, `.github/workflows/doom-cosim.yml`

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Pin the user's MesenCE fork as a submodule under `sim/mesen2/Mesen2`.
2. Write the mapper. Its first job is to correct the plan's sketch: find the real predicate
   that distinguishes a rendering or CPU read from a debugger peek, and fix
   [`plan/09-testing-ci.md`](../plan/09-testing-ci.md) in the same commit.
3. Write `sim/cartmodel/` as a C API over `fcbus_host` plus the test-pattern generator.
4. Scenario S0: the tutorial boot ROM against the test-pattern model, asserting a stable
   `ppu_count` histogram and a screenshot hash, run headless with `--testRunner`.
5. Cache the Mesen2 build on its commit hash; the run must stay under 20 minutes.

## Acceptance

Every command must pass from the repository root.

```
# toolchain on PATH: .NET SDK 10 and sdl2-config
doom/sim/mesen2/build.sh
doom/sim/mesen2/run_scenario.sh S0 --diagnostic   -> exit 0, deterministic
doom/sim/mesen2/run_scenario.sh S0                -> exit 1 until I-19 is answered
```

The diagnostic run is the bring-up checkpoint and is what `ci/workflows/doom-cosim.yml`
requires. Strict S0 is the real acceptance and is deliberately left failing; the lane is not
activated on a diagnostic alone.

## Traps

The Lua API names are now verified against a build, not just `LuaDocumentation.json`:
`emu.stop` ends a `--testRunner` run even though the command-line help says `emu.exit`.

Two traps found the hard way and fixed in the code that is here; do not reintroduce them.
`BaseMapper::DebugReadVram` bypasses `MapperReadVram` entirely, so a debugger peek reads
ordinary CHR storage -- but only as long as the mapper's predicate also excludes
`MemoryOperationType` values other than `PpuRenderingRead` and `Read`. And the adapter must
clear the host action log before every input byte, or a ROM-page or log payload byte that
happens to look like `FP_COM_INI` is replayed as a second init.
