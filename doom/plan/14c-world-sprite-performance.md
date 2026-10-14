# 14c -- World sprite performance plan

Date: 2026-10-04. Status: options 1..5 implemented or assessed; hardware
pending ([HR-17](../HARDWARE-REQUESTS.md)). Parent: [14](14-world-sprites.md).

Hardware report on the second HR-17 candidate (ROM 9031): better than the first,
but still lags noticeably with more than one pickup or any enemy on screen.

## Offline optimization follow-up (2026-10-13)

Hardware output is unavailable. Continued P3-W5 offline; physical acceptance
and the >=20 fps target remain open. The default keeps coherent sprite/background
publication. The loose variant remains an experiment requiring visual review.

The additional changes in `fcworld.c` preserve raster, palettes, tiles, OAM and
budget decisions: calculate texture columns once per actor; build NES bitplanes
directly from local texels; accumulate the four palette errors without a loop;
reuse palette tables for consecutive actors of one family. Fixed undefined
signed shifts for negative projection offsets exposed by UBSan.

New [Cortex-M33 benchmark](../tools/perf/README.md): Arm GCC 13.2.1, firmware
architecture flags and `-Os`, 72 atlas-based cases including multiple actors,
flips, scale changes and occlusion. Host pre-change and emulated post-change
output match exactly (37,050 bytes). Counts include called routines:

| Routine | Before this follow-up | After | Reduction |
|---|---:|---:|---:|
| Rasterize actors | 1,381,203 | 860,405 | 37.7% |
| Build palette/tile/OAM frames | 2,942,354 | 1,589,147 | 46.0% |
| Apply upload budget | 785,123 | 785,123 | 0% |
| Total | 5,108,680 | 3,234,675 | 36.7% |

The 3.4M instruction regression gate fails on the saved original source and
passes on the optimized source. This is CPU work, not a hardware FPS gain.
The existing packed background converter also passes its exact-output check:
977,251 instructions on demo 1 frame 100, about 7.9 ms under the old calibration.
Its harness now requires normal completion instead of accepting arbitrary
emulator faults.

Fresh Mesen core/UI builds, demos 1..3, frames 100..399, three game tics/picture:
**12.7 / 13.8 / 12.5 modeled published fps**, zero failures across 7,200 captured
PPU frames. Palette-only replay artifacts remain 4/3/3 as described below.
Demo 1 with the previous timing/drop policy is 9.3 fps. `--device-old` now also
defaults to three tics/picture, and JSON reports include timing assumptions.
Demo 1 streamed and loose controls both model 17.7 fps with zero failures;
loose sprites lag by 1.92 pictures on average, at most six, with 13 palette-only
replay artifacts (streamed has none).
The fixed render-time model cannot credit this follow-up's reduced sprite CPU
cost until hardware timings calibrate it. A 400-frame demo 1 recording remains
byte-identical to the pre-cache optimization recording (sprite and background
streams); the initial 200-frame comparison also matches the starting tree.

Verification: 15/15 CTests in Release and ASan/UBSan; 525 Python tests passed
(five skipped), and all 36 boot-ROM tests passed explicitly with NESASM.
The new partial-tile regression independently decodes all 256 combinations of
width/height 1..16, including transparent padding and flipped tile reuse.
LeakSanitizer is disabled because this sandbox cannot run its process inspection;
AddressSanitizer and fail-fast UndefinedBehaviorSanitizer remain enabled.
Protocol freshness and flash/fix-bank checks pass.

Default firmware: `.build/doom-world-perf/artifacts/fcpico_doom_native_world_whx.uf2`,
SHA-256 `90739cc30b9acf843f33f6c0c738083486fff814aac26362be1f792c7804d8c4`.
Firmware uses 315,572 bytes; post-zone heap remains 33,572 bytes. Actual compiler
stack reports: raster 384 bytes (was 128), frame build 672 (was 688), budget 600;
these are individual functions, excluding callers/callees and interrupt frames.
No persistent RAM was added. ROM remains `20DOOM-05-9031` with the unchanged fix bank.
Loose variant: `.build/doom-world-perf-loose/artifacts/fcpico_doom_native_world_whx.uf2`,
SHA-256 `a7838702a289e4d9356c0d5640f9496c64253e318f4d2cdacf4bc2569b190b42`;
post-zone heap 33,568 bytes. Both retain the startup delay, audio and fetch probe.

Machine-readable results: [offline performance evidence](world-sprite-perf-2026-10-13.json).
The local `.build/doom-world-perf/evidence/` also preserves source snapshots,
full reports and test logs. HR-17 remains a deferred hardware check, not a
requirement to stop offline work. CHR upload bandwidth and commit latency remain
the transport bottleneck; this change does not claim the >=20 fps acceptance gate.

## Distance-dependent stutter and flash follow-up (2026-10-13)

Hardware follow-up: after receiving the candidate below, the user reports
“Much better performance”. This confirms a qualitative improvement; no measured
FPS or explicit confirmation of flash/desync elimination was supplied.
[Hardware record](../HARDWARE-LOG.md#2026-10-13----hr-17-distance-stability-candidate-performance-improved).

User reports the worst stutter just before a pickup/enemy reaches the streamed
fallback size, with occasional full-frame flashes/desync. No frame/timing capture
was available, so the physical flash symptom has not been reproduced offline.

The submission path was missing from the earlier CPU benchmark. A worst-case
26-tile cold generation against 53 resident cache entries takes **168,809 M33
instructions while the old device glue masks interrupts**. At 150 MHz that is
at least 1.125 ms even at one cycle per instruction. A delayed bus heartbeat
cannot rearm background DMA on time; this is a strong candidate cause, not a
confirmed explanation of the physical flashes.

- Submission now briefly reserves the idle scheduler, then searches the cache
  and builds OAM with IRQs enabled. A `PREPARING` state prevents the heartbeat
  from observing partial data; release/acquire publication exposes the finished
  state. Both normal and console-reinitialization paths use this reservation.
  The reservation is 14 instructions in Unicorn, excluding the IRQ wrapper and
  stall check. Total submission work is still about 168K instructions.
- Held-commit state is established before publishing the active scheduler state.
  Removing the old IRQ mask alone would expose a premature-commit race on a
  fully cached generation. Function-boundary interrupt injection reproduces
  two premature commit packets with the old source; the fixed source passes
  43 assertions, including full 26-tile preparation and a lease lasting longer
  than the normal stall timeout without disturbing the bus mailbox.
- An actor enters native sprites at <=12 bounding tiles, stays native through
  16, and streams above 16 instead of waiting until the former 26-tile capacity
  cliff. Re-entry at <=12 avoids mode jitter near the cutoff. The global pool
  and aggregate 26-tile budget remain; nearer/larger actors use the established
  software-rendered fallback. The size/approach/recede regression passes.

Fresh recorded demos, frames 100..399, three tics/picture, zero failures across
7,200 PPU frames:

| Modeled published FPS | Demo 1 | Demo 2 | Demo 3 |
|---|---:|---:|---:|
| Before distance policy | 12.7 | 13.8 | 12.5 |
| With 12/16 actor-size policy | 13.4 | 14.5 | 13.0 |
| Gain using unrounded FPS | 6.1% | 5.0% | 3.7% |

Mean native tiles per used frame fall from 8.07/4.99/7.52 to 6.97/4.01/6.65.
Palette-only replay artifacts are 1/3/4 and remain reported separately.
The fixed-time cart model cannot represent missed ARM IRQ deadlines, so these
FPS gains measure the admission change, not the interrupt fix. Scene labels
count native admitted actors: an earlier fallback also changes these buckets,
so their per-scene averages are not a controlled before/after comparison.

Verification: Release and ASan/UBSan CTest 16/16; Python 525 passed / five skipped;
exact pre/post scheduler output for the 37,596-byte extended M33 workload;
instruction regression gate fails before/passes after; flash layout, unchanged
9031 ROM/fix bank, RAM and picotool checks. Post-zone heap is 33,548 bytes.

New default candidate:
`.build/doom-world-distance/artifacts/fcpico_doom_native_world_whx.uf2`, SHA-256
`eb6c439fe49dea577d2ae8d564ac6d626e73bb28ed7513f22d78c17aebf58f8c`.
It preserves coherent gating, audio, the diagnostic startup delay and the
fetch probe. [Machine-readable evidence](world-sprite-distance-2026-10-13.json)
and local `.build/doom-world-distance/evidence/` preserve reports and source
snapshots. Hardware flash/desync resolution remains open; test this candidate
around the previously troublesome distance before declaring that symptom fixed.

## How frame rate is measured

Every test of this work reports frames per second.

- **Hardware.** The firmware prints, every 60 published pictures:
  - `video fps= rendered= conversion_us=`: published pictures/s, engine frames/s
    (including drops) and conversion time. `rendered` well above `fps` means the
    transport holds pictures back; both low means engine or conversion time.
  - `world_early frames= loose_skipped= resolved_at_us=`: generations submitted
    at resolve, pictures published without a new generation (loose builds), and
    how far into the frame resolve happens (calibrates the cosimulation).
  - `world_wait count= avg_us= max_us= timeouts=` and `world_us` (engine cost).
  - `probe pa12_per_frame= cs1_pa12_per_frame=` (option 5 probe).
- **Cosimulation.** `run_world_runtime.py` replays a host recording through the
  real ROM, cart model and scheduler with a device engine clock and prints
  published/rendered fps overall and per scene (no actors, 1 pickup, 2+ pickups,
  enemies) from the host's per-frame actor census:
  - `--device`: current firmware estimate. Render 2.8 heartbeats (HR-15: 85 ms
    per streamed picture minus 38 ms conversion), conversion 0.6 heartbeats
    (8.8 ms, below), generation submitted at resolve 1.4 heartbeats into the
    render (assumed until `resolved_at_us` is measured), wait instead of drop,
    3 game tics per recorded frame (`--tics-per-frame`; lockstep recordings
    otherwise play the game at one tic per picture and understate change).
  - `--device-old`: previous firmware (38 ms conversion, drops).
  - `--loose`: loose gating (option 5); reports sprite lag in pictures.
  - `--baseline --scenes-from DIR`: everything streamed, same scene classes.

  ```sh
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --demo 1 --first 100 --count 300 --output /tmp/n1 --device
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --demo 1 --first 100 --count 300 --output /tmp/b1 --device --baseline \
    --scenes-from /tmp/n1
  ```

  `replay_palette_artifacts` counts screenshots where OAM and CHR match a
  generation but a sprite palette does not, right after a dropped or skipped
  generation: the lockstep recording planned palettes for sprites that were
  never shown. The device plans against the generation actually shown. Any
  other mismatch fails. Identical consecutive pictures (a still view whose
  actors are sprites) are disambiguated by the generation on screen.
- **Converter cost.** `doom/tools/perf/m33_convert_cost.py --playpal P --frame F`
  counts Cortex-M33 instructions for one conversion in Unicorn and checks the
  picture against the host; calibrated to the old converter's 38.2 ms.

## Results (cosimulation, demos 1..3, recorded frames 100..399, 3 tics/frame)

Published fps, 0 failures in every run:

| Configuration | Demo 1 | Demo 2 | Demo 3 |
|---|---:|---:|---:|
| Streamed, previous firmware | 11.8 | 11.8 | 11.8 |
| Streamed, new converter | 17.7 | 17.7 | 17.7 |
| Native, previous pipeline (`--device-old`, no budget) | 9.3 | 9.4 | 8.9 |
| Native, new firmware, no tile budget | 12.4 | 13.6 | 12.4 |
| Native, new firmware, budget 8 (default) | 12.7 | 13.8 | 12.5 |
| Native, new firmware, budget 4 | 12.7 | 13.9 | 12.7 |
| Native, loose gating (budget 8) | 17.7 | 17.7 | 17.7 |

Per scene, native previous / new (budget 8):

| Demo | No actors | 1 pickup | 2+ pickups | Enemies |
|---|---|---|---|---|
| 1 | 11.0 / 15.9 | 9.9 / 15.0 | 9.4 / 13.0 | 8.7 / 11.6 |
| 2 | 11.0 / 17.5 | 9.0 / 13.5 | 9.2 / 12.7 | 9.4 / 13.7 |
| 3 | 10.3 / 15.8 | - | 8.9 / 12.6 | 8.8 / 12.1 |

Loose gating: sprites trail their picture by 1.9 / 1.1 / 1.4 pictures on
average and at most 6 (about 0.3 s) in the busiest moments.

Small buckets (under 30 pictures) are noisy. The model does not include the
device world-sprite engine cost (`world_us`) or render-time differences between
scenes; hardware lines calibrate both.

## Where the time goes now

Gated, per picture on demo 1 (lockstep recording, before option 3): CHR 2.7
heartbeats, OAM 1.5, wait for shown 0.9, idle 0.5, against a 3.4-heartbeat
engine cycle. CHR uploads (7..8 new tiles per generation, 4 per heartbeat) are
the remaining cost; walk animation produces new art for most pictures. The
commit-to-shown latency is 2 heartbeats by protocol (commit packet, DMA frame,
shown report).

## Options

### 1. Wait instead of drop -- implemented

`fcvideo_frame_end` polls the bus, the BOOTSEL/serial line and audio for up to
50 ms while a world generation holds the back buffer, then drops as before.
Lockstep cosimulation: 8.5 / 9.6 / 8.6 to 9.9 / 11.1 / 10.3 fps. Check audio
(`[audio]` drops) on hardware.

### 2. Faster conversion -- 2a implemented, 2b deferred

- 2a (done): `fcvideo_init` derives packed tables: per palette and Bayer row, a
  byte whose bits give both bit-planes for the four x phases, so four pixels
  combine with masks only; error tables paired in 16-bit lanes. Cost sums run
  row-major per 16-pixel block; the pixel loop has no divides. Output is
  byte-identical (C/Python oracle tests, 40-frame host checksum, Unicorn
  picture check). Cortex-M33: 4.73M to 1.08M instructions, 38.2 to about
  8.8 ms. The 16 KB dither LUT is now built in the converter's unused frame
  buffer at init: RAM post-zone 26,332 to 33,572 bytes.
- 2b (deferred): split conversion between cores. About 4 ms more at best;
  core 1 belongs to the PD renderer. Revisit after hardware timings.

### 3. Fewer tile uploads -- implemented

- Scale-step hysteresis (`fcworld_scale_step`): an actor keeps its scale step
  until its scale is a full step away (identity = `mobj` pointer).
- Per-generation new-tile budget (`fcworld_frame_limit`, default
  `FCWORLD_TILE_BUDGET` 8): over budget, an actor that was unoccluded in both
  frames, kept its palette slots and was not stale last time reuses its shown
  tiles at its new patch origin for one generation. Line limits, pool size and
  screen bounds are rechecked; otherwise it uploads.
- Effect: +2..4% fps (table). Larger pools need the menu/logo tiles (would
  require restoring them from PRG when menus open); not done.

### 4. Cheaper generations -- assessed; RP-side part implemented

- Implemented: submit the generation at resolve (`fcpico_world_resolved`),
  before visplanes and walls are drawn, when the scheduler is idle. Measured
  gain is negligible: at resolve the previous generation is usually still
  committing, so this rarely fires. It remains as a no-cost path; hardware
  reports how often (`world_early frames=`).
- Not implemented, with reasons: five tiles per CHR packet and a same-NMI
  commit+DMA both exceed the 2,200-cycle NMI total (tables path is at 2,198);
  pipelining two generations needs a split pool and a third stream buffer
  (17 KB; RAM post-zone now has about 9 KB headroom above the 24 KB minimum,
  not enough).

### 5. Architectural -- loose gating built as a variant; fetch probe built

- Loose gating (`FCPICO_WORLD_LOOSE`, default OFF; separate UF2): pictures
  publish without waiting; a generation is submitted only when the scheduler
  is idle, otherwise sprites keep the last one. Cosimulation: 17.7 fps, the
  streamed ceiling, with sprites lagging their picture (mean 1..2 pictures, max
  6). Admitted actors are not in the background, so a lagging sprite can sit
  slightly off its occlusion edges or miss one picture when newly admitted.
  Needs visual review on hardware.
- Sprite-fetch substitution probe (`FCPICO_SPRITE_FETCH_PROBE`, default ON):
  two passive PIO1 counters read PA12 (GP18) and CS1 (GP17) on every PPU read;
  no pin is driven. Expected with 8x8 sprites at `$1000`: about 3,856 A12-high
  reads per frame (16 sprite pattern fetches on each of 241 lines). If
  `cs1_pa12_per_frame` is 0, sprite fetches never select the cartridge: serving
  sprite patterns from the RP would need a board change (CS1 decode / CHR RAM
  output enable), and this option stops. If it is nonzero, the board already
  routes them to the RP and a fetch-timing probe is the next step.

## Measurement requests for hardware

Flash each candidate in HR-17 and capture 60 s each with no actors, with 2+
pickups and with enemies: the `video`, `world_early`, `world_wait`, `world_us`
and `probe` lines. Compare the default and loose builds side by side for sprite
lag and smoothness.
