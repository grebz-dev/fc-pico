<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-15 Mesen2 co-simulation skeleton

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | L |
| **Depends on** | I-06, I-12 |
| **Work plan task** | P0-T11 |

## Goal

Co-simulation runs the real 6502 boot ROM against the real firmware model, which is the only
way to catch a protocol or timing mistake before a hardware session. .NET cannot be installed
in the development sandbox, so this is a CI deliverable.

## Specification

`plan/09-testing-ci.md`, "L6 -- Mesen2 co-simulation", including the mapper sketch
and its warning that the method names are unverified.

## Owns (create or modify only these)

`sim/mesen2/*` (mapper, build script, scenario runner, Lua), `sim/cartmodel/*`, `ci/workflows/doom-cosim.yml`, `.github/workflows/doom-cosim.yml`

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Pin a Mesen2 commit as a submodule under `sim/mesen2/`.
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
