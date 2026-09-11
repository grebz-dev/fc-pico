<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-11 Engine fork: superbuild guard and platform skeleton

| | |
|---|---|
| **Lane** | B -- partly local |
| **Size** | M |
| **Depends on** | I-08 (soft) |
| **Work plan task** | P0-T2 |

## Goal

The engine is a submodule with a documented plan and no code. Until it can be configured
from the superbuild, neither the host runs nor any video work can start.

## Specification

`rp2040-doom/FCPICO-PORT.md` (already written, in the submodule) and
`plan/08-build.md`.

## Owns (create or modify only these)

In the `doom/rp2040-doom` submodule: `CMakeLists.txt` (guards only), `src/CMakeLists.txt` (one new target), `src/fcpico/*` (new). In this repository: the submodule pin.

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. Do not reformat engine files; keep every diff minimal and behind `FCPICO_SUPERBUILD` or `#if FCPICO`.

## Steps

1. Add the `FCPICO_SUPERBUILD` guards described in `FCPICO-PORT.md` so the parent project
   initialises the SDK, and make `pico-extras` optional.
2. Create `src/fcpico/` with a `common_fcpico` library and stub `i_video_fcpico.c`,
   `i_input_fcpico.c`, `i_sound_fcpico.c` that satisfy the engine's interfaces and do
   nothing.
3. Add the `doom_tiny_fcpico` target with the flag set from `plan/08-build.md`.
4. Verify the original targets still configure: `doom_tiny` and `chocolate-doom` must be
   unaffected when `FCPICO_SUPERBUILD` is unset.
5. Commit and push the submodule branch first, then advance the pin here in a separate
   commit naming the engine commit.

## Acceptance

Every command must pass from the repository root.

```
# in the submodule, with the host platform (no ARM toolchain needed):
cmake -S doom/rp2040-doom -B /tmp/be -DPICO_SDK_PATH=/home/user/pico-sdk -DPICO_PLATFORM=host
# then: the device lane builds doom_tiny_fcpico
```

## Traps

Two repositories, one branch name. The submodule pin is a separate commit; a pin that
points at an unpushed engine commit breaks every later clone.
