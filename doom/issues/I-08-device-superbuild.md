<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-08 Device configuration of the superbuild

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | M |
| **Depends on** | I-06 |
| **Work plan task** | P0-T1 |

## Goal

Every remaining device-side issue is blocked on a configuration that can build for the
RP2350 at all. It cannot be verified in the development sandbox -- there is no ARM compiler
and the proxy blocks the download -- so this issue's deliverable is a CI job that proves it.

## Specification

`plan/08-build.md` in full: the superbuild sketch, the toolchain pins (pico-sdk
2.1.1, arm-none-eabi-gcc 13.2.Rel1, picotool 2.1.1) and the flash layout.

## Owns (create or modify only these)

`CMakeLists.txt` (the device branch only), `cmake/versions.cmake`, `cmake/boards/fcpico.h`, `ci/workflows/doom-device.yml`, `.github/workflows/doom-device.yml` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. In particular leave the `FCPICO_HOST_ONLY` branch of `CMakeLists.txt` working exactly as it is.

## Steps

1. Write `cmake/boards/fcpico.h` starting from pico-sdk's `pico2.h`: 4 MB flash, the FC PICO
   pin macros from `plan/01-constraints.md`, nothing else.
2. Replace the device branch's `FATAL_ERROR` with the real superbuild: SDK import, project,
   `pico_sdk_init()`, then the subdirectories. Keep `FCPICO_HOST_ONLY=ON` byte-compatible.
3. Add a trivial device target that proves the toolchain: a `fcpico_hello` that blinks GP25
   and prints the unique ID, so the job has something to build before I-09 and I-10 land.
4. Write the device CI job: fetch and cache pico-sdk 2.1.1, install arm-none-eabi-gcc
   13.2.Rel1, configure `MinSizeRel`, build, run `tools/flash_layout_check.py` on the ELF,
   and upload the UF2, map and size report.

## Acceptance

Every command must pass from the repository root.

```
# locally: the host lane must keep working
cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build build-host && ctest --test-dir build-host
# then: the doom-device workflow is green and its artifacts include a UF2 and a size report
```

## Traps

`plan/01-constraints.md` warns that the engine is size-sensitive and that gcc 10.x
miscompiles it. Pin the version in CI and print it in the job log.
