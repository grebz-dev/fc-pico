<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-15 Mesen2 co-simulation skeleton

| | |
|---|---|
| **Lane** | B -- host emulator build, local and CI |
| **Size** | L |
| **Depends on** | I-06; I-12 only for Doom scenarios, not S0 |
| **Work plan task** | P0-T11 |

## Goal

Co-simulation runs the real 6502 boot ROM and NES PPU against the host cartridge model.
Start S0 with the tutorial ROM and a test pattern; the engine host build is not a prerequisite.
This checks protocol integration, not ARM instruction execution or physical PIO/DMA timing.
Build locally where dependencies are available and provide a reproducible CI runner.

## Current execution (2026-09-20)

- Selected fork: `https://github.com/grebz-dev/MesenCE-FC-PICO`, initially pinned at
  `20f497c9620a7f4e55b7752e1d22734f9d7492fa` under `sim/mesen2/Mesen2`.
- The fork requires .NET 10, SDL2 and a C++17 compiler; the old .NET 8 assumption is stale.
- Parent owns the fork mapper, runner/build scripts, integration, plans and progress.
  Independent tasks own `sim/cartmodel/` + `tests/cartmodel/`, device build validation,
  and local emulator tool installation respectively. Agents report verification to parent
  before commits; shared status documents have one writer.
- First checkpoint: compile the mapper, boot the unmodified tutorial PRG, record actual
  selected PPU reads and mailbox traffic. Keep CS1 decode assumptions explicit.
- Then run strict S0 (stable visible pattern and count). If the documented count/layout
  contradiction prevents it, retain a failing strict S0 and a separate diagnostic run;
  do not alter protocol constants or discard reads merely to make S0 green.
- Resume from `../PROGRESS.md` for commands, commits and the last observed failure.

## Specification

`plan/09-testing-ci.md`, "L6 -- Mesen2 co-simulation", including the mapper sketch
and its warning that the method names are unverified.

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
   `plan/09-testing-ci.md` in the same commit.
3. Write `sim/cartmodel/` as a C API over `fcbus_host` plus the test-pattern generator.
4. Scenario S0: the tutorial boot ROM against the test-pattern model, asserting a stable
   `ppu_count` histogram and a screenshot hash, run headless with `--testRunner`.
5. Cache the Mesen2 build on its commit hash; the run must stay under 20 minutes.

## Acceptance

Every command must pass from the repository root.

```
# then: the doom-cosim workflow runs S0 headless and green
```

## Traps

Verify the Lua API names against the pinned checkout's `LuaDocumentation.json`. The names
in `plan/09-testing-ci.md` were read from documentation, not from a build.
