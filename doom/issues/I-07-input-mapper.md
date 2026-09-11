<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-07 Controller mapping as a host-testable module

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | M |
| **Depends on** | none |
| **Work plan task** | P1-T4 / P3-T1 |

## Goal

Eight buttons have to drive a game built for a keyboard, and the mapping is all edge cases:
tap versus hold, a strafe modifier, sticky presses across a slower tic rate. That is logic,
not integration, so it can be settled now and simply called from the engine later.

## Specification

`plan/05-input.md` in full.

## Owns (create or modify only these)

`port/input/fcinput.h`, `port/input/fcinput.c`, `port/input/fcinput_cheats.h`, `port/input/CMakeLists.txt`, `tests/input/*` (new)

## Must not touch

`tests/CMakeLists.txt` -- it discovers suite directories by glob, so a new suite is a
new directory and nothing else. `tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Implement the mapper against a tiny local key-code enum, not the engine's `doomkeys.h`:
   take the key codes as a configuration table so the engine side is a one-file adapter.
   Emit events into a caller-supplied ring, so tests read them back.
2. Implement, per the plan: direction and action mapping; B as use-on-tap and strafe-on-hold
   with the 8-frame window; Select as next-weapon on tap and automap on a 20-frame hold;
   always-run; the sticky press that survives a tic boundary; Select plus Start for pause.
3. Implement the cheat-sequence recogniser behind a compile-time switch, and make sure a
   matched sequence swallows the Select release so normal behaviour cannot fire too.
4. Tests: feed pad states frame by frame and assert the exact event stream, including order.
   One test per rule, named after the rule.

## Acceptance

Every command must pass from the repository root.

```
cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## Traps

Frames (60.1 Hz) and tics (35 Hz) are different clocks. The mapper is polled per tic but
latches per frame; a test that conflates them will pass while the game drops taps.
