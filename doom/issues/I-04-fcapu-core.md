<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-04 APU register sequencer

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | L |
| **Depends on** | none |
| **Work plan task** | P4-T1 |

## Goal

Music and effects are the last subsystem with no host-testable core. The sequencer is pure
logic over a byte cursor, so it can be finished and proven without any hardware.

## Specification

`plan/06-audio.md` in full, especially "Decision D5", the arbitration order, the
15-pair cap (`APU_PAIRS_MAX_V2`) and the note-retrigger rule. The `.apus` reader already
exists in `tools/audio/apus.py` and is tested; this is the C side that plays those streams.

## Owns (create or modify only these)

`port/apu/fcapu.h`, `port/apu/fcapu.c`, `port/apu/CMakeLists.txt`, `tests/apu/*` (new)

## Must not touch

`tests/CMakeLists.txt` -- it discovers suite directories by glob, so a new suite is a
new directory and nothing else. `tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Implement `fcapu` per the API sketch in `plan/02-architecture.md`: init, music play/stop/
   pause/volume, sfx start/stop/playing, and `fcapu_pump()` which emits at most
   `APU_PAIRS_MAX_V2` pairs per frame through a caller-supplied write callback (do not call
   into `fcbus` directly -- take a function pointer, so the tests can capture the writes).
2. Implement the arbitration order from the plan: DPCM trigger, voice steals, effect scripts,
   music note-ons, music timbre; deferred writes go to a 64-entry queue with a drop counter.
3. Implement the retrigger rule for `$4003/$4007/$400B/$400F` exactly as the plan states it,
   and cite `tools/audio/apus.py`'s `thin_register_writes` docstring in a comment: the two
   must agree, because one produces what the other consumes.
4. Tests: never more than the cap in any frame; the terminator invariant is the caller's, not
   yours; a sound effect steals pulse 2 and restores it; pause/resume leaves no stuck note
   (`$4015` and the volume registers are checked); the deferred queue never laps; a stream
   with a loop point loops at the right frame.

## Acceptance

Every command must pass from the repository root.

```
cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
cmake -S doom -B build-asan -G Ninja -DFCPICO_HOST_ONLY=ON -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g"
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```

## Traps

No allocation and no floating point: this runs on core 1 alongside the bus interrupt.
Use `tests/ctest_lite.h` like the `fcbus` suites do.
