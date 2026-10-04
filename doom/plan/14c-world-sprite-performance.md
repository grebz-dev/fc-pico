# 14c -- World sprite performance plan

Date: 2026-10-04. Status: plan; measurements below, no option beyond the
measurement tooling implemented yet. Parent: [14](14-world-sprites.md).

Hardware report on the second HR-17 candidate (ROM 9031): better than the first,
but still lags noticeably with more than one pickup or any enemy on screen.

## How frame rate is measured

Every test of this work reports frames per second.

- **Hardware.** The firmware's `video` serial line (every 60 published pictures)
  reports `fps=` (published pictures/s) and `rendered=` (engine frames/s,
  including frames dropped because both stream buffers were busy), next to
  `conversion_us`. `rendered` well above `fps` means the transport is holding
  pictures back; both low means engine or conversion time.
- **Cosimulation.** `run_world_runtime.py --device` replays a host recording
  through the real ROM, cart model and scheduler with a device engine clock:
  2.8 heartbeats of render, then 2.3 heartbeats (38 ms) of blocking conversion,
  and a finished frame is dropped if both buffers are still busy. The 2.8 comes
  from HR-15 serial (480 pictures in about 2,443 heartbeats, no drops: 11.8 fps,
  85 ms per picture, minus 38 ms conversion). It prints published and rendered
  fps overall and per scene (no actors, 1 pickup, 2+ pickups, enemies) from the
  host's per-frame actor census (`frameN.actors`). `--baseline --scenes-from DIR`
  gives the streamed comparison on the same scenes.

  ```sh
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --demo 1 --first 100 --count 300 --output /tmp/fps-n1 --device
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --demo 1 --first 100 --count 300 --output /tmp/fps-b1 --device --baseline \
    --scenes-from /tmp/fps-n1
  ```

  Replays with dropped frames report `replay_palette_artifacts`. These are
  frames where only a sprite palette differs, right after a drop: the
  lockstep host recording planned palettes against a picture that was skipped.
  The device plans against the generation actually shown, so they are not
  device faults. OAM, CHR or non-adjacent palette mismatches still fail.

## Current numbers (cosimulation, device timing, 300 frames per demo)

| Demo | Streamed | Native (current: drop) | Native, wait instead of drop |
|---|---:|---:|---:|
| 1 | 11.8 | 8.5 | 10.1 |
| 2 | 11.8 | 9.6 | 11.1 |
| 3 | 11.8 | 8.6 | 10.3 |

Per scene, native current / wait (streamed is 11.7..12.0 in every scene):

| Demo | No actors | 1 pickup | 2+ pickups | Enemies |
|---|---|---|---|---|
| 1 | 10.0 / 11.1 | 8.6 / 10.0 | 9.8 / 10.9 | 8.2 / 9.9 |
| 2 | 11.3 / 11.5 | 10.8 / 11.7 | 8.9 / 10.3 | 9.5 / 11.1 |
| 3 | 9.5 / 12.0 | 10.0 / 11.7 | 12.0 / 11.7 | 8.2 / 10.1 |

Small buckets (under 30 pictures) are noisy. All runs: 0 failures. The
model does not yet include device world-sprite engine cost (`world_us`).

## Where the time goes

1. **The engine itself caps streaming at about 11.8 fps.** Render is about
   47 ms and conversion 38 ms, and they run one after the other. Native
   sprites cannot beat this ceiling, and it is what players feel even
   without actors.
2. **Dropped frames multiply transport delay.** After a picture is published,
   its generation still needs the commit, the DMA and the shown report (plus
   any CHR left over). If that takes longer than the next 47 ms render, the
   finished frame is thrown away and a whole new render starts. A short delay
   becomes a lost render: native replays render 13.6..14.5 fps but publish
   8.5..9.6.
3. **CHR bandwidth.** Enemies need about 8 new tiles per generation (4 per
   heartbeat), so their transport often exceeds one render period.
4. **Fixed cost per changing generation.** Even static pickups need OAM, a
   commit, a DMA frame and the shown report whenever the view moves (the
   positions change), about 3 heartbeats. Streamed pictures need about 1.

## Options, in recommended order

Each step reports cosimulation fps per scene (as above) and, when flashed,
the hardware `video fps=/rendered=` lines in the same scenes.

### 1. Wait instead of drop (RP firmware only) -- next

When a frame finishes while the back buffer is held by a generation in OAM or
wait-shown stage, poll the bus (and audio servicing) until the buffer frees,
bounded at about 3 heartbeats, then convert. Cosimulation: +16..20% overall
(table above), most of the native-vs-streamed gap. Risks: anything core 0
must service during the wait (USB serial, BOOTSEL poll, audio deadlines);
measure `[audio]` drops and IRQ latency on hardware. Acceptance:
cosimulation within 15% of streamed in every scene; no audio regressions.

### 2. Faster conversion (helps every scene, raises the ceiling)

`fcvideo_convert_staged` takes 38 ms for about 59k output pixels, about 96 CPU
cycles per pixel. It divides per pixel (`flat % width`, `/ width`), branches per
bit, and looks up the attribute per pixel; `choose_attributes` sums four error
tables per pixel. Plan:

- a. Restructure the hot loop per 8-pixel tile row: precomputed attribute per
  16-pixel span, LUT row pointer per palette, bit packing without divides; the
  same for the cost sums (RAM-resident, word reads). Target 8..12 ms, the
  original plan-04 goal; the host CTest/Python oracle keeps exactness.
- b. Split conversion rows between core 0 and core 1. Core 1 runs the PD
  render helper and is idle during conversion; measure first.

Expected: 38 ms down to about 10 ms gives an 11.8 to about 17.5 fps streamed
ceiling. Native gains as long as the transport keeps up, which makes options
3 and 4 matter more afterwards.

### 3. Fewer tile uploads per generation (RP only)

- Scale-step hysteresis: change an actor's step only after it moves more than
  one step or holds the new one for 2 frames (fewer re-rasterisations while
  approaching).
- A per-generation new-tile budget (for example 8): an actor whose new tiles
  do not fit keeps its previous tiles at the new position for one more
  generation (stale animation frame, correct place). A longer overflow streams
  the actor.
- Larger pool: repack the 53-tile pool if eviction misses show up (add a
  pool-miss counter to the cart model stats first).
- Measure: tiles uploaded and cache hits per generation, fps per scene.

### 4. Cheaper generations on the console (ROM)

- Present OAM-only generations faster: when no tiles change, send the
  committing OAM packet in the same heartbeat as the picture swap, so the commit
  DMA overlaps the picture's first frame. Requires reworking the gate order.
- Five tiles per CHR packet: not possible within the 2,200-cycle NMI total
  today (tables path is 2,198); only with a cycle cut elsewhere.
- Pipeline two generations (upload N+1 while N waits for shown) needs a
  split tile pool and a third stream buffer (17 KB) or re-ordered gating;
  RAM headroom is about 1.8 KB, so this depends on option 2 freeing buffers.

### 5. Architectural (hardware-gated)

- Sprite pattern fetch substitution: serve sprite pattern bytes at
  `$1000-$1FFF` from the RP like the BG stream, removing CHR uploads
  entirely. Requires proof that FC PICO hardware sees and can answer those
  fetches in time (plan 14 mapper section). Probe on hardware before any design.
- Loose gating: publish a picture without waiting when its generation differs
  from the shown one only by small OAM moves (sprites lag one picture). This
  trades exact sprite/background coherence for fps; needs user review.

## Measurement requests for hardware

With the firmware that prints `video fps=... rendered=...`: 60 s standing in
a room with no actors, with 2+ pickups, and with enemies, plus the
`world_us` lines. These calibrate the cosimulation's render time and add the
world-sprite engine cost, which it does not model yet.
