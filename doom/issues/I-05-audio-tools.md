<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-05 Music and effect conversion tools

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | L |
| **Depends on** | I-04 (soft) |
| **Work plan task** | P4-T3 |

## Goal

Every music and effect byte the firmware will ever play comes out of these converters, and
they are the part a human can improve later without touching code.

## Specification

`plan/06-audio.md`, "Offline pipeline" and "Existing arrangements to start from".
`tools/audio/apus.py` (writer, tested) and `dpcm.py` (encoder, tested) already exist.

## Owns (create or modify only these)

`tools/audio/vgm2apus.py`, `tools/audio/mus2apus.py`, `tools/audio/sfx2apu.py`, `tools/audio/wadextract.py`, `tests/tools/test_audio_convert.py` (new)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. `wadextract.py`: pull named lumps out of a WAD (`D_E1M1`, `DS*`) to files. Test against a
   synthetic WAD you build in the test, not a real one.
2. `vgm2apus.py`: VGM register log to `.apus`, merging same-frame writes, applying the
   retrigger rule from `apus.thin_register_writes`, and failing loudly with the frame number
   if an arrangement exceeds 12 pairs in a frame.
3. `mus2apus.py --auto`: MUS or MIDI to `.apus` with the plan's voice assignment (melody to
   pulse 1, second voice to pulse 2, bass folded into the triangle's range, GM drums to
   noise). Quality only has to be "recognisable"; determinism is mandatory.
4. `sfx2apu.py`: a Doom sound lump to a synthesised pulse or noise script, plus a
   hand-authorable text format so a human can override one.
5. Tests: round trips through `apus.read_apus`; the cap is enforced; two runs of every tool
   on the same input produce identical bytes.

## Acceptance

Every command must pass from the repository root.

```
python3 -m pytest doom/tests/tools -q
```

## Traps

FamiStudio's exact command-line option names are still unknown (its documentation was
unreachable). Do not invent them: drive the tools from files, and record the real option
names in `plan/06-audio.md` if you get a chance to run it.
