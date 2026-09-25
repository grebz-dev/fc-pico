<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-15 Mesen2 co-simulation skeleton

| | |
|---|---|
| **Lane** | B -- host emulator build, local and CI |
| **Size** | L |
| **Depends on** | [I-06](I-06-ci-host-lane.md); [I-12](I-12-engine-host-build.md) only for Doom scenarios, not S0 |
| **Work plan task** | [P0-T11](../plan/10-workplan.md#p0-t11-mesen2-co-simulation-skeleton) |

## Status (2026-09-24)

**partial**. Trace 6 calibrated the mapper: strict S0 now passes with the measured
66/64 selected-read cadence, stable count and a reviewed picture golden. Fixed Doom
frames pass D0 with the tutorial ROM and D1 with the v2 ROM; S1 passes self-reflash
in both directions. Dynamic Doom-frame S2 and later input/audio/save scenarios remain.
The host engine and v2 ROM are available. See [`../PROGRESS.md`](../PROGRESS.md) and
[`../sim/mesen2/DOOM-FRAME.md`](../sim/mesen2/DOOM-FRAME.md).

## Goal

Co-simulation runs the real 6502 boot ROM and NES PPU against the host cartridge model.
Start S0 with the tutorial ROM and a test pattern; the engine host build is not a prerequisite.
This checks protocol integration, not ARM instruction execution or physical PIO/DMA timing.
Build locally where dependencies are available and provide a reproducible CI runner.

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
doom/sim/mesen2/run_scenario.sh S0 --frames 180 --output /tmp/fcpico-s0
```

Strict S0 is the calibrated test-pattern gate. The dynamic S2 gate remains open.

## Traps

The Lua API names are now verified against a build, not just `LuaDocumentation.json`:
`emu.stop` ends a `--testRunner` run even though the command-line help says `emu.exit`.

Two traps found the hard way and fixed in the code that is here; do not reintroduce them.
`BaseMapper::DebugReadVram` bypasses `MapperReadVram` entirely, so a debugger peek reads
ordinary CHR storage -- but only as long as the mapper's predicate also excludes
`MemoryOperationType` values other than `PpuRenderingRead` and `Read`. And the adapter must
clear the host action log before every input byte, or a ROM-page or log payload byte that
happens to look like `FP_COM_INI` is replayed as a second init.
