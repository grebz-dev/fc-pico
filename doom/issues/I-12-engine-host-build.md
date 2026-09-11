<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-12 Engine host build without SDL

| | |
|---|---|
| **Lane** | A/B -- try locally first |
| **Size** | L |
| **Depends on** | I-11 |
| **Work plan task** | P0-T3 |

## Goal

A host build of the whole engine is what makes deterministic demo runs, golden frames and
co-simulation possible. The multicore and alarm shims it needs already exist and are tested;
this connects them.

## Specification

`plan/08-build.md` ("host build") and `sim/host_shim/README.md`, which records
exactly what pico-sdk's host platform does and does not provide.

## Owns (create or modify only these)

In the submodule: `src/fcpico/host_main.c` and its CMake wiring. In this repository: `sim/host_shim/` (extend only if something is missing), the submodule pin.

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Configure the engine for `PICO_PLATFORM=host`, linking `host_shim`, and fix what fails
   to link. Grow the shim only where the SDK genuinely lacks something, and document each
   addition in its README table.
2. Write `host_main.c` with the command line from the plan: `--whx`, `--demo`, `--frames`,
   `--lockstep`, `--dump-8bit`, `--dump-stream`, `--pads`.
3. Prove determinism: two runs of the same demo produce identical dumps. Make this a test,
   not a claim.
4. `doom1.whx` lives in the submodule; reference it by path, never copy it.

## Acceptance

Every command must pass from the repository root.

```
cmake -S doom -B build-host-engine -G Ninja -DPICO_PLATFORM=host -DPICO_SDK_PATH=/home/user/pico-sdk
cmake --build build-host-engine
build-host-engine/.../fcpico_doom_host --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 600 --lockstep --dump-stream /tmp/o1
build-host-engine/.../fcpico_doom_host --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 600 --lockstep --dump-stream /tmp/o2
diff -r /tmp/o1 /tmp/o2
```

## Traps

`get_core_num()` returns the real core in the shim, but the SDK's host spin locks are
documented as single-threaded dummies. If the engine relies on a spin lock across the two
threads, say so in the shim README rather than papering over it.
