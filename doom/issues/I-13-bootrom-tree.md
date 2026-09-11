<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-13 Doom boot ROM source tree and Linux build

| | |
|---|---|
| **Lane** | B -- verified in CI only |
| **Size** | M |
| **Depends on** | I-06 |
| **Work plan task** | P2-T1 |

## Goal

Every console-side change needs an assembler that runs in CI. `nesasm.exe` is a 32-bit
Windows binary and Wine cannot be installed in the development sandbox, so this has to be
proven in a CI job.

## Specification

`plan/07-bootrom.md` in full, especially the bank layout, the RAM map (which
addresses the fix bank dictates) and "Building on Linux/CI".

## Owns (create or modify only these)

`bootrom/src/*`, `bootrom/build.sh`, `bootrom/ci/tutorial_md5_gate.sh`, `ci/workflows/doom-bootrom.yml`, `.github/workflows/doom-bootrom.yml` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**. The permanent fix bank is sacred: copy `tutorial_project/BOOTROM/bootrom_fixr.bin`, never rebuild or edit it.

## Steps

1. Copy the tutorial's sources into `bootrom/src/`, trimmed as `plan/07-bootrom.md` lists,
   with the opcode definitions coming from `bootrom/gen/protocol.inc`.
2. Write `build.sh` around Wine plus the checked-in `nesasm.exe`.
3. Write the toolchain gate: re-assemble the *tutorial* ROM with a pinned `dbdate.h` and
   check `rom.NES` against MD5 `B6CD675342B6C8AD79E537E2C9860579`. If that passes, the
   toolchain is trustworthy; if it does not, nothing else about the ROM means anything.
4. Assemble twice and compare, byte for byte, so the build is reproducible; check the fix
   bank region with `tools/nes/check_fixbank.py`.

## Acceptance

Every command must pass from the repository root.

```
# then: the doom-bootrom workflow is green, and its log shows both
#   the tutorial MD5 gate passing and two identical builds of doom.nes
```

## Traps

The RAM map is not free: `BOOTROM_FIX/SysEqu.h` fixes `KEY_*` at `$82`-`$8C`,
`FLG_2000/2001` at `$B0`/`$B1`, `NMI_FLG` at `$B3` and the flash buffers at `$400`/`$500`.
`plan/07-bootrom.md` has the resulting map; verify it against both `SysEqu.h` files before
writing a line of assembly.
