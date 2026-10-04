# Progress log

One entry per task from [`plan/10-workplan.md`](plan/10-workplan.md), newest first. Format:

```

## P3-W5b -- frame-rate reporting and performance plan (2026-10-04)

- Hardware (HR-17 second candidate `b55cade4...`): better, still lags noticeably
  with 2+ pickups or an enemy on screen. User asked for fps in every test and a
  plan: [14c](plan/14c-world-sprite-performance.md).
- Device: `video` serial line gains `fps=` (published pictures/s) and
  `rendered=` (engine frames/s including drops). Candidate
  `.build/doom-world-test/artifacts/fcpico_doom_native_world_whx.uf2` SHA-256
  `9d7caa2e4ede0d3dc4ce5d690a4e34d3cef11871bb1cfa62992095cbdfa41a80` (ROM 9031
  unchanged), RAM post-zone 26,352 bytes.
- Host: `--world-dump` writes `frameN.actors` (admitted monsters, pickups,
  others, streamed). Cart model: device engine clock
  (`FCPICO_WORLD_RENDER_TENTHS`, `FCPICO_WORLD_CONVERT_TENTHS`, drops when both
  buffers are busy, `FCPICO_WORLD_WAIT`). `run_world_runtime.py --device`
  (render 2.8 + conversion 2.3 heartbeats, calibrated to HR-15's 11.8 fps)
  prints published/rendered fps overall and per scene; `--scenes-from` for
  baselines; palette-only mismatches right after a dropped frame are counted as
  `replay_palette_artifacts` (lockstep recording planned for a skipped picture).
- Cosimulation, demos 1..3, recorded frames 100..399, device timing, 0 failures:
  streamed 11.8/11.8/11.8 fps; native (current drop policy) 8.5/9.6/8.6;
  native with wait-instead-of-drop 10.1/11.1/10.3. Per-scene tables in 14c.
- Verified: host CTest 15/15, Python 525 passed, links OK.

## P3-W5a -- shared HUD palette and world sprite transport (2026-10-04)

- Request: free the BG palette the 9016 native HUD reserves, validate, then
  improve frame rate with native sprites on screen.
- HUD palette: new preset `FCVIDEO_PRESET_SHARED_HUD` (default) is shadow detail
  with its dark-red subpalette 2 replaced by the HUD's `00 30 16` in HUD index
  order: `1D 00 30 / 08 18 30 / 00 30 16 / 09 19 30`. HUD cells use
  `FCVIDEO_HUD_PALETTE` 2; world cells choose among all four palettes again, so
  the green palette returns while the HUD is shown. The device/host override of
  palette 3 is gone, so the HUD tints with the world during flashes (as Doom's
  status bar does); the episode menu overlay uses palette 2. Chosen by the
  converter's own block error over 300 demo 1..3 frames (shadow lift on):
  current 6.52, shared HUD + green 6.51, shared HUD + red 6.48, full four
  without HUD 6.46. Warm ramps (`06 16 36`, `07 17 37`) scored 5.56..5.59 but
  dithered brown walls into olive in previews, so they were rejected.
- Sprite tiles (`fcworld_bounds`/`fcworld_raster_actor`): rasters are sampled
  in native pixels relative to the patch origin (left/top offsets, rounded to a
  native pixel) at a scale snapped to `FCWORLD_SCALE_STEPS` (8) geometric steps
  per octave, so tiles depend only on patch, step and flip. Sprites now end at
  line 191 while the status bar is shown: a tile overhang reached the HUD face
  sprites (9 sprites on line 201 in Mesen) after the layout change.
- ROM `20DOOM-05-9031`: a new world packet outranks a pending OAM DMA, so HUD
  and commit DMAs run in the heartbeat where the RP waits for the shown
  generation instead of costing a resend. py65 NMI budgets (critical/total):
  CHR 1699/2115, OAM 1209/2103, commit 1276/2183, DMA 1208+513/2137, tables
  1782/2198 (limit 2200).
- RP pipeline: `fcworld_submit_held` starts CHR and OAM staging before the 38 ms
  conversion; the committing packet waits for `fcworld_release` at publication.
- Mesen cosimulation, demo 1 recorded frames 100..249, 0 failures every run,
  1200 stable heartbeats, NMI exits 251..257:

  | Configuration | heartbeats/picture | tiles uploaded | cache hits | resends |
  |---|---:|---:|---:|---:|
  | 9030, exact Doom sampling (previous) | 6.89 | 1576 | 92 | 131 |
  | anchored, scale-stepped raster | 5.80 | 991 | 672 | 131 |
  | + ROM 9031 packet priority | 4.99 | 991 | 672 | 0 |
  | 2-heartbeat conversion, submit after conversion | 6.98 | 991 | 672 | 0 |
  | 2-heartbeat conversion, held submit (device order) | 5.62 | 991 | 672 | 0 |
  | streamed baseline, 2-heartbeat conversion | 3.01 | - | - | - |

  ```sh
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --first 100 --count 150 --output <dir> --convert-beats 2 --early
  ```
  The cart model logs the scheduler stage on each heartbeat row and models
  conversion with `FCPICO_WORLD_CONVERT_BEATS` / `FCPICO_WORLD_EARLY`.
- Verified: host CTest 15/15 plain and ASan/UBSan; Python 525 passed; boot ROM
  36 passed; device build OK, firmware 312,148 bytes, RAM post-zone 26,360
  bytes (minimum 24,576). Candidate
  `.build/doom-world-test/artifacts/fcpico_doom_native_world_whx.uf2` SHA-256
  `b55cade4ab953b46e724bcdf3a50c16b4a5ee20460cef6e655687edf5b1a4597`, ROM
  `ee7138fb008ab405355d41a56341e859f444551cb28417dbf0c02278598a0135`. No
  hardware run yet (HR-17 updated).
- Remaining gap to streaming is CHR bandwidth (about 3.2 CHR heartbeats per
  generation). Untried: five tiles per packet (needs NMI cycles), coarser scale
  steps, a larger tile pool.

## P3-W3/W4 -- world sprite frame-rate measurement (2026-10-03)

- Hardware (HR-17 first pass, [log](HARDWARE-LOG.md)): per-tile palette candidate
  `f1e9e45d...` runs; colours much better; frame rate drops while enemy sprites
  are on screen. No serial capture supplied.
- Cosimulation, same ROM/cart model, demo 1 recorded frames 100..249:
  ```sh
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --first 100 --count 150 --output /tmp/fcpico-world-runtime-150            # native
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py --engine <host> \
    --first 100 --count 150 --output /tmp/fcpico-world-runtime-base --baseline  # streamed
  ```
  Native: 0 failures over 1200 PPU frames, 6.89 heartbeats per picture (max 11),
  8.7 fps transport limit. Streamed baseline (`--world-off` recording, empty
  generations): 0 failures, 1.02 heartbeats per picture (58.9 fps limit).
  Native heartbeat use (cart model `stats` line): CHR 677 (55%), OAM 213 (17%),
  waiting for console OAM DMA 129 (10%), idle 215 (17%); 1,576 tiles uploaded for
  127 generations (12.4 per generation), 92 cache hits (6%), 131 resends.
- Interpretation: CHR upload bandwidth dominates. On hardware the gated picture
  holds the back buffer, so render + conversion of the next frame (conversion
  38 ms, P1-T9) only starts after the generation shows; native frames therefore
  cost roughly upload latency plus engine time. Tiles rarely repeat because each
  actor's scale and fcvideo's 4-of-5 column sampling phase change every frame.
- Added for the next measurement: `world_us resolve=avg/max build=avg/max` device
  serial line (raster/masks in `pd_end_frame`, palette plan + tiles at frame end);
  `run_world_runtime.py --baseline` and `heartbeats_per_frame_*`; cart-model stage
  counters. Candidate `.build/doom-world-test/artifacts/fcpico_doom_native_world_whx.uf2`
  SHA-256 `7aed714dd786ab9df58b45fa72e6a0d282f5af718bb152a7ca1992ad4a2926be`,
  firmware 310,652 bytes, RAM post-zone 26,416 bytes.
- Candidate improvements, not implemented (need a decision; see plan 14 notes):
  translation-invariant native-space sampling plus quantised scale bands so tiles
  repeat (cache hits); five tiles per CHR packet using the PAL bytes (+25%, needs
  the 2200-cycle total budget relaxed or trimmed); overlap conversion with upload
  via a third stream buffer (17 KB RAM, more than the 1.8 KB headroom); a lower
  per-generation tile budget that streams more actors to keep fps up.

## P3-W2/W4 -- dynamic per-tile world sprite palettes (2026-10-03)

- User review of the first candidate: zombies colourless, pickups wrong
  (stimpack/medkit red boxes, ARM1 grey, clips without brass, rockets grey, ...),
  and a request to swap palettes per on-screen combination. Changes:
  - Atlas format v2 (`tools/build_world_atlas.py`, 407,997 bytes): per family a
    15-colour local palette clustered in chroma-boosted OKLab (near-greys stay
    neutral; hue difference weighted so dark browns avoid NES olive), 4-bit
    texels, two candidate 3-colour palettes and per-NES-colour u8 error tables.
    Candidates come from reviewed `palette_overrides` in
    `plan/world-sprite-scope.json` first, then a block-clustered exhaustive fit
    that avoids NES olive/teal. `--review LUMPS` writes a 3x source/runtime sheet.
  - Runtime (`fcworld_plan_palettes`, `fcworld_frame_build`): each frame loads
    candidates of the nearest families into sprite palettes 0/2, replacing a slot
    only when the generation still on screen does not use it (commits write
    palettes before OAM DMA, so this prevents recolouring displayed sprites);
    every 8x8 tile picks the loaded palette (0..3) with least weighted error.
  - Flash: `FLASH_WHX_MAX` 2 MB; atlas at `FLASH_WORLD_ADDR` `0x10280000`
    (512 KB); audio may use the whole 512 KB asset region again.
- Verified: host CTest 15/15 plain and ASan/UBSan (`test_fcworld` 1254 checks,
  synthetic v2 atlas, planner lock cases); Python 510 passed; demos 1..3 keep the
  same admission counts with 0 metadata mismatches. Mesen end to end (demo 1,
  frames 100..249): 1200 PPU frames, 0 failures, now also requiring every used
  sprite palette to show its generation's colours; 1200 stable heartbeats, NMI
  exits 251..257, 752 packets, 131 resends, 0 errors. Tiles by palette: 0: 652,
  1: 246, 2: 776, 3: 3; 21 palette-set changes over 149 frames.
- Device candidate `.build/doom-world-test/artifacts/fcpico_doom_native_world_whx.uf2`
  SHA-256 `f1e9e45db571d273342e155c996970be6c7ed1e1d5d88313ce1f284a2b703560`;
  firmware 310,452 bytes; WHX 296,808 bytes below its reservation; atlas payload
  byte-exact at `0x10280000`; RAM post-zone 26,448 bytes. Review sheets:
  `.build/doom-world-test/atlas-review/world_atlas_review.png`,
  `.build/doom-world-test/artifacts/mesen-world-samples.png`.
- Background note: grey 3D views are pre-existing since the 9016 native HUD,
  which takes BG subpalette 3 (`0F 00 30 16`) and leaves the converter grey
  `1D 00 30`, olive `08 18 30` and red `06 16 30`; not changed here.
- Limits: 3 colours per tile remain; previews assume a family owns both dynamic
  slots, so crowded mixed scenes fall back more; colours need user review.

## P3-W3/W4 -- v5 native world sprite runtime (2026-10-03)

- User review decision: RP2350 LUT/DDA scaling, host+Mesen runtime this session,
  full shareware enemy/pickup scope. Committed 2026-10-03 at user request:
  engine `a141b433` (status-bar ammo placeholder) and `70442382` (world sprites),
  then the root world-sprite commit following `d87b82d`, which pins `70442382`.
- Protocol v5 (`fcbus_protocol.h`): unchanged v4 stream/mailbox geometry (count
  15122). One acknowledged world packet per heartbeat in the command area; CHR
  packets carry four tiles in the attribute bytes, OAM packets stage 16 entries
  and may commit a generation with sprite palettes 0/2. Heartbeat pad2 = shown
  generation << 4 | applied sequence. `fcbus_core_publish_world()` swaps a picture
  only when the console reports its generation and replays its ATTR/PAL then.
- ROM `20DOOM-05-9030` (`tools/build_world_sprite_rom.py`): py65 worst cases with
  15 APU pairs: CHR 1707 critical / 2123 total; OAM stage 1217/2111; commit with
  palettes 1284/2191; DMA (packet deferred) 1192+513/2121; tables 1774/2190.
  Palette writes moved from the DMA path (measured 2213 > 2200) to the commit
  packet. Main-loop `WORLD_APPLY`; menu/hide paths yield world slots 35..63.
- CHR: 53 sprite tiles unused by every HUD/menu/logo layout (no glyph restore).
- `port/video/fcworld.{h,c}`: atlas, fcvideo-sampled raster, flip-aware tiles,
  scheduler (shown-generation tile protection, resend, OAM halves, stall release);
  heartbeat path RAM-resident on device without libc.
- Engine (`fcpico/world_sprites.c`, guarded hooks in `r_things.c`, `r_main.c`,
  `pd_render.cpp`): eligible actors are deferred, ranked (monsters, pickups,
  corpses; nearest first) and admitted whole within 26 tiles / 29 OAM / 8 per line;
  rejected actors draw unchanged. Admitted actors never clip PD geometry; masks
  use the resolved front-span depth (`pd_world_front_depth`) before visplanes.
- Atlas (`tools/build_world_atlas.py`, `FCPICO_WORLD_WAD`): 30 families, 314
  patches, 181,894 bytes at `FLASH_WORLD_ADDR` `0x10340000` (audio capped at
  256 KB). Absent from the shareware IWAD: HEAD, SKUL, PINV, PSTR, MEGA, CELL, CELP,
  BSKU, RSKU, YSKU, SGN2, PLAS, BFUG. Palettes 0/2 are fitted to reviewed family
  groups in `plan/world-sprite-scope.json` (pure error minimisation collapsed all
  families to dark browns); result P0 `08 2D 10`, P2 `0F 02 17`.
- Commands and results:
  ```sh
  cmake -S doom -B build-engine -G Ninja -DFCPICO_BUILD_ENGINE=ON -DPICO_PLATFORM=host \
    -DPICO_SDK_PATH=$HOME/.local/fcpico/pico-sdk -DFCPICO_WORLD_WAD=/tmp/fcpico-audio-doom1.wad \
    -DPython3_EXECUTABLE=$PWD/doom/.venv/bin/python
  cmake --build build-engine --target doom_tiny_fcpico
  doom/sim/mesen2/build.sh          # with ~/.dotnet and SDL2 on PATH
  doom/.venv/bin/python doom/sim/mesen2/run_world_runtime.py \
    --engine build-engine/rp2040-doom/src/fcpico_doom_host --first 100 --count 150 \
    --output /tmp/fcpico-world-runtime-150
  FCPICO_AUDIO_WAD=/tmp/fcpico-audio-doom1.wad FCPICO_WORLD_WAD=/tmp/fcpico-audio-doom1.wad \
    BUILD_DIR=/tmp/fcpico-native-world-device doom/tools/build_bg_text_probe_device.sh --native-world
  ```
  - Mesen end to end (demo 1, recorded frames 100..249, real sprite limit): 1200
    captured PPU frames, 0 failures. Each screenshot (rows 0..231) equals the
    independent decode of exactly one recorded picture with the console's OAM/CHR,
    and world OAM 35..63 presents exactly that picture's generation. 1200 stable
    heartbeats at 15122, no DMA stops, NMI exits scanlines 251..257, peak 6 world
    sprites per line, 150 frames published, 127 generations, 741 packets, 128
    resends (DMA frames), 0 scheduler errors. Native update rate: mean 6.79
    heartbeats per generation (about 8.8 fps) with actors changing every frame.
  - Host demos 1/2/3, 1200 frames each: admitted 4590/7865, 6301/12936,
    6217/11466 eligible actors (rest streamed for budget); atlas/engine metadata
    mismatches 0; raster overflow and frame build failures 0.
  - Host CTest 15/15 plain and ASan/UBSan (`test_fcworld` 1350 checks; v5 gate,
    hello and console model); Python bootrom/tools/protocol/fcvideo 497 passed with
    `FCPICO_AUDIO_WAD`; Mesen tests 13 passed; v4 native status Mesen probe still
    count 15122 / NMI scanline 256; `gen_protocol.py --check` and Markdown links pass.
  - Device: firmware 309,228 bytes (215,060 free before WHX); WHX and atlas fit;
    UF2 atlas payload verified byte-exact at `0x10340000`; RAM post-zone 26,452
    bytes against the 24,576 minimum (was 34,632). Candidate
    `.build/doom-world-test/artifacts/fcpico_doom_native_world_whx.uf2`, SHA-256
    `b55b5cfe02425616e9c2ec8a53ea3749405d586cf17c5d580605f80ce92d4b2a`; ROM
    `5a91ddccab31ea1cf327420fc0e7bfebd7abeafa9030271489447705a3923eff` is the one
    Mesen ran. Built from the uncommitted tree on root `bf0280f` / engine
    `0e18c993` (`source.patch` in the artifact directory); see HR-17. No hardware
    flashed at that point; later HR-17 entries record the hardware pass.
- Mesen capture note: `emu.takeScreenshot()` at `endFrame` returns the previous
  picture; the runner pairs screenshot f with the state sampled at frame f-1 start.
- Remaining limits: about 9 fps native updates under motion is below the 20 fps
  target (CHR upload bandwidth: 4 tiles/NMI, resends on UI DMA frames); spectre
  fuzz over a native actor and a one-frame glyph recolour when the menu closes are
  approximate; physical runtime CHR writes unproven (HR-17); P3-W5 open.

## P3-W1/W2 -- world sprite scaling cosimulation review gate (2026-10-02)

- Serena activated for `/home/josh/Github/fc-pico`. User requested sample output
  for candidate approaches before new device firmware. Implemented deterministic
  shareware imp/stimpack extraction, global-phase LUT/DDA scaling, masked CHR,
  whole-actor fixture admission and three isolated NES ROM candidates using the
  current 9016 HUD. Root baseline `bf0280f89c231d4a3db869c08f31dabc4470a670`,
  engine `0e18c993234d76d69f7a0be61b3f329a6b7c732c`; changes are uncommitted.
  Preserved existing README/upstream-plan/memory work.
- Exact generation and capture commands:
  ```sh
  doom/.venv/bin/python doom/sim/mesen2/run_native_status_probe.py \
    --host-frame /tmp/fcpico-audio-engine-compare/guard-updated/stream/frame000300.bin \
    --output /tmp/fcpico-world-background
  doom/.venv/bin/python doom/sim/mesen2/run_world_sprite_probe.py \
    --wad /tmp/fcpico-audio-doom1.wad \
    --background /tmp/fcpico-world-background/frame.bin \
    --output /tmp/fcpico-world-sprites
  doom/.venv/bin/python doom/tools/build_world_sprite_art.py \
    --wad /tmp/fcpico-audio-doom1.wad --output /tmp/fcpico-world-sprites-repro
  doom/.venv/bin/python -m pytest doom/tests/mesen2/test_doom_frame.py \
    doom/tests/tools/test_ppu_sprites.py -q
  git diff --check
  ```
- Results: 54 patches / 176 frame-view mappings; all output hashes and manifests
  identical on regeneration. WAD SHA-256
  `1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771`.
  All 15 scene/approach runs pass exact CHR/OAM and paired-baseline sprite-mask
  comparisons, three consecutive fixed-generation captures, 30 stable
  heartbeats at count 15122 without DMA stops, and NMI on scanline 256.
  Sprite-limit removal disabled; fixed banks unchanged. Relevant reference and
  heartbeat regression tests: 12 passed. Whitespace check passes.
- Samples: `/tmp/fcpico-world-sprites/comparison.png`, `review.html`, `review.md`
  and `summary.json`; full PNG/baseline/ROM/source/mask/CHR/OAM/trace artifacts
  under each scene/approach. The RP host LUT and NES CPU LUT produce identical
  pictures; resident bands expose the requested 13px versus resident 12px size.
  Open samples use 20/22 world OAM and 320/352 CHR bytes. Crowded samples admit
  4/7 actors, consume 24 OAM / 384 CHR bytes and reach eight per scanline; the
  remaining three use whole-actor fixture fallback. Near 64px actor falls back.
  py65 independently executes the assembled CPU LUT/mask routine and measures
  25043..60035 setup cycles, including PPU writes with rendering disabled.
- Exact findings/remaining budgets: [14b](plan/14b-world-sprite-probe-results.md).
  `$1380..$15FF` is gameplay borrowing of menu font tiles, requiring restoration;
  it is not permanently free CHR. All mask scenes use controlled native-coordinate
  visibility over a recorded Doom background; engine ownership, WHX identity,
  geometry restoration, real legacy fallback and continuous tile uploads remain
  open. Host Python timings are not RP2350 measurements; no audio sequence is
  active in these fixed probe captures. P3-W1/W2 remain partial, W3/W4/W5 open.
  No device firmware was built or flashed, and no physical acceptance is claimed.

## HR-16 -- hardware audio pass and merge handoff (2026-10-02)

- User confirms the supplied native audio candidate works on hardware and things
  behave as expected. Recorded the result in `HARDWARE-LOG.md` and moved HR-16
  to completed requests; candidate root `7b63586` / engine `0e18c993`, UF2 SHA-256
  `f5cc51d8ab2698d448971520983c0a07ddd7245b2d9b5445c07fd4f10dbb105a`.
- Implementation findings retained: full music/effect bytes exceed the space
  before WHX, so the archive uses the reserved asset region with matching XIP
  lookup tables and a content-identity check; polls during rendering stalls
  stretched tempo, so device advancement now runs after bus IRQ DMA re-arm,
  protected against engine-side state mutation by local interrupt guards.
- P4-T2 is done: engine modules and optional bank installation are implemented,
  host/sanitizer/flash checks pass, and physical audio works. P4-T6 stays partial:
  scripted end-to-end S4 and extended timing/listening evidence remain. Human
  arrangements, DPCM and the full M4 release gate are not closed by this pass.
- “Traces” in the user's request meant implementation findings and validation
  evidence, not new raw serial/execution captures. No hardware metrics were
  invented or inferred; no new waveform/serial files supplied. Existing host,
  NMI and Mesen evidence is linked in the prior candidate entry.
- Rechecked the 600-frame host audio capture twice while preparing the handoff:
  byte-identical 1,777 writes, maximum 12 pairs per rendered frame, zero drops;
  527 sequencer frames (initialization starts after rendering begins). These
  remain synthetic host measurements, not physical serial readings.
- Hardware evidence, README/audio plan/workplan and completion tracking updated;
  Markdown links and whitespace checks pass. This evidence entry supports the
  requested merge into `doom`; unrelated upstream-contribution edits are preserved.

## HR-16 / P4-T2 -- native HUD audio UF2 candidate (2026-10-02)

- Engine `0e18c993` moves device audio advancement to core 0's accepted heartbeat
  IRQ, after DMA re-arm. All engine sequencer operations, initialization and
  pumps exclude local interrupts. Same-heartbeat polls are no-ops. A callback in
  `fcbus_device` prepares the next mailbox while the engine renders.
- Added `[audio]` serial diagnostics: installed bank, music state, sequencer
  frames, maximum pairs, deferred/dropped writes and maximum IRQ duration.
- Added a host interrupt-mask model test compiling the device guard path, and
  120 simulated heartbeat pumps without engine polling. All 14 CTests pass in
  `/tmp/fcpico-audio-asan` with ASan/UBSan and `ASAN_OPTIONS=detect_leaks=0`.
  Focused engine CTest (`engine_audio_bank|engine_input_walk|host_shim_test`) passes
  3/3 after rebuilding `/tmp/fcpico-native-ui-host-engine`.
- Built native ROM `20DOOM-04-9016` with audio twice using
  `FCPICO_AUDIO_WAD=/tmp/fcpico-audio-doom1.wad BUILD_DIR=/tmp/fcpico-audio-candidate-first
  doom/tools/build_bg_text_probe_device.sh --native-status`, then with
  `BUILD_DIR=/tmp/fcpico-audio-candidate-second`. Complete UF2, ROM and archive
  match byte-for-byte. Logs: `/tmp/fcpico-audio-candidate-{first,second}.log`.
- Candidate copied to
  `.build/doom-audio-test/artifacts/fcpico_doom_audio_native_9016.uf2`; SHA-256
  `f5cc51d8ab2698d448971520983c0a07ddd7245b2d9b5445c07fd4f10dbb105a`.
  Its directory contains ROM, ELF, archive/manifest, checksums, `TESTING.md` and
  `nmi-budget.json`. UF2 payload decoding verifies exact archive bytes and no
  block reaches saves. Firmware: 301,552 bytes; 222,736 bytes free before WHX.
- Strengthened native ROM NMI test to check all 15 register writes, including
  `$4015/$4017`, with text updates and OAM DMA. Focused Python test passes.
  Replayed the packaged ROM itself: text path total 1608 / critical 1201 cycles;
  OAM path total 2098 / critical 1178 + 513 stall cycles. Both satisfy the
  2200 total / 1900 critical limits, including worst-case DMA parity.
- Native status Mesen probe passes with 30 stable heartbeats, count=15122,
  no DMA stops and NMI exits at scanline 256. Artifacts:
  `/tmp/fcpico-audio-candidate-mesen`; log:
  `/tmp/fcpico-audio-candidate-mesen.log`. This validates the console UI path,
  not ARM IRQ execution or audible output.
- Added [HR-16](HARDWARE-REQUESTS.md#hr-16-native-hud-audio-test-candidate-tasks-p4-t2--p4-t6)
  with exact artifact/hash, reproduction, listening steps and serial measurements.
  No hardware flashed. Actual IRQ duration, realtime tempo, sound quality and
  controller behavior under simultaneous audio remain physical checks.

## P4-T2 -- generated bank installation and flash image (2026-10-02)

- Engine commit `f7abee22` installs the optional generated default bank and adds
  host `--dump-apu FILE` capture plus sequencer counters. Explicit caller banks
  still take precedence; missing/mismatched archive identity disables the bank.
- Added `tools/audio/build_bank.py` and `port/audio`: all 13 shareware songs and
  55 synthesized effects are generated locally from `FCPICO_AUDIO_WAD`, with
  priorities read from engine `sounds.c` and a documented initial voice mapping.
  Archive and generated C/JSON remain build outputs, with no game assets in git.
- Placement: 227,280-byte archive at `FLASH_ASSETS_ADDR` (`0x10300000`), leaving
  297,008 bytes before saves. Device C tables use XIP pointers; host builds embed
  identical bytes. A content identity prefix pairs archive and firmware tables;
  it is not a runtime payload checksum.
- Extended `whx2uf2.py --audio` to merge archive blocks with firmware/WHX and
  reject reserved-region overflow and WHX overlap. `fcpico_doom_audio_uf2` builds
  a complete image. `build_shadow_detail.sh` supports `FCPICO_AUDIO_WAD`, includes
  matching archive/manifest checksums, and explicitly clears audio when omitted.
- Verified: `FCPICO_AUDIO_WAD=/tmp/fcpico-audio-doom1.wad doom/.venv/bin/python
  -m pytest doom/tests/tools -q`: 378 pass, including ASan/UBSan generated-table
  replay of every real and synthetic song/effect, identity mismatch and UF2 bounds.
  `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/fcpico-audio-asan
  --output-on-failure`: 13/13 pass after rebuilding. Focused engine CTest
  `engine_audio_bank|engine_input_walk|host_shim_test`: 3/3 pass.
- Two independent 600-frame demo runs emit identical APU captures: 1,777 writes,
  max 12 per frame, zero sequencer drops. Capture SHA-256:
  `c72ca910a39ded3377435c0be3c3091a9e01cf382523523a6dc27fcbf2631616`.
  All indexed frames and NES streams still match the untouched pre-adapter
  baseline; artifacts: `/tmp/fcpico-audio-engine-compare/{bank-first,bank-repeat}`.
  The two previously documented baseline golden failures remain unrelated.
- Verified pinned device build via `FCPICO_AUDIO_WAD=/tmp/fcpico-audio-doom1.wad
  doom/tools/build_shadow_detail.sh`. Tutorial MD5 and permanent fix-bank checks
  pass; firmware flash check passes: 301,064 bytes, 223,224 bytes free before WHX.
  Decoded merged UF2 archive bytes exactly match host/device `audio.bin`, and
  every block ends before saves. Archive SHA-256:
  `53b832ba1132c77def125e12b44330cd4d9bb6f0b5dfba73c6c1e5817312b9e8`.
- Complete flashable candidate:
  `.build/doom-shadow/artifacts/fcpico_doom_shadow_detail_delay_whx.uf2`, SHA-256
  `bf12fcbb1a4c2092e0552ee14a5e2a3400be1a711703bf6b5d42d2a75f5add9d`.
  Build log: `/tmp/fcpico-audio-bank-device-build.log`. No device was flashed.
- Markdown links, shell syntax and whitespace checks pass. P4-T2 stays partial:
  rendering-stall tempo, combined UI/APU mailbox/NMI timing, S4/Mesen and physical
  listening remain. DPCM and human arrangements are separate unfinished tasks.

## P4-T2 -- engine audio adapters (2026-10-02)

- Committed converters in root `fb016e2`, then continued with engine commit
  `ff1b39bc`. Sound/music modules now use `fcapu`, resolve WHX music names and
  SFX aliases, and support stable handles, pause/resume, stop/query and volume.
- Added sequencer music status and synthesized volume updates during silent
  frames. Added `tests/audio_engine` against the actual engine adapter, covering
  synthetic and empty banks, priority steals, stale handles and deferred stops.
- Device delivery runs on core 0 once per bus heartbeat under an interrupt
  guard. Host delivery uses rendered frames and discards writes. No generated
  bank is installed: default builds remain silent and P4-T2 stays partial.
- Verified host-only configure/build and CTest in
  `/tmp/fcpico-audio-host-unit`: 13/13 pass. ASan/UBSan build in
  `/tmp/fcpico-audio-asan`: 13/13 pass with `ASAN_OPTIONS=detect_leaks=0`
  (LeakSanitizer cannot run under the environment's ptrace).
- Verified `doom_tiny_fcpico` host build in
  `/tmp/fcpico-native-ui-host-engine`; `engine_input_walk` and `host_shim_test`
  pass. Two golden checks (`engine_host_determinism`,
  `engine_title_menu_golden`) fail identically on untouched root `fb016e2` /
  engine `84ecd1de`, built separately in `/tmp/fcpico-audio-baseline-build`.
  Independent 600-frame demo runs of baseline and two updated runs have identical
  indexed frames and NES streams; dumps are in
  `/tmp/fcpico-audio-engine-compare/{baseline,guard-updated,guard-repeat}`.
- Verified pinned device build:
  `NESASM_BIN=/home/josh/Github/fc-pico/.build/tools/nesasm/nesasm cmake --build
  .build/doom-shadow/device --target doom_tiny_fcpico -j4`.
  Permanent fix-bank check passes. `python3 doom/tools/flash_layout_check.py
  .build/doom-shadow/device/rp2040-doom/src/fcpico_doom.elf --whx
  doom/rp2040-doom/doom1.whx` passes: firmware 299,000 bytes, 225,288 bytes free
  before the WHX region. The 213,777-byte generated music bank would leave only
  11,511 bytes there before effects/metadata; placement still needs proof.
- Markdown links and both repository whitespace checks pass. Remaining work:
  install the generated bank, prove flash placement, then validate audible
  playback and combined UI/APU timing on the console. No hardware claim made.

## P3-U4 -- reproducible native HUD and NAW dump (2026-10-01)
- Commits: root `006413b` plus the 9016 HUD commits; engine submodule
  `84ecd1de`.
- Verified: `OUTPUT_DIR=/tmp/fcpico-native-menus-9016-committed
  doom/tools/reproduce_native_hud.sh` -> 34 Python tests, two host C tests,
  five Mesen scenes, and successful 9016 device build. A separate
  `--verify-twice` run built byte-identical ROMs and UF2s; post-commit ROM and
  UF2 matched the recorded hashes. UF2 SHA-256 is
  `44228ccb98282e2f35decfea7123cdd35e8e7044910cea258f5b4037d73e6667`.
- Artifacts: default script output is under `/tmp/fcpico-native-hud-repro/`;
  the flashable file is `device-first/artifacts/fcpico_doom_native_status_whx.uf2`.
  NAW import files (`doom-hud.ppu`, `.oam`, captures and archive) are tracked
  under `assets/hud_edit_reference/` and reproducible with
  `tools/export_hud_ppu_dump.py` from the Mesen capture.
- Physical validation: user reports the 9016 build works as intended. Console
  model, region, serial excerpt, photos and individual interaction sequence
  were not restated. The hardware log records the confirmation. The output
  manifest records root and engine commits plus source, ROM and UF2 hashes.

## <task id> -- <title>
- Commits: <range or list>
- Verified: <command> -> <result summary>
- Measurements: <fps / cycles / bytes / ms, with the command>
- Left out: <what and why>
- Plan changes: <documents touched>
```

## I-05 / P4-T3 -- automatic music and synthesized effects complete (2026-10-01)

- Commits: uncommitted working-tree changes based on root `ce221d4`;
  engine submodule remains `84ecd1de`.
- Added `tools/audio/mus2apus.py`: direct MUS parser, MIDI type 0/1 parser
  with rational tempo timing and running status, deterministic pulse/pulse/
  triangle/noise arrangement, sustain/volume/expression, finite/looping playback
  and a whole-shareware-WAD CLI. A setup frame and empty retrigger frame keep
  the first note audible on later loop iterations under `fcapu`'s own thinning.
- Added `tools/audio/sfx2apu.py`: DMX/WAV RMS envelope with pulse pitch or
  noise brightness tracking, voice-restricted APUS effects, final muting, and
  hand-authored `.sfx` overrides. Nonperiodic PCM with negative correlation
  peaks exposed a pitch-estimation exception; fixed and covered with a small
  synthetic regression fixture.
- Verified: `FCPICO_AUDIO_WAD=/tmp/fcpico-audio-doom1.wad
  doom/.venv/bin/python -m pytest doom/tests/tools -q` -> 369 passed,
  including the opt-in real-WAD acceptance test. The ordinary suite uses
  synthetic fixtures and skips only the external-WAD test.
- Real input: Doom95 shareware archive downloaded from the
  [idgames mirror](https://ftp.fu-berlin.de/pc/games/idgames/idstuff/doom/win95/doom95.zip)
  into `/tmp/fcpico-audio-shareware.zip`, with only `DOOM1.WAD` extracted to
  `/tmp/fcpico-audio-doom1.wad`; no game assets added to git. WAD MD5:
  `f0cefca49926d00903cf57551d901abe`; SHA-256:
  `1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771`.
- Verified twice: `doom/.venv/bin/python doom/tools/audio/mus2apus.py
  /tmp/fcpico-audio-doom1.wad --auto -o /tmp/fcpico-audio-music-first`
  and the same command with `-o /tmp/fcpico-audio-music-second`. All 13
  outputs match byte for byte and decode under the 12-pair music cap.
  Individual sizes, frame counts, pair maxima and SHA-256 hashes are in
  `/tmp/fcpico-audio-metrics.json`. Music totals 213,777 bytes, with sizes
  from 318 (`D_INTROA`) to 36,794 (`D_INTER`), exceeding the original
  2-6 KB estimate. Flash placement remains an integration gate.
- Verified: all 55 real DMX lumps converted twice to both pulse/noise
  (110 scripts) with identical bytes, no loop and final muted channel state;
  at most four pairs per frame. Outputs are in `/tmp/fcpico-audio-effects/`.
  The optional WAD test reproduces these checks without storing sample bytes.
- Additional integration probe: `/tmp/fcpico-audio-replay.c` compiled with
  `gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g
  -Idoom/port/apu -Idoom/fcbus /tmp/fcpico-audio-replay.c
  doom/port/apu/fcapu.c -o /tmp/fcpico-audio-replay`. Running
  `ASAN_OPTIONS=detect_leaks=0 /tmp/fcpico-audio-replay
  /tmp/fcpico-audio-loop-test.apus /tmp/fcpico-audio-music-first/*.apus`
  replayed two loops of every song through the unmodified C sequencer with
  no drops, at most 12 writes per frame and channel disable on stop. The
  one-note synthetic stream asserted the length reload on both iterations.
  LeakSanitizer alone cannot run under this environment's ptrace; ASan and
  UBSan passed with leak detection disabled.
- Verified: `python3 doom/tools/check_md_links.py doom` and `git diff --check`.
- Plan correction: shareware has 13 songs; remove episode-3 `D_BUNNY` from
  its list (the real WAD omits it and `src/doom/f_finale.c` uses it for episode 3).
  Record measured sizes instead of asserting the old flash estimate, and
  correct the stated SFX headroom to three pairs under the 15-pair mailbox cap.
- I-05 and P4-T3 are done for host tooling. FamiStudio arrangements, listening,
  the engine audio adapter, resource placement, DPCM bank and combined
  console/controller timing remain later work; no audible-console claim.

## I-05 / P4-T3 -- WAD extraction and VGM music conversion (2026-10-01)

- Commits: uncommitted working-tree changes; no engine submodule changes.
- Added `tools/audio/wadextract.py`: validated IWAD/PWAD lump extraction,
  case-insensitive names/globs, last-entry duplicate resolution and deterministic
  filenames. Synthetic fixtures cover ranges, unsafe names and missing selections.
- Added `tools/audio/vgm2apus.py`: VGM/VGZ NES register writes, sample waits,
  NTSC/PAL timing, shared retrigger thinning and a 12-pair music cap. Loops retain
  their initial writes; unrepresentable loops and unsupported chips/DPCM fail
  explicitly. CLI fixtures round-trip through the existing APUS reader and
  compare two outputs byte for byte.
- Verified: `doom/.venv/bin/python -m pytest doom/tests/tools/test_audio_convert.py -q`
  -> 48 passed. `doom/.venv/bin/python -m pytest doom/tests/tools -q`
  -> 318 passed. System `python3` has no pytest; use the existing repository venv.
- Verified: `python3 doom/tools/check_md_links.py doom` and `git diff --check`.
- Limits: synthetic inputs only; no FamiStudio arrangement export or hardware
  listening claim. Automatic MUS/MIDI conversion, synthesised-effect conversion,
  all-shareware-song acceptance and engine/console integration remain open.
- Updated I-05/index, P4-T3 status, audio plan and README; no firmware, bus
  protocol, permanent fix bank or tutorial changes.

## P3-U4 concrete HUD candidate 9013 (2026-09-29)
- Built a flashable WHX UF2 with the unchanged permanent fix bank. The
  renderer now paints concrete and native-resolution status text from the
  semantic snapshot: current ammo, health/armor percentages and all four
  current/max ammunition rows. Host-only max ammo follows backpack capacity.
  ARMS 2–7 and key icons remain sprites. A 28×32 source-derived face sits in a
  32×32 square; ten resident expressions deduplicate to 119 flipped tiles.
- Verified: Python `doom/tests` 389 passed; host C `test_fcvideo` and `test_fcui`
  passed; Mesen status, title, pause, stale menu and special-face probes passed
  with 30 stable heartbeats, `count=15122`, no DMA stops and NMI exit at
  scanline 256. The status OAM peaks at eight sprites per scanline. Firmware
  flash layout uses 290,984 of 524,288 bytes before WHX; the ROM fix bank
  matches exactly. The Mesen fixed-frame status preview omits the two dynamic
  host commands that darken four face-backing tiles in the skipped row.
- Visual preview: `/tmp/fcpico-native-ui-status/mesen/run/final.png`; physical
  validation pending [HR-15](HARDWARE-REQUESTS.md#hr-15-concrete-native-hud-and-enlarged-face-task-p3-u4).
- Next: [background-first plan](plan/12-sprite-ui.md#background-first-follow-up-and-8x16-evaluation-2026-09-29)
  for static menu art and possible 8×16 sprites.

## P3-U4 visual follow-up -- Mesen iteration after HR-14 (2026-09-28)
- [HR-14](HARDWARE-REQUESTS.md#hr-14-aligned-status-and-paused-menu-doom-logo-task-p3-u4-visual-follow-up)
  reported a yellow logo corner outside menus, incomplete right-side face,
  shortened difficulty labels, small main-menu letters and `READ` instead of
  `READ ME`. A stale-menu-ID Mesen probe reproduced the corner; scanline OAM
  accounting reproduced nine sprites on each upper face row. Both checks were
  red before the fixes and green after them.
- The ROM now gates logo drawing on the active menu bit. The face moves to a
  full-height center niche, and weapon/key rows move below the number rows;
  Mesen sees no scanline with more than eight active sprites. The paused main
  menu uses 8×8 full-size letters, a 56×24, 21-tile original-derived logo
  with three brighter visible colors, and live health/armor. The face,
  weapons and keys reappear when the menu closes. `READ ME` is complete.
- The difficulty menu suppresses abbreviated sprite labels and keeps Doom's
  original styled patches. A scripted host-engine run reached skill menu ID 3
  and showed the full five original names; its captured frame also passed v4
  Mesen cosimulation. These long patches still pass through the background
  converter and are visibly dithered. Native background treatment remains a
  separate clarity improvement.
- Native-size editable PNGs for title/paused logos, all 42 faces and paired
  episode letters now round-trip losslessly through `edit_sprite_sheets.py`.
  `preview_native_ui.sh` generates Mesen test scenes without a device build.
  `pytest doom/tests -q` passed 389/389 and host C tests passed 11/11.
  Paused, title, episode, difficulty, options, gameplay, ouch and stale-ID
  Mesen probes each passed 30 stable `count=15122` heartbeats and NMI exit on
  scanline 256. The host engine builds and the input-walk test passes. Two
  host image-golden tests still fail against old expected frames; both fail
  identically with the submodule's original `m_menu.c`, so this iteration did
  not cause those mismatches. No UF2 was built during this iteration, as
  requested.

## P3-U4 visual follow-up -- physical alignment and paused logo (2026-09-28)
- [HR-13](HARDWARE-REQUESTS.md#hr-13-styled-status-sprite-logo-and-episode-names-task-p3-u4-visual-follow-up)
  photo showed top status values crossing the metal border and Doomguy
  crossing the lower divider. The 9010 source-derived logo was limited to
  the title, so it was absent on the photographed paused menu.
- Moved number rows and the 3×3 face upward in OAM. Static `H` and `R` labels
  now render directly in the native status background, freeing four sprites.
  Paired main-menu glyphs use 14 entries. An 18-entry, 48×24 reduction of the
  same `M_DOOM` patch fills the paused-menu capacity exactly: 31 status, 14
  labels, 18 logo and one cursor = 64. The 64×32 original-derived title logo
  remains. Mesen's paused and title screenshots show the corresponding logos.
- `pytest doom/tests -q` passed 388/388; `ctest --test-dir build-host`
  passed 11/11. Mesen paused, title, gameplay, episode, options and grin
  probes each passed 30 stable heartbeats at `count=15122`, no DMA stops,
  with NMI exit on scanline 256. The device build passed flash layout and
  fix-bank gates, links at 289996 firmware bytes, and embeds the byte-identical
  Mesen ROM SHA-256
  `3ed0ae5f185fb9793e87333386819c7e78de0d008fe8d131376656a2cf369ebf`.
  Physical validation of the 9011 layout and lower-half logo tiles remains
  open in [HR-14](HARDWARE-REQUESTS.md#hr-14-aligned-status-and-paused-menu-doom-logo-task-p3-u4-visual-follow-up).

## P3-U4 visual follow-up -- styled status and native menu art (2026-09-28)
- [HR-12](HARDWARE-REQUESTS.md#hr-12-transparent-sprite-menus-gray-status-red-text-and-special-faces-task-p3-u6)
  physically passed red text, gray status backing and hit/pickup faces; the
  shareware episode menu still used numbered labels. Plan 12 now records the
  visual follow-up and its OAM/palette constraints.
- The status backing has a native pixel metal frame and section separators.
  Tall sprite digits and the BG ammo row have dark one-pixel shadows. The
  shareware episode menu uses three actual names packed into 19 paired-glyph
  CHR tiles and 21 OAM entries. The first 64×24 block-letter logo was
  rejected visually before hardware testing. A 64×32, 32-tile blue/gold
  version now samples the actual `M_DOOM` patch at nearly its original aspect
  ratio and replaces the converted main-menu patch on the title screen. It is omitted
  on the paused game menu because 35 status entries plus 26 menu entries and
  a cursor leave only two OAM slots.
- `ctest --test-dir build-host --output-on-failure` passed 11/11 and
  `python -m pytest doom/tests -q` passed 388/388. Mesen title, episode,
  paused main/options, status, ouch and grin probes each passed 30 heartbeats
  at `count=15122`, no DMA stops, and NMI exit on scanline 256. The device
  build passed flash layout and fix-bank gates, links at 289808 firmware
  bytes, and embeds ROM SHA-256
  `845b7b1c8c0a010df8b11bddaad4de3db7baabb922888e81244157698b1d34fb`.
  New lower-half sprite pattern storage and display contrast await NES-001
  validation in [HR-13](HARDWARE-REQUESTS.md#hr-13-styled-status-sprite-logo-and-episode-names-task-p3-u4-visual-follow-up).

## P3-U6 -- transparent menu and special face candidate (2026-09-27)
- [HR-11](HARDWARE-REQUESTS.md#hr-11-native-pause-hud-sprite-menus-and-idle-face-task-p3-u5)
  passed on NES-001 for persistent status and idle face animation. The game
  now skips original label and skull patches on five native menus, and the
  converter no longer masks their backdrop. Titles, sliders and dynamic
  load/save screens continue to use converted game graphics.
- The visible status region uses solid gray background palette 3; the other
  three background palettes remain available to the game picture. Tall HUD
  glyphs, menu labels and native ammo-row glyphs use red. Two 3×3 faces were
  added to the resident atlas for `STFOUCH0` and `STFEVL0`. Bonus pickups now
  request the grin, and head-on damage selects the ouch resident face.
- Mesen validates transparent menu and both special face packets with 30
  stable `count=15122` heartbeats each and no DMA stops. The RP2350 image
  links at 289616 firmware bytes and embeds the byte-identical Mesen ROM
  SHA-256 `5f79f2619203943021931c1b32522b4f4f012ad5b297b2d297fc8d0471efbd8a`.
  All 11 host C tests and 386 Python tests pass. [HR-12](HARDWARE-REQUESTS.md#hr-12-transparent-sprite-menus-gray-status-red-text-and-special-faces-task-p3-u6)
  physically confirms red text, gray backing, and working pickup/ouch faces.
  Native episode labels are generic; plan 12 records a follow-up for actual
  episode names and richer status/menu art.

## P3-U5 -- persistent HUD, native menu labels and idle face (2026-09-27)
- [HR-10](HARDWARE-REQUESTS.md#hr-10-native-sprite-status-and-doomguy-face-task-p3-u4)
  established stable v4 transport and a good physical native HUD, but Start
  hid the sprites and exposed the old converted bar. Mesen reproduced this
  with a Start input poll and menu-state UI packet.
- The status now remains visible during menus, including its native ammo row;
  the converter clears the old status backing on paused frames. The ROM has
  native sprite labels and a cursor for main, episode, skill, options and sound
  menus. Menu id and selection reuse high bits of the v4 UI packet. A bounded
  dark panel replaces legacy menu label pixels, preserving options and sound
  controls. Load/save names and other long or dynamic screens retain their
  background text.
- The resident atlas now includes the three healthy straight faces, four
  damage representatives, god and dead (nine faces, 81 tiles), plus 24 tall
  glyph tiles. The full 42-face sheet remains available as source art.
- Mesen Start probes cover gameplay pause, title menu without status, options,
  and healthy face variants. Each completes 30 stable count-15122 heartbeats
  with no new DMA stops and NMI exit at scanline 256. The permanent fix bank
  still matches byte for byte. The RP2350 image links with 289528 firmware
  bytes and matches the passing Mesen ROM SHA-256
  `cfa4f4c4f2e840f125a6f45b0b8d89d611705358712c7cae6707cdd03edecaaf`.
  All 11 host C tests and 386 Python tests pass. [HR-11](HARDWARE-REQUESTS.md#hr-11-native-pause-hud-sprite-menus-and-idle-face-task-p3-u5)
  physical validation passed for persistent status and idle face; see HR-11.

## P3-U4 -- native status sprites and authentic 3×3 face (2026-09-27)
- [HR-9](HARDWARE-REQUESTS.md#hr-9-native-background-text-and-v4-status-transport-task-p3-u3)
  passed on NES-001. After the countdown, native bottom text and live `H100`
  appeared over Doom. Later heartbeats `7037..9773` held `proto=4`,
  `count=15122`, `raw=41114`, no new stops/resyncs/timeouts/errors, and stable
  dropped count 102. Earlier `proto=2` lines belong to the previous ROM before
  update. See [hardware log](HARDWARE-LOG.md).
- Generated [all 42 face variants](assets/doomguy_faces.md) from the local
  WHX as 24×24, 3×3-tile art: 6048 CHR bytes. A seven-face representative
  set occupies 63 tiles; 24 tall-glyph tiles plus those faces use 87 of the
  128 pattern slots beside the system font. Mesen shows the real face art.
- The isolated `20DOOM-04-9006` ROM displays two-tile-high health and armor,
  a 3×3 face, seven owned-weapon indicators and three key slots. Its 35 OAM
  entries peak at eight sprites per scanline. The converter clears the legacy
  status backing only during visible v4 gameplay. A 28-tile BG row shows
  bullet, shell, rocket and cell counts around the face; two changed tile
  commands per frame update it through the v4 NMI.
- Mesen: 30 stable count-15122 heartbeats, exact CHR/OAM/UI bytes and NMI
  completion. A dead-face/H000/R999 variant also passes. Py65 measures the
  two-command NMI at 1201 critical/1608 total cycles with 15 APU pairs, and
  deferred OAM commit at 1178 critical plus 513 DMA/2098 total. Both remain
  inside the 1900/2200 limits. All 11 host C tests pass. The RP2350 target
  links with 289248 firmware bytes, leaving 235040 before WHX.
- [HR-10](HARDWARE-REQUESTS.md#hr-10-native-sprite-status-and-doomguy-face-task-p3-u4)
  candidate UF2 is ready (SHA-256
  `1f6130e97ad5e00fa8c7ea3cec7833db5c8e4202c0618cd059fc75f1cbbcb8f4`);
  its embedded ROM matches the passing Mesen ROM. Physical status layout and changing ammo row remain
  unverified. The current ROM maps other expressions to a face from the same
  pain level; full face paging, ready-ammo/current-max detail, menu rendering,
  and final visual tuning remain.

## P3-U3 -- v4 native background text and device stream compaction (2026-09-27)
- [HR-8](HARDWARE-REQUESTS.md#hr-8-intermittent-sprite-diagnosis-task-p3-u2)
  produced good-boot photos before and during Doom: the original A/diamond,
  same-index duplicate pair and copied-art pair are all visible. The
  intermittent missing-symbol state was not reproduced.
- Added an isolated v4 ROM (`20DOOM-04-9001`) with 28 fixed native background
  text tiles in row 28 and the v3 health sprite decoder. A compacted picture
  stream skips the 448 pattern bytes belonging to that row, moving the
  mailbox from byte 15426 to 14978. The expected frame count is 15122.
- Mesen renders `NATIVE BACKGROUND TEXT WORKS` and `H200` together. It reports
  30 stable count-15122 heartbeats, exact nametable tiles, UI bytes, OAM,
  palette and attributes, and NMI RTI on scanline 256. The 28-tile row and
  the picture outside that row round-trip through the Python encoder; a C
  test checks every retained word in the RP2350 compaction path.
- The device applies compaction only after v4 negotiation and discards a
  converted frame if the protocol changes during conversion. The existing
  v2 production ROM still uses its original layout. The RP2350 target links,
  flash layout passes at 288944 firmware bytes, all 11 host C tests pass,
  and 290 Python tools/boot-ROM tests pass.
- Built [HR-9](HARDWARE-REQUESTS.md#hr-9-native-background-text-and-v4-status-transport-task-p3-u3)
  candidate `/tmp/fcpico-bg-text-device/artifacts/fcpico_doom_bg_text_probe_whx.uf2`,
  SHA-256 `86f68ed099a860767c5d6a9bef2a15a10154d9813188947190f62fe6511f9fa1`.
  Its embedded ROM is byte-identical to the Mesen probe and its fixed bank
  matches the tutorial image. Physical v4 validation remains pending.
- This probe uses a fixed message and only H plus three health digits. Dynamic
  long text, the full status bar, authentic face sprites and menu layout are
  still to be implemented and validated.

## P3-U2 / U3 -- intermittent NES sprite probe and v3 transport slice (2026-09-27)
- [HR-7](HARDWARE-REQUESTS.md#hr-7-sprite-pattern-storage-and-visible-probe-task-p3-u2)
  reflashed and initially displayed the `A`, diamond, two rectangle face-shape
  placeholder tiles and checker over working Doom video. After two NES power
  cycles the `A` and diamond were missing; they returned after another cycle.
  Heartbeats `10173..12616` stayed at count 15554 with no new stops, resyncs,
  timeouts or errors. [Hardware log](HARDWARE-LOG.md#2026-09-27----hr-7-sprite-probe-on-nes-001)
  records the photos and counters. The rectangles are geometry probes, not
  Doomguy art. This is an intermittent result, not a completed atlas gate.
- Added an optional diagnostic ROM (`20DOOM-02-9002`) that repeats the original
  A/diamond pattern indices in later OAM slots and copies their art to two new
  pattern indices. `run_sprite_probe.py --diagnostic` passes in Mesen with 232
  exact changed pixels, unchanged background, OAM/CHR/palette equality and 30
  stable count-15554 heartbeats. The corresponding NES-001 UF2 is
  `/tmp/fcpico-sprite-diagnostic-device/artifacts/fcpico_doom_sprite_diagnostic_whx.uf2`,
  SHA-256 `d23459faad3c7c83771473a12d0f9a22870d175a66e97fe33cdae789669442d4`.
  See [HR-8](HARDWARE-REQUESTS.md#hr-8-intermittent-sprite-diagnosis-task-p3-u2).
- `run_sprite_probe.py --limit` puts nine sprites on one scanline. Mesen retains
  all nine OAM entries but visibly drops the last, with sprite-limit removal
  disabled; its first-eight mask matches the independent compositor.
- Added v3 negotiation and a fixed 144-byte mailbox carrying a 16-byte UI
  snapshot, with count 15570. V1/v2 counts and mailboxes are unchanged. The
  isolated `20DOOM-03-9001` console ROM receives the extra bytes at `$0300`.
  Its NMI commits a complete `$0200` OAM shadow by DMA only when `$0310` is
  set, deferring BG palette/attributes for that frame. This is a measured
  transport slice, not a production HUD ROM.
- Py65: full tables plus 15 APU pairs take 1742 critical / 2165 total cycles;
  OAM commit plus 15 pairs takes 1684 critical including the worst DMA parity /
  2091 total. Both fit the 1900/2200 limits. Mesen: 30 stable count-15570
  heartbeats, exact 16 UI bytes, correct BG tables, NMI RTI at scanline 256.
  Reproduce with `DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python
  doom/sim/mesen2/run_ui_transport_probe.py`.
- Added a compact, checked status packet with 10-bit ammo values, an engine
  capture of current health, armor, ammo, weapon/key state and resolved face,
  and v3-only publication from the device frame sink. The current production
  ROM still negotiates v2, so these bytes are not yet displayed. All 11 host C
  tests and 289 tools/boot-ROM Python tests pass; the RP2350 Doom target links.
- The isolated `--hud` v3 ROM verifies packet generation/XOR, decodes health
  into native system-font sprites in a complete OAM shadow, then commits by
  DMA on the next NMI. Mesen renders `H000`, `H200` and `H999` on a reserved
  dark status strip with exact OAM, count 15570 and NMI RTI at scanline 256.
  This proves a dynamic native digit path, not the complete status bar.
- Remaining: diagnose the intermittent physical sprite symbols; expand the
  status decoder, upload real face art, suppress the converted legacy status, provide
  native BG text for long rows and menus, and validate dynamic scenes in Mesen
  and on NES-001. No production HUD migration is claimed yet.

## P3-U2 -- setup-time sprite probe in Mesen (2026-09-25)
- `tools/build_sprite_probe.py` builds an isolated Doom v2 ROM with known
  sprite patterns at `$1800..$185F` and `$1FF0..$1FFF`, seven visible OAM
  entries and a sprite palette color. The production ROM source and permanent
  fix bank are byte-unchanged. The boot ROM's system font occupies the first
  128 sprite tiles (`$1000..$17FF`) in the Mesen dump; the probe uses the other
  128 tiles as a candidate atlas region.
- `sim/mesen2/run_sprite_probe.py` runs that ROM and the normal v2 ROM against
  the same synthetic background stream. The Mesen probe had 30 stable
  heartbeats at count 15554, no DMA stops, exact BG palette/attributes, and
  NMI exits on scanline 255. OAM, authored CHR bytes and sprite palette match
  exactly; the independent reference and Mesen agree on all 168 changed sprite
  pixels. Every pixel outside the probe area matches the normal ROM image.
- Verified: `DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python
  doom/sim/mesen2/run_sprite_probe.py --output /tmp/fcpico-sprite-probe-u0`
  -> pass. `sim/mesen2/DOOM-FRAME.md` records the runnable command and limits.
- Device candidate: `doom/tools/build_sprite_probe_device.sh` passed the
  tutorial MD5 gate, pinned RP2350 build and flash-layout check. Firmware uses
  287836 bytes, leaving 236452 before WHX. The merged UF2 is
  `/tmp/fcpico-sprite-probe-device/artifacts/fcpico_doom_sprite_probe_whx.uf2`
  (8158 blocks), SHA-256
  `a33f2ef0780d2fa49b487225636a7989e891e3e5a791892aba0f89c0b5b3e022`.
  Embedded ROM SHA-256 `0522ca2793f71bc83851d7d55ae588d45e8143779ecacc0c23da92785de9ccb2`
  equals the Mesen U0 ROM; its test-only stamp `20DOOM-02-9001` forces a
  reversible reflash through the unchanged fix bank. [HR-7](HARDWARE-REQUESTS.md#hr-7-sprite-pattern-storage-and-visible-probe-task-p3-u2)
  requests the physical check.
- Remaining: physical pattern storage/mirroring and overscan proof, followed
  by bounded atlas uploads, negotiated UI transport and actual HUD migration.

## P3-U1 / U3 -- resident status and transport candidate (2026-09-25)
- Added `plan/sprite-ui-status-resident-candidate.json`: ordinary resident
  8×8 digits/icons plus a 16×16 face cost 29/64 OAM and peak at 8/8 sprites
  per scanline. Four current/max ammo pairs use a reserved 28-tile native BG
  text row. This is a capacity candidate, not a rendered HUD or accepted layout.
- Measured v2 NMI variants with `NmiHarness`: no BG writes 1034 critical /
  1073 total cycles; attributes 1492/1531; palette 1156/1195; both 1614/1653.
  Fifteen APU pairs add 368 total cycles. The audit tables a candidate
  constant 16-byte UI descriptor and a deferred OAM DMA on a no-table-write
  frame; exact v3 decode, generation and both DMA parities remain unmeasured.
- Verified: `python3 doom/tools/check_sprite_layout.py
  doom/plan/sprite-ui-status-resident-candidate.json --lines` -> 29 OAM,
  peak 8/8 and 28 reserved BG text cells. The checker now validates native
  BG text cell geometry separately from OAM. `doom/.venv/bin/python -m pytest
  doom/tests/tools doom/tests/mesen2 -q` -> 276 passed;
  `python3 doom/tools/check_md_links.py doom` -> 49 files, 596 links,
  zero broken; `doom/.venv/bin/python -m pytest doom/tests/bootrom -q`
  -> 25 passed; `git diff --check` -> clean. No production protocol or HUD
  migration was claimed.

## P3-U1 / U2 groundwork -- native text scope and sprite reference (2026-09-25)
- Serena project `fc-pico` activated. The user approved native background tiles
  for long HUD messages and menu rows; compact status values, face and icons
  remain sprite targets. [Plan 12](plan/12-sprite-ui.md) and the
  [U1 audit](plan/12a-sprite-ui-u1-audit.md) now record the decision and the
  calibrated stream-fetch concern. Source research cites Dragon Warrior and
  Zelda disassemblies plus NESdev PPU/vblank documentation; game behavior
  inferred from symbols is labeled as such.
- `tools/ppu_decode.py` accepts optional OAM, sprite CHR and sprite palette
  dumps for an independent 8×8 sprite composite over the current stream. It
  models OAM selection/order, eight-per-line limit, Y offset, priority, flips,
  transparency, clipping and palette aliases. It was subsequently compared
  with the Mesen U0 sprite frame above.
- Corrected `tools/check_sprite_layout.py` to count all eight scanlines of a
  partially occupied tile and reject scanline zero, then reran the compact
  status draft: 38/64 OAM,
  peak 8/8 scanline sprites. This is a capacity draft, not legibility evidence.
- Verified: `doom/.venv/bin/python -m pytest doom/tests/tools -q` -> 262 passed;
  `python3 doom/tools/check_sprite_layout.py doom/plan/sprite-ui-status-draft.json`
  -> 38/64, 8/8; `python3 doom/tools/check_md_links.py doom` -> 49 files,
  591 links, zero broken; `git diff --check` -> clean.
- Remaining: U1's all-screen layouts, atlas/RAM/flash budget and measured
  transport schedule; U2 sprite storage/visibility probe on NES-001;
  production sprite HUD transport, rendering, suppression and validation.

## P3-U1 -- source dimensions and sprite capacity gate (2026-09-25)
- Added the [feasibility audit](plan/12a-sprite-ui-u1-audit.md) with active
  producer inventory, WHX metadata identity/dimensions, NMI/transport lower
  bounds and the remaining layout, storage and timing decisions.
- Added `tools/check_sprite_layout.py` to reject proposed native layouts above
  64 OAM entries, eight sprites per scanline or 256x240 bounds. `--lines` prints
  each occupied scanline. A compact status draft uses 38 OAM entries and peaks
  at eight per line; it is not legibility-approved and cannot coexist with an
  80-character message under its current allocation.
- Verified: three boundary tests pass; a direct 246x15 `M_JKILL` source-patch
  rectangle fails at 62 OAM entries and 31 sprites per scanline; Markdown links
  pass (49 files, 589 links, zero broken). This is host arithmetic/source
  evidence only, with no new ROM, Mesen or physical sprite result.

## P3-U1..U6 -- sprite UI roadmap (2026-09-25)
- Planning only: explored engine UI/vpatch composition, host/device frame sinks,
  converter, bus/boot-ROM transport and the calibrated Mesen validation paths.
- Added plan 12 and I-20; promoted status/HUD/menu text, symbols, indicators and
  Doomguy face sprites from optional release polish to required M3-UI. Reordered
  the execution checklist while retaining incomplete performance/S2/save gates.
- Feasibility gates: real per-line sprite limits, physical non-streamed pattern
  storage, native layouts, asset/memory budgets and measured OAM/NMI scheduling.
  Specified fixed/dynamic UI co-simulation, independent composite comparisons,
  legacy regressions and NES-001 acceptance artifacts.
- Verified: `python3 doom/tools/check_md_links.py doom` -> 48 files, 584 links,
  zero broken; `git diff --check` -> clean.
- No firmware, engine, boot-ROM, emulator, test code or generated assets changed.
  No new emulator or hardware execution is claimed.

## HR-6 -- hardware shadow-detail and B-use milestone (2026-09-25)
- User confirms Doom works, contrast is much better and the B button works.
  USB serial is stable through 1,829 heartbeats at `count=15554`, with one startup
  DMA stop/resync and no later increase, zero timeouts/errors, and drops settled
  at 691. At 480 frames, conversion average/max is 38208/38251 us.
- Hardware-tested UF2: `/tmp/fcpico_doom_shadow_detail_delay_whx.uf2`, SHA-256
  `3d298001d93f028dd6a3e9f37287e8d77762704b0c1295485c8fd22275f88dbc`,
  8158 blocks, 287836 firmware bytes, 236452 bytes free before WHX.
- Two rebuilds omitted `FCPICO_DIAGNOSTIC_ENGINE_DELAY=ON` and failed hardware
  startup. Restoring the previously working profile succeeds; the specific timing
  or diagnostic dependency is not isolated. Both native picotool 2.1.1 and bundled
  Windows picotool 2.3.0 produce identical firmware UF2s from the restored ELF.
- Added `tools/build_shadow_detail.sh` to preserve the toolchain, board, startup
  diagnostics, embedded ROM, flash layout check and picotool/WHX packaging. It
  records build configuration and checksums outside `/tmp`; see [build guide](plan/08-build.md#hardware-milestone-build).
- Verified milestone commit: fresh script build reproduces the hardware-tested
  merged UF2 hash exactly; local bootstrap of pinned picotool succeeds; tutorial
  ROM MD5 and flash layout gates pass. Host CTest 10/10, focused Python tests
  259/259, and Markdown links pass.
- Remaining: precise geometry/chord/scene checks, longer stability, dynamic S2,
  converter optimization and later audio/save milestones. No broader release
  acceptance is implied. Details are in [the hardware log](HARDWARE-LOG.md).

## P2-T6 -- shadow detail video candidate (2026-09-25)
- The default now uses `$1D $00 $30` for gray and `$08 $18 $30` for brown,
  while retaining white in each subpalette. Its integer PLAYPAL shadow lift
  runs once while building the error and Bayer lookup tables; frame conversion
  and stream layout are unchanged. The earlier presets remain selectable.
- [Three-scene comparison and measurements](assets/palette_eval.md) show less
  sparse gray and more visible room geometry. A high-contrast dither penalty
  and removing shared white reduced HUD legibility; RGB interpolation would
  soften thin text and increase converter work.
- The rebuilt host engine emitted a v2 stream for DEMO1 frame 300; a mailbox
  decode produced [this candidate preview](assets/shadow_detail_frame300.png).
  Host and RP2350 engine builds passed. Firmware
  occupies 287812 bytes, leaving 236476 bytes before WHX. The merged UF2 is
  `/tmp/fcpico_doom_shadow_detail_whx.uf2` (8158 blocks), SHA-256
  `25796370ce0a6a8df662fc05710b0cdbf4fa300bfa6b9bd9053befacea7cd46a`.
- Subsequent hardware result: [HR-6](HARDWARE-REQUESTS.md#hr-6-shadow-detail-and-pending-bvideo-checks-task-p2-t6)
  confirms improved contrast, working B and stable playback; detailed geometry
  and scene checks remain. No new test suite was run for this visual exploration.

## P1-T4 / video follow-up -- B use and 224-line display candidate (2026-09-25)
- Commits: working tree after parent `b2bb6ef`; engine host-runner diagnostic is
  submodule commit `3c8af2bb`.
- Input: the B tap now posts use keydown for one poll and keyup on the next,
  so Doom samples the down state while building a tic command. The mapper
  regression failed before the change with an extra immediate keyup, then
  passed. The real host runner's scripted E1M1 B tap produced `use frames > 0`
  while an idle pad produced zero.
- Video: nearest-neighbour vertical scaling maps 200 Doom rows to 224 NES
  rows at lines 8..231. Both eight-line margins are explicitly zeroed in
  bitplane packing because they share attribute blocks with image rows. The
  reference converter uses the same source-row map. The whole-frame synthetic
  gradient PSNR is 12.55 dB under the new area weighting (previous 200-line
  layout: 13.02 dB); the updated acceptance floor is 12.5 dB.
- Verified: `cmake --build build-host && ctest --test-dir build-host
  --output-on-failure` -> 10/10 passed; `doom/.venv/bin/python -m pytest
  doom/tests/tools doom/tests/fcvideo -q` -> 259 passed; `python3
  doom/tests/input/test_engine_input.py /tmp/fcpico-bfix-engine/rp2040-doom/src/fcpico_doom_host
  doom/rp2040-doom/doom1.whx` -> passed; `doom/.venv/bin/python
  doom/tests/fcvideo/check_engine_stream.py
  /tmp/fcpico-bfix-engine/rp2040-doom/src/fcpico_doom_host
  doom/rp2040-doom/doom1.whx` -> frames 0, 100 and 101 byte-exact against
  the Python reference. `DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python
  doom/sim/mesen2/run_doom_frame.py /tmp/fcpico-bfix-stream/frame000100.bin
  --rom /tmp/fcpico-review/device/rp2040-doom/src/fcpico/doom_bootrom/doom.nes
  --frames 180 --output /tmp/fcpico-bfix-d1` -> 30 stable v2 heartbeats at
  count 15554, no DMA stops, exact palette/attributes, 0.9936 visible-picture
  correlation, and 30 NMI exits on scanline 255. `python3
  doom/tools/check_md_links.py doom` -> 45 files, 529 links, 0 broken.
- Device: `cmake --build /tmp/fcpico-review/device --target doom_tiny_fcpico
  -j 4` passed with GCC 13.2; `python3 doom/tools/flash_layout_check.py
  /tmp/fcpico-review/device/rp2040-doom/src/fcpico_doom.elf` passed with
  287772 firmware bytes and 236516 free before WHX. `whx2uf2.py` produced
  `/tmp/fcpico_doom_buse_scale224_whx.uf2` (8158 blocks), SHA-256
  `14160dd8f6301f4c41b12f6f39979a71f5a1f554799ac83284e598b1842d67b4`.
- Left out: B-use behavior, picture height and conversion timing on NES-001
  require [HR-5](HARDWARE-REQUESTS.md#hr-5-b-use-and-taller-video-task-p1-t4-and-video-follow-up).
  The previous 35.6 ms measurement belongs to the 200-line firmware, not this
  candidate. Work stops at this physical verification gate before audio.
- Plan changes: [04](plan/04-video.md), [05](plan/05-input.md), [10](plan/10-workplan.md),
  and D3 in [00](plan/00-overview.md) now describe the host-verified candidate.

## HR-4 -- physical input result and B-use diagnosis (2026-09-24)
- NES-001 input works for movement, strafing and menus. A B tap does not
  activate use. A host probe replayed a B tap through `--pads`: pad frames
  included press and release, the adapter emitted matching space keydown/up,
  but `G_BuildTiccmd()` never set `BT_USE`. Both events arrive before it reads
  the final key state. The probe was removed after diagnosis; firmware fixes
  await the requested advice on the display layout.
- The supplied photograph's lower gap matches the planned 200-line Doom image
  at NES lines 16..215, leaving 24 blank lines beneath. The 256x240 output
  stream has room to scale the image vertically; see [`HARDWARE-LOG.md`](HARDWARE-LOG.md).
- Serial remained stable through `hb=2221` at `count=15554` with no new DMA
  stops or resyncs. Conversion average/max at 540 frames was 35574/35612 us.

## P1-T4 -- NES pad input wired to Doom (2026-09-24)
- The RP2350 bus now retains up to 31 consecutive pad-1 snapshots from v2
  heartbeats. `I_StartTic()` drains them into the existing mapper and posts
  Doom key events; short presses between tics survive. The host runner accepts
  one numeric pad byte per tic via `--pads`, and `--warp` starts a level for
  reproducible movement checks. Start opens the menu and A/B map to menu
  accept/back while it is active.
- Verified: GCC 13.2 RP2350 `doom_tiny_fcpico` build and flash layout check
  passed (287724 firmware bytes, 236564 free before WHX). Host CTest 10/10;
  engine host CTest 4/4, including an E1M1 scripted Up run whose player
  position differs from an idle run. The merged input UF2 is
  `/tmp/fcpico-hw-doom/fcpico_doom_input_whx.uf2` (8157 blocks), SHA-256
  `82405a0823193a0d7ea7c6b8f5c053450e7d049399fa23d1d7f21973caf5fbd8`.
- Left out: physical pad check ([HR-4](HARDWARE-REQUESTS.md#hr-4-controller-input-on-doom-task-p1-t4)), pad 2, and context-specific automap
  controls. The measured 35.6 ms conversion time still exceeds the plan's
  performance target and needs a separate optimization pass.

## HR-3 -- Doom visible on NES-001 (2026-09-24)
- Hardware result: the user reports Doom playing back on screen with the
  stream-fix/BOOTSEL image. Serial showed stable v2 count 15554 from heartbeat
  20 through 385, converted frames rising from 6 to 120, and no additional
  DMA stops or resyncs after the initial one of each. Timeouts and protocol
  errors remained zero. The 2020 dropped frames accrued around startup and
  did not grow during the captured steady interval.
- Measurements: `conversion_us=35579/35614` average/max at 120 converted
  frames. Image quality, longer stability, and physical controller input have
  not yet been reported; details are in [`HARDWARE-LOG.md`](HARDWARE-LOG.md).

## HR-3 -- USB serial BOOTSEL command in Doom firmware (2026-09-24)
- The device main core now accepts `bootsel` followed by Enter on USB serial and
  calls the RP2350 ROM USB-boot reset. It polls during the 30-second diagnostic
  delay and at the end of each rendered frame, so later UF2 updates can start
  with the cartridge installed.
- Verified: `cmake --build /tmp/fcpico-review/device --target doom_tiny_fcpico -j 4`
  passed with GCC 13.2; `flash_layout_check.py` passed with 286516 firmware bytes
  and 237772 bytes free before the WHX address. `whx2uf2.py` produced the merged
  candidate at `/tmp/fcpico-hw-doom/fcpico_doom_streamfix_delay_bootsel_whx.uf2`
  (8153 blocks), SHA-256
  `1221d341ac281b28cdd0712410aefa7b91b3fab99e7f0a15c76ad8e07e593127`.
- Left out: physical USB command validation; [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6) hardware validation is pending.

## HR-3 / I-14 / I-15 -- pre-hardware review: stream selection and PPU address fixes (2026-09-24)
- Findings: the Doom setup filled nametable 0 with tile `$00`, unlike the working
  tutorial's `$80`. This targets `$0000` instead of the `$0800` stream port. The
  previous Mesen mapper selected all `$0000-$0FFF` reads and discarded fetches at
  specific cycles to force the measured count, hiding the setup error. Selecting
  `(addr & $F800) == $0800`, with no cycle filtering, reproduces the existing
  hardware failure exactly: black screen, 129 physical mailbox reads / count=128,
  zero selected rendering reads, and one DMA stop per heartbeat. The same decode
  naturally reproduces the tutorial's measured 66/64 cadence and existing S0 golden.
- Second finding: correcting tiles alone yields count=15550 (62 pre-render reads),
  still outside the v2 sync window. The three-byte heartbeat leaves the PPU's current
  address at `$0803`; `$2005` updates temporary scroll state, so it does not undo this.
  Restoring `$2006` to `$0801` after the packet restores the tutorial's pre-render phase,
  producing count=15554 without changing any protocol/count constant.
- Changes: setup uses tile `$80` in nametable 0, clears attributes and adjacent nametable
  1 to zero, and the NMI restores `$0801` before control/scroll registers. ROM stamp is
  `20DOOM-02-0002` so the fixed bank installs the change. Mesen now decodes addresses;
  wider masks require diagnostic mode. The Doom runner checks every completed steady
  heartbeat for the exact count and no new DMA stops, including mailbox-only failures.
- Regression loop: `doom/.venv/bin/python -m pytest doom/tests/bootrom/test_doom_setup.py -q`
  failed on the old assembled ROM (`background fetches ... [0]`), then passed after the
  fix. The heartbeat-order test also failed before the address restore. With the rebuilt
  mapper, `run_doom_frame.py ... --rom /tmp/fcpico-review/before/doom.nes` now fails with
  `count=128, expected=15554, new DMA stops=1`; the intermediate tile-only run recorded
  count=15550. Captures/logs are under `/tmp/fcpico-review/`.
- Verified: pinned NESASM CE tutorial MD5 gate passed; unchanged fix-bank check passed.
  `cmake --build build-host && ctest --test-dir build-host --output-on-failure` -> 10/10.
  `doom/.venv/bin/python -m pytest doom/tests doom/sim/pioemu -q` -> 386 passed, 1 skipped.
  New PIO tests confirm the raw diagnostic counts both selected and unselected `/RD`
  pulses once, while the existing counter counts only selected pulses; both report N-1.
  The earlier heartbeat sampling and ROM-tail changes remain covered by host tests.
- Co-simulation: `DOTNET_ROOT=/home/josh/.dotnet doom/sim/mesen2/run_scenario.sh S0
  --frames 180 --output /tmp/fcpico-review/S0` -> strict pass, count=15490, 15426 selected
  rendering reads, unchanged screenshot golden. Real host DEMO1 frame 100 through D0/D1
  -> correlation 0.9942, exact palette/attributes/mailbox. S1 tutorial -> `0002`, broken
  Doom `0001` -> `0002`, and recovery `0002` -> tutorial each passed 3000-frame runs with
  byte-exact final PRG and 30 stable final heartbeats. v2 count=15554, v1 count=15490;
  no steady DMA stops. v2 NMI exits at scanline 255; py65 worst case 1614 critical /
  2021 total cycles, below 1900 / 2200 limits.
- Device: GCC 13.2 diagnostic and recovery builds passed; 286108 firmware flash bytes,
  238180 bytes free before WHX, 36052 post-zone RAM bytes (minimum 24576). Merged UF2:
  `/tmp/fcpico-hw-doom/fcpico_doom_streamfix_delay_whx.uf2`, 8151 blocks, SHA-256
  `17f60cda5ea6905a9885bb39796156a66da7721c4f62906cd597f329b12127c7`.
  Reconstructed UF2 payload contains the exact assembled `0002` ROM and byte-exact WHX.
  It retains the 30-second delay, raw counter, heartbeat fix, ROM tail and double support.
- Limits: the address decode is supported by tutorial code, measured cadence and exact
  reproduction of the hardware symptom; this is not a new electrical measurement or a
  hardware validation of the fixed ROM. Physical boot/display and dynamic frame sequences
  remain unverified. The old raw-count-only request is superseded in [`HARDWARE-REQUESTS.md`](HARDWARE-REQUESTS.md).

## P2-T4 / P1-T6 -- S1 reflash co-simulation, stamp fix, first Doom hardware UF2 (2026-09-23)
- What landed: the Mesen FC PICO mapper now emulates the cartridge PRG flash (JEDEC
  unlock on A10-A0, 4 KiB sector erase, bit-clearing program, a short DQ7/DQ6/DQ3
  status phase that the fix bank polls). The cart model keeps its own copy of the
  served ROM (`FCPICO_SERVE_ROM` may name a different one) and, like the device,
  defaults to protocol v2 when serving the Doom ROM. `run_doom_frame.py --serve-rom
  --frames` runs S1 and byte-compares the console's final PRG with the served image.
  `tools/whx2uf2.py` merges `doom1.whx` at `0x10080000` into the firmware UF2.
- Bug found: the fix bank's `CHK_ROMVER` (`$F2B2` in `bootrom_fixr.bin`) reflashes after
  five mismatches only if the cartridge stamp starts with `"20"`; otherwise it prints
  PICO NOT FOUND and halts. The previous `DOOM-02-000001` stamp would never have
  reflashed a tutorial console; S1 reproduces that (PRG unchanged, 4,703 bytes differ).
  The stamp is now `20DOOM-02-0001`, a bootrom test pins the prefix, and plan 03's
  claim that the check does not exist is corrected.
- Verified: S1 `run_doom_frame.py frame000100.bin --rom tutorial rom.NES --serve-rom
  doom.nes --frames 3000` -> console PRG equals `doom.nes`; correlation 0.9942;
  16/16 palette, 64/64 attributes and 128-byte mailbox equal; 30 NMI exits on
  scanline 255. Reverse recovery (Doom console, tutorial served) -> PRG equals
  `rom.NES`. D0, D1 and strict S0 still pass. Host CTest 10/10; pytest 373 passed,
  1 skipped; bootrom 24 passed; protocol and link checks clean. GCC 13.2 device ELF
  uses 282,488 flash bytes (241,800 free before WHX). Merged UF2
  `/tmp/fcpico-hw-doom/fcpico_doom_whx.uf2` (8,137 blocks) is [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6).
- Left out: timing of the real flash chip (the model completes erase/program
  immediately after a short status phase); hardware boot, display and conversion
  timing are [HR-3](HARDWARE-REQUESTS.md#hr-3-first-doom-boot-v2-reflash-and-engine-display-tasks-p2-t4-p1-t3-p1-t6). Dynamic multi-frame co-simulation, input and sound remain open.

## I-13 / I-14 / I-15 -- v2 console ROM and fixed-frame D1 gate (2026-09-23)
- What landed: a display-first 32 KB Doom v2 NES ROM with the unchanged fix
  bank, 128-byte mailbox NMI, per-frame palette and attribute writes, explicit
  controller heartbeat, and 15-pair APU replay. Native NESASM CE at commit
  `6fc41cda` builds it; normalization of two NES 2.0 header bytes makes a
  tutorial reassembly reproduce MD5 `b6cd675342b6c8ad79e537e2c9860579`.
  The device build now embeds the v2 ROM, and a dedicated boot-ROM workflow
  assembles it twice, checks the fix bank, and runs the py65 tests.
- Verified: worst-case py65 NMI uses 1602 critical / 2009 total cycles
  (limits 1900 / 2200), 129 `$2007` reads, and all 15 APU writes. D1 Mesen
  frame 100 has 0.9942 visible correlation; 16/16 palette, 64/64 attributes,
  and the normalized 128-byte mailbox agree. 30 post-startup NMI exits occur
  on scanline 255. New RP2350 ELF/UF2 links with the v2 ROM, uses 282,488
  firmware bytes (241,800 before WHX), and leaves 36,368 post-zone RAM bytes.
- Left out: S1 erase/reflash cannot be measured with the current Mesen mapper,
  which does not emulate PRG flash writes. D1 replays a fixed frame; dynamic
  frame sequences, pad 2, v1 bulk data mode in the new ROM, hardware boot,
  conversion timing and sound production remain open. Do not flash this UF2
  without an explicit hardware reflash procedure and recovery plan.

## I-17 / P1-T3 -- Doom picture in Mesen, device publication candidate (2026-09-23)
- What landed: D0 replays an actual DEMO1 frame through the tutorial-ROM Mesen PPU.
  The screenshot agrees with the independent PPU reconstruction, and console
  palette/attribute RAM equals the stream mailbox byte-for-byte. A v1 32-byte
  bulk palette upload now mirrors its second half so the NES palette aliases do
  not overwrite the backdrop; Mesen's attribute bulk response accounts for the
  tutorial ROM's extra dummy read. The RP2350 adapter stages scanlines directly
  into `fcvideo`, publishes converted frames to `fcbus`, and embeds the tutorial
  ROM. The UF2 is a candidate, not yet hardware-approved.
- Verified: D0 frame 100 visible correlation 0.9942 and 16/16 palette plus
  64/64 attribute bytes equal; screenshot under `/tmp/fcpico-d0-final/run/`.
  Strict S0 is green with a newly reviewed black-backdrop golden, 30/30 valid
  post-startup mailboxes, count 15490 and no post-startup DMA stops. The staged
  converter matches the bulk API's stream exactly. Host 128 KB zone replay:
  3/3 CTests including two 600-frame runs. RP2350 ELF/UF2 links; flash uses
  282,500 bytes, leaving 241,788 before WHX; static RAM leaves 36,368 bytes
  after the configured 128 KB Doom zone (24 KB minimum gate).
- Left out: hardware boot/display and measured conversion timing; `doom1.whx`
  is not in the UF2 and still needs loading at `0x10080000`. D0 replays a fixed
  stream through tutorial v1, not the Doom v2 ROM. S1 reflash, full S2 sequence,
  palette/attribute updates per dynamic frame and v2 NMI timing remain open.

## I-12 / I-17 -- host Doom-to-NES stream output (2026-09-23)
- What landed: the SDL-free host runner's `--dump-stream DIR` converts each composed
  320x200 indexed frame to a 17,408-byte v2 cartridge stream using the default
  shared-white preset, the actual WHX PLAYPAL, attribute hysteresis and the current
  engine palette-flash set. Dumps are named `frame000000.bin`, etc.
- Verified: two 600-frame DEMO1 runs produce identical indexed frames and identical
  streams (CTest `engine_host_determinism`). The independent Python oracle matches
  C output byte-for-byte on real frames 0, 100 and 101, preserving the previous
  attribute table across all intervening frames. A v2-mailbox NES PPU-model decode
  of frame 100 shows the game view and status bar at 256x240; preview:
  `/tmp/fcpico-doom-nes-frame100.png`.
- Left out: device bus publication, measured conversion time, controller pad input,
  and Mesen S2. The current Mesen runner supports tutorial-ROM S0 only, so the
  preview is not yet an emulated-console acceptance result.

## I-16 / P1-T1 -- indexed full-frame composition (2026-09-23)
- What landed: the FC PICO adapter now composes all 320x200 scanlines from the renderer's
  view buffers and packed overlay list, including title/menu/HUD/status patches and the
  melt-wipe state. It sends lines through `fcvideo_line_sink`; the host sink writes full
  raw frames and PLAYPAL-indexed PNG previews every 100th frame. The device build also
  executes composition and launches the renderer's core-1 worker, but its sink remains a
  no-op until stream conversion/publication is integrated.
- Verified: SDL-free host CTest 3/3, including two identical 600-frame captures, 60 golden
  hashes, six PNG previews, and separate title/menu goldens. Visual samples show the title,
  menu, E1M1 status bar and wipe. Python suite: 365 passed, 1 skipped. GCC 13.2 RP2350
  engine ELF links at 242,612 flash bytes, leaving 281,676 before the WHX partition.
  A 600-frame AddressSanitizer replay passes after correcting the host runner's one-byte
  `singletics` type and the renderer's visplane-row lookahead. UBSan still reports 133
  warnings in existing engine arithmetic/indexing paths; none are in the new compositor.
- Left out: independent Chocolate Doom pixel comparison, hardware display, and NES stream
  output. Preview captures are under `/tmp/fcpico-fullframe-b/`, title under
  `/tmp/fcpico-title-frames-current/`, and menu under `/tmp/fcpico-menu-frames/`.

## I-12 / P0-T3 -- SDL-free host engine runner (2026-09-23)
- What landed: `PICO_PLATFORM=host` now builds the whole FC PICO engine and pthread shim
  without SDL. The runner reads the checked-in WHX by path, starts DEMO1, captures indexed
  320x168 view frames and stops at `--frames`. The host render barrier and wipe progression
  are lockstep; no scanline thread is required for this intermediate view capture.
- Verified: default host `cmake --build /tmp/fcpico-engine-host-12 -j 4` passes;
  `ctest --test-dir /tmp/fcpico-engine-host-12 --output-on-failure` -> 2/2, including
  two byte-identical, changing 600-frame DEMO1 captures. GCC 13.2 RP2350 engine ELF and
  test-pattern targets still link. Raw output from manual runs is under
  `/tmp/fcpico-host-frames-a/` and `-b/` (600 files each, 53,760 bytes per view).
- Left out: the raw view omits status/menu/wipe composition. `--dump-stream` and `--pads`
  report unsupported until [I-16](issues/I-16-stage-a-composition.md)/[I-17](issues/I-17-fcvideo-impl.md) and the input adapter land; [I-12](issues/I-12-engine-host-build.md) acceptance remains
  partial. This is not yet a co-simulation-ready Doom video stream.

## I-11 / P0-T2 -- RP2350 engine platform skeleton (2026-09-23)
- Commits: engine submodule `0c27c1ce` and this parent integration commit.
- What landed: optional parent superbuild integration, isolated `common_fcpico` platform
  sources, a silent/no-display engine adapter, a USB serial bring-up marker, and a
  `doom_tiny_fcpico` target producing `fcpico_doom.elf`. The FC PICO build omits VGA/I2S
  libraries; shared engine changes only guard behavior that assumed Pico networking,
  ENDOOM or the VGA audio module. The device bus target now compiles its Pico SDK sources
  once per firmware, which also preserves the test-pattern build.
- Verified: GCC 13.2.Rel1 `cmake --build /tmp/fcpico-engine-device-13 --target
  doom_tiny_fcpico -j 4` links; `flash_layout_check.py` passes with 237,896 bytes used
  and 286,392 free before WHX. The existing RP2350 test-pattern target rebuilds, all
  10 host C suites pass, and standalone Pico host engine configuration succeeds.
- Left out: on-console boot to `D_DoomMain` is unverified. The adapters do not yet
  initialise the cartridge bus, compose 8-bit frames, read controllers or play sound.
  Native `chocolate-doom` configuration needs SDL2 development packages not installed here.

## I-19 / P0-T9 / P0-T10 / P0-T11 -- hardware-calibrated PPU path (2026-09-22)
- Commits: Mesen submodule calibration commit and this commit.
- Hardware evidence: complete NES-001 trace 6 decodes to 66 selected reads on pre-render
  and 64 on each of 14 complete visible lines. `/RD` low widths are 160-200 ns for 2,705
  of 2,706 pulses. Traces 4 and 5 are incomplete by 28 and 21 packed words and are retained
  only as diagnostics.
- Explanation: `66 + 240*64 + 65 NMI reads = 15491` physical reads; `fcppu_rna` captures
  `!X` before decrementing X and therefore reports the zero-based index 15490. The original
  `convVram()` source index is flat, so its 34-iteration batches are not 34-word physical
  line strides; hardware advances 32 words per visible line. A normal re-arm has no extra
  four-zero-byte stream prefix.
- What landed: trace-aware decoder segmentation and fixture regression, calibrated L3 model,
  linear 32-word C/Python/test-pattern stream conversion, zero-based host counter emulation,
  hardware-timed Mesen selection, reviewed S0 ARGB golden, and the missing Mesen static-link
  dependency on the shared test-pattern controller.
- Verified: `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` -> 365 passed,
  1 skipped; host C 10/10; generated protocol check and 51-document link check pass.
  `run_scenario.sh S0 --frames 180` passes strict S0 twice with
  `counts_after_startup=[15490]`, 30/30 valid mailboxes, zero post-startup DMA stops,
  15,426 selected rendering reads and identical debugger-peek traces/screenshots. The
  RP2350 test-pattern UF2 builds and passes the flash-layout check (68,564 bytes used,
  455,724 free); SHA-256 `a1f129761e70c0c3f6aa65b02e7e69cfe9e93cd244da08ddcb1bc66d25d7b2e9`.
- Left out: the corrected stream geometry needs the replacement hardware UF2 visual check;
  engine frame composition/publication remains [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md)/[I-16](issues/I-16-stage-a-composition.md)/[I-17](issues/I-17-fcvideo-impl.md).

## I-10 / I-19 -- first NTSC measurement and test-pattern init repair (2026-09-22)
- Commits: this commit.
- What landed: the first NES-001 run validates the existing v1 hardware count at 15490
  (5495/5507 exact frames; 12 low outliers). The blank screen exposed a separate adapter
  divergence: Mesen reacted to `FP_COM_INI` with `PF_COM_DMOD`, while the physical main loop
  could not observe the init after the IRQ backend drained it. A shared backend-neutral
  test-pattern controller now owns pattern/table publication and init-time data mode for
  both adapters. Init count/stage are retained in stats and printed by the CLI.
- Capture changes: the initial three 65,536-word traces were incomplete over USB/terminal
  capture. The sampler now emits 8,192 words, about 2.0 ms or 31 NTSC scanlines, retaining
  the required within-line prefetch evidence while reducing text output below 256 KiB.
- Verified: focused red-to-green checks produced `1332` fcbus checks, `7` test-pattern
  checks and `149` cartmodel checks; all 10 host C suites pass. The RP2350 UF2 builds and
  `flash_layout_check.py` reports 68,564 bytes used with 455,724 bytes free.
- Hardware follow-up: the replacement UF2 displays its test patterns successfully on the
  same NES-001, confirming the init-event diagnosis and completing [I-10](issues/I-10-testpattern-firmware.md). Three decoder-valid
  traces remain [I-19](issues/I-19-hw-trace.md)'s last hardware input. Strict Mesen S0 remains intentionally
  uncalibrated until those traces determine which strobes the PIO counter omits.
- Plan changes: [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9), [I-10](issues/I-10-testpattern-firmware.md), the hardware log, and the new hardware fixture directory record
  the session and exact artifact.

## I-17 -- NES presets, PLAYPAL tables and flash sets (2026-09-21)
- Commits: `21aa70e`, this commit.
- What landed: build-selectable default and named A/B/C sub-palette presets, including the
  PiPU set, plus allocation-free C generation of the 1 KB error table, 16 KB dither LUT,
  and 16-byte NES palette from PLAYPAL palette 0. The RGB palette is the same NESDev example
  table used by the Python reference. It is called once at setup, not in the frame/ISR path.
  All 14 mailbox palette sets match the engine's integer red/yellow/green tint arithmetic;
  set 0 is the exact preset, and switching sets does not rebuild `err`/`lut`.
- Verified: `ctest --test-dir /tmp/fcpico-video-host --output-on-failure` -> 9/9 passed;
  `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/fcpico-video-asan --output-on-failure`
  -> 9/9 passed; `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` ->
  365 passed, 1 skipped.
  New differential cases compare all 3 preset tables to `fcvideo_ref.py` and compare a full
  converted frame using C-generated tables to the Python stream oracle. Further cases cover
  all 3 presets and 14 flash sets.
- Left out: engine frame input, device publication and timing, and real
  Mesen pixels remain open. Strict S0 is still gated by [I-19](issues/I-19-hw-trace.md)'s hardware count calibration.

## I-17 -- host video stream core (2026-09-21)
- Commits: this commit.
- What landed: a caller-owned pure-C converter for 320x200 index frames. It decimates to
  256 columns, letterboxes to 240 lines, chooses 16x16 sub-palettes with 12% hysteresis,
  dithers through supplied lookup tables, packs the exact 34-word/line stream including
  next-line prefetch, and writes the v2 palette/attribute mailbox. The tables are supplied
  by the caller; preset/PLAYPAL table generation remains to be implemented.
- Verified from the repository root:

  ```
  cmake --build build-host && ctest --test-dir build-host --output-on-failure
  # 9/9 passed
  cmake --build build-asan && ASAN_OPTIONS=detect_leaks=0 \
    ctest --test-dir build-asan --output-on-failure
  # 9/9 passed
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 358 passed, 1 skipped
  ```

- The differential Python test compiles the same C source as a host shared library and
  compares complete streams and attributes against `fcvideo_ref.py` for gradient,
  checkerboard and random frames, plus a repeated frame with hysteresis and a palette flash.
  Its 34-word/line layout matches the independently measured Mesen fetch-order gate below.
- Left out: device timing/publication, engine frame composition, table generation and
  Mesen pixel goldens. S0 DMA remains stopped pending [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); no displayed-pixel
  correctness claim is made from its current open-bus screenshot.

## I-15 -- Mesen PPU fetch-order gate and display-first order (2026-09-21)
- Commits: Mesen fork change and this parent commit.
- What landed: an optional per-frame PPU rendering fetch trace in the Mesen mapper and a
  default frame-121 trace in the S0 runner. The runner validates the scanline/cycle pattern,
  low/high bitplane address pairs and sprite-read count, then checks that debugger peeks do
  not change the trace. The workplan and issue index now put PPU emulation and host video
  conversion ahead of the remaining audio assets.
- Verified from the repository root, using the pinned local .NET/SDL toolchain:

  ```
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" doom/sim/mesen2/build.sh
  # 8/8 C tests pass; MesenCE builds
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180
  # exit 0; deterministic; 241 x 68 = 16388 background reads
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180 --cs1-mask 0xe000
  # exit 0; deterministic; 16388 background + 3856 sprite = 20244 reads
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 357 passed, 1 skipped
  python3 doom/tools/check_md_links.py doom
  # 50 files, 38 links, 0 broken
  python3 doom/tools/gen_protocol.py --check
  # exit 0
  ```

- Both fetch traces contain `$FF` for every rendering read: the measured heartbeat counts
  are still 16453 and 20309 against v1's 15490. Strict S0 therefore remains failing, and
  neither trace is a pixel golden or a hardware calibration. [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) must settle the
  counter before the display path can make those claims.
- Next: implement [I-17](issues/I-17-fcvideo-impl.md)'s host converter stages B-E against the Python reference, then feed
  engine frames through it after [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md)/[I-16](issues/I-16-stage-a-composition.md). Keep co-simulation as the picture gate.

## I-04 / P4-T1 -- host APU sequencer (2026-09-21)
- Commits: this commit.
- What landed: an allocation-free APUS reader with loop points inside silence runs,
  music and three SFX voices, priority-based voice replacement, DPCM triggers, pulse/noise
  steals and restoration, pause/resume, four-level music volume, the note-retrigger rule,
  and a 64-entry deferred write queue. A caller-supplied callback owns mailbox writes and
  its terminator. The sequencer limits itself to `APU_PAIRS_MAX_V2` pairs per heartbeat.
- Verified from the repository root:

  ```
  cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON
  cmake --build build-host
  ctest --test-dir build-host --output-on-failure
  # 8/8 passed, including test_sequencer (179 checks)
  cmake -S doom -B build-asan -G Ninja -DFCPICO_HOST_ONLY=ON \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g"
  cmake --build build-asan
  ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
  # 8/8 passed
  ```

- The unmodified sanitizer command ran every test body successfully but LeakSanitizer
  failed at process exit under this environment's ptrace restriction. Disabling only leak
  detection gave the clean ASan/UBSan run above; this module performs no allocation.
- Left out: engine sound/music adapters and asset conversion remain [P4-T2](plan/10-workplan.md#p4-t2-engine-soundmusic-modules----spec-06-api-table-size-m-depends-p4-t1-p1-t5)/[P4-T3](plan/10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap). No
  device or listening claim is made here.

## I-08 / I-09 / I-10 -- resumed acceptance verification (2026-09-21)
- Commits: this commit; resumed verification and the device-map artifact fix.
- Acceptance: finish the interrupted host, Python, generated-file, link and workflow checks,
  and verify the device build produces the map at the workflow's corrected path.
- Verified from the repository root:

  ```
  cmake -S doom -B /tmp/bh -G Ninja -DFCPICO_HOST_ONLY=ON -DFCPICO_BUILD_COSIM=ON
  cmake --build /tmp/bh
  ctest --test-dir /tmp/bh --output-on-failure
  # 100% tests passed, 0 tests failed out of 7
  doom/.venv/bin/python -m pytest doom/tests doom/sim -q
  # 352 passed, 1 skipped in 3.59s
  python3 doom/tools/gen_protocol.py --check
  # exit 0
  python3 doom/tools/check_md_links.py doom
  # 50 file(s), 38 link(s) checked, 0 broken
  diff doom/ci/workflows/doom-device.yml .github/workflows/doom-device.yml
  # exit 0, identical
  ```

- Device verification, using the existing local picotool installation to avoid the SDK's
  network fetch (the first configure attempt failed because GitHub DNS was unavailable):

  ```
  PATH="$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH" \
  PICO_SDK_PATH="$HOME/.local/fcpico/pico-sdk" \
  cmake -S doom -B /tmp/fcpico-doom-device-resume -G Ninja \
      -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s \
      -Dpicotool_DIR=/tmp/fc-picotool-2.1.1/picotool
  cmake --build /tmp/fcpico-doom-device-resume --target fcpico_testpattern
  python3 doom/tools/flash_layout_check.py /tmp/fcpico-doom-device-resume/port/fcpico_testpattern.elf
  ```

  -> configure and all 125 build steps passed; UF2 and
  `/tmp/fcpico-doom-device-resume/fcpico_testpattern.elf.map` exist.
- Measurements: 64880 bytes, 87.6% flash budget free; `text 64880, data 0, bss 301868`,
  matching the earlier independent builds.
- Documentation: corrected the CI README introduction to acknowledge both active lanes.
- Left out: hardware acceptance still needs [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); strict S0 remains unresolved.
  [I-04](issues/I-04-fcapu-core.md) remains the next independent implementation task.

## I-15 -- co-simulation diagnostic histogram fix (2026-09-21)
- Commits: this commit.
- What changed: the S0 runner now filters its steady-state histogram to v1 heartbeat rows
  (65 CPU reads and a nonzero rendering-read count). Mesen logs ordinary CPU writes and
  startup transitions in the same trace, which previously polluted the histogram with
  zero and cumulative counter values.
- Verified:

  ```
  PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet" \
    doom/sim/mesen2/run_scenario.sh S0 --diagnostic --frames 180
  ```

  -> deterministic diagnostic run; the steady histogram is `[16453]` for the `$F000` CS1
  decode, while DMA stops remain as expected because the hardware count discrepancy is not
  resolved. The focused regression test and full suite pass: `353 passed, 1 skipped`.
- A second diagnostic with `--cs1-mask 0xe000` reports `[20309]`, with 20,244 rendering
  reads and the same 65 CPU mailbox reads. Both masks remain deterministic under the
  per-frame debugger-peek run; neither can produce the firmware's expected 15,490 count.
- Left out: strict S0 remains gated on [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md); no protocol or count constant was changed.

## I-08 / I-09 / I-10 -- device build acceptance (2026-09-20)
- Commits: this commit (build fix and records); the implementation landed earlier in
  `201ce2d` and `c9c4e12`.
- Verified, with `arm-gnu-toolchain-13.2.Rel1` from the repository root and pico-sdk 2.1.1:

  ```
  export PATH="$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH"
  cmake -S doom -B build-rp2350 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DPICO_SDK_PATH=<pico-sdk 2.1.1> -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s
  cmake --build build-rp2350 --target fcpico_testpattern
  python3 doom/tools/flash_layout_check.py build-rp2350/port/fcpico_testpattern.elf
  ```

  -> clean configure and build from an empty directory, `-Wall -Wextra -Werror`, and
  `fcpico_testpattern.elf/.bin/.hex/.uf2` plus `fcpico_testpattern.elf.map` produced.
  The host lane is unaffected: 7/7 ctests and 352 passed / 1 skipped as before.
- Measurements: firmware 64880 bytes of the 524288-byte budget below `FLASH_WHX_ADDR`,
  87.6% free; `text 64880, data 0, bss 301868`. Identical across two independent clean
  builds.
- Defect found and fixed: `port/CMakeLists.txt` set `-Wl,-Map` through `LINK_FLAGS`, but
  `pico_add_extra_outputs()` appends its own `-Map` afterwards and GNU ld honours the last
  one. The requested `port/fcpico_testpattern.map` was therefore never written, and the
  device workflow had been uploading an artifact path that could not exist. The redundant
  flag is gone and both copies of `doom-device.yml` now name the file the SDK actually
  writes, `build-rp2350/fcpico_testpattern.elf.map`.
- Correction to the status reconciliation entry below: it records that "the installed ARM
  compiler is 10.3.1, not the required 13.2.Rel1" and that "the device workflow exists only
  at `ci/workflows/doom-device.yml`". Both are wrong. `arm-gnu-toolchain-13.2.Rel1` has been
  in the repository root all along (it is covered by the root `.gitignore`, which is why a
  tree listing does not show it), `/usr/bin/arm-none-eabi-gcc` 10.3.1 is merely what is on
  `PATH`, and `.github/workflows/doom-device.yml` is present and byte-identical to its
  template. **Check the repository root before concluding a tool is missing.**
- Left out: this is build acceptance only. Nothing here has run on an RP2350 or a Famicom,
  so [I-08](issues/I-08-device-superbuild.md), [I-09](issues/I-09-fcbus-device.md) and [I-10](issues/I-10-testpattern-firmware.md) stay short of their hardware-facing claims and [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) still
  gates the bus model. The device lane was already active in `.github/workflows`.
- Plan changes: `port/CMakeLists.txt`, both `doom-device.yml` copies, [`ci/README.md`](ci/README.md),
  [`issues/README.md`](issues/README.md).

## P0-T11 / I-15 -- co-simulation runs, and it contradicts the plan (2026-09-20)
- Commits: `87862d0` (scope), this commit; fork `grebz-dev/MesenCE-FC-PICO` at `5c2de02e`.
- Verified: `doom/sim/mesen2/build.sh` -> MesenCE built with the FC PICO mapper, host
  cartmodel 7/7 ctests passed; `doom/sim/mesen2/run_scenario.sh S0 --diagnostic` -> exit 0,
  `debug_peeks_do_not_change_results: true`; `doom/sim/mesen2/run_scenario.sh S0` -> exit 1
  with the four strict errors listed below. Re-run a third time from a clean output
  directory: byte-identical hashes.
- Measurements (steady state, frames 121-300, one heartbeat per frame, 170 of 179
  post-startup heartbeats identical):

  | quantity | `$0000`-`$0FFF` decode | `$0000`-`$1FFF` decode |
  |---|---|---|
  | selected PPU reads per frame | 16388 = 241 x 68 | 20244 = 241 x 84 |
  | `$2007` reads per frame | 65 | 65 |
  | reported count | 16453 | 20309 |
  | expected `PPU_COUNT_VAL_V1` | 15490 | 15490 |
  | heartbeats / DMA stops | 2220 / 2220 | 4264 / 4264 |

  Run time: 3.5 s for the pair of determinism runs, against a < 2 min target. The MesenCE
  build is the whole cost of the lane.
- What this settles. The bridge is real: the unmodified tutorial PRG boots, `FP_COM_INI` is
  answered over the tutorial's own `PF_COM_DMOD` -> `DRQ` -> `DLD` bulk transfer, and
  debugger peeks across `$0000`-`$2FFF` on every frame change nothing, so the mapper's
  read predicate is correct. The `$2007` side matches the specification exactly: 65 reads =
  the 64-byte mailbox (`FC_COM_BUF_SIZE_V1`) plus the buffered-read dummy, which is also
  what the boot-ROM cycle harness measured independently.
- What this breaks. `PPU_PICTURE_COUNT` = 15426 cannot be the number of CS1-qualified PPU
  reads in a frame under any decode: widening the mask adds sprite fetches (16 per line,
  which is how the second column proves the tutorial's sprite pattern table is `$1000`),
  and narrowing it cannot remove background fetches without removing the picture. 68 is
  32 visible tiles plus the 2 tiles prefetched for the next line, 2 bytes each; 241 is 240
  visible lines plus the pre-render line. 15426 is exactly `241 x 64 + 2`, the same 241
  lines counting only the in-picture tiles, with the `+2` inside `PPU_COUNT_WINDOW`. So the
  counter constant and the 34-word stream line disagree by precisely the 4 prefetch bytes
  per line: 964 per frame, against an observed 962. This is the September 11 prior-art
  contradiction (`241 x 68 = 16388` consumed bytes versus `15426` counted picture reads)
  with a number attached.
- Two of the three candidate explanations in `plan/01` can now be closed off the bench, which
  is recorded as a sharpened [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9) rather than as an edit to `plan/01` ([I-19](issues/I-19-hw-trace.md) owns that file).
  The counter's qualifier is not in doubt: the tutorial configures `fcppu_rna` with
  `sm_config_set_jmp_pin(&cn, PI_CS1_BIT)`, so the count is "CS1-low reads per frame" exactly.
  And the cheap CS1 hypothesis is dead as an explanation of the count: a narrow `$0000`-`$0FFF`
  decode is still needed for the 68-byte line layout, and the `$0000`-`$1FFF` column proves the
  sprite fetches really are at `$1000`, but no address mask reaches 15426. What remains is
  whether the PIO counter misses the dots 321-336 prefetch strobes, or the frame is not
  consumed as 241 lines -- and one trace covering dots 241-340 of a single line distinguishes
  them.
- Left out, deliberately: no protocol constant was changed and no read was discarded to make
  the count check pass. Consequently the DMA stops on every heartbeat, every selected read
  returns open bus, and S0's screenshot is a white screen -- so no screenshot golden was
  frozen either. Strict S0 stays failing until [I-19](issues/I-19-hw-trace.md) answers the counter question.
  Doom-mode cartridge and scenarios S1-S6 remain future work behind [I-12](issues/I-12-engine-host-build.md).
- Local toolchain, no root required (WSL2 Ubuntu 22.04, the sandbox has no sudo):
  `curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0` installs the
  SDK into `~/.dotnet`; `apt-get download libsdl2-dev` plus `dpkg -x` into
  `~/.local/fcpico/sdl2` supplies the headers, after pointing that copy's `sdl2-config`
  `prefix` at itself, adding `-I.../include` and `-I.../include/x86_64-linux-gnu` to its
  `--cflags` (the Debian headers indirect through `SDL2/_real_SDL_config.h`) and `-L` to its
  `--libs`, and repointing `libSDL2.so` at the system runtime `libSDL2-2.0.so.0.18.2`, which
  was already installed. Then
  `PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet"`.
- Plan changes: [`plan/09-testing-ci.md`](plan/09-testing-ci.md) gains "S0 as measured" and a corrected CI paragraph;
  `ci/workflows/doom-cosim.yml` is rewritten against the commands that actually work and
  stays inactive; [`ci/README.md`](ci/README.md), `issues/I-15`, [`issues/README.md`](issues/README.md) and
  [`plan/10-workplan.md`](plan/10-workplan.md) updated. `plan/01` still carries the uncorrected prior-art
  paragraph; it is owned by neither this issue nor its author, and the measurement above is
  the input a future correction needs.

## Python development environment -- 2026-09-20
- Created `doom/.venv` with Python 3.12.11 and installed `tools/requirements.txt`, including
  pytest 9.1.1. The environment is ignored by Git.
- Verified: `doom/.venv/bin/python -m pytest doom/tests doom/sim -q` ->
  348 passed, 1 skipped (the expected PIO IRQ emulation skip).
- Activate from the repository root with `source doom/.venv/bin/activate`, or invoke
  `doom/.venv/bin/python` directly. This resolves the missing-pytest limitation below;
  the ARM toolchain requirement remains 13.2.Rel1.

## Status reconciliation -- 2026-09-20
- Scope: documentation review and cleanup only; no implementation changes.
- Reviewed: plans, agent playbook, issue index, engine-side notes, hardware requests/log,
  current sources and commits through `e9526b9`. Engine submodule remains at `d8a20ca`;
  its FC PICO platform contains planning notes only.
- Verified: `cmake -S doom -B /tmp/fcpico-doom-status-host -G Ninja
  -DFCPICO_HOST_ONLY=ON && cmake --build /tmp/fcpico-doom-status-host &&
  ctest --test-dir /tmp/fcpico-doom-status-host --output-on-failure` -> 6/6 passed;
  `python3 doom/tools/gen_protocol.py --check` -> clean;
  `python3 doom/tools/check_md_links.py doom` -> no broken links.
- Verification limits: `python3 -m pytest doom/tests doom/sim -q` cannot run because
  pytest is not installed. The installed ARM compiler is 10.3.1, not the required
  13.2.Rel1; Wine and .NET are absent from PATH. Earlier test counts below are historical.
- Previously unlogged implementation: `201ce2d` added device CMake configuration,
  the RP2350 bus backend, test-pattern firmware, serial CLI, trace capture/decoder and
  synthetic trace tests ([I-08](issues/I-08-device-superbuild.md)/[I-09](issues/I-09-fcbus-device.md)/[I-10](issues/I-10-testpattern-firmware.md)). `c9c4e12` corrected DMA rearming and trace word
  alignment. `e9526b9` exposed boot-ROM emulator memory and added protocol assertions.
  These commits do not record device build acceptance. The device workflow exists only
  at `ci/workflows/doom-device.yml`, so [I-08](issues/I-08-device-superbuild.md)/[I-09](issues/I-09-fcbus-device.md)/[I-10](issues/I-10-testpattern-firmware.md) remain partial pending validation
  and review against their acceptance criteria.
- Milestone: M0 remains incomplete. No hardware session is recorded; [HR-1](HARDWARE-REQUESTS.md#hr-1-phase-0-trace-capture-and-board-facts-task-p0-t9)/[I-19](issues/I-19-hw-trace.md) still gates
  bus-model calibration and board facts. Licensing enquiry [HA-1](HARDWARE-REQUESTS.md#ha-1-licensing-enquiry-task-p0-t13)/[I-18](issues/I-18-licensing.md) is also pending.
- Next: restore Python test dependencies and run the suite; finish device build/CI
  acceptance for [I-08](issues/I-08-device-superbuild.md) through [I-10](issues/I-10-testpattern-firmware.md). [I-04](issues/I-04-fcapu-core.md) (APU sequencer) is independent host work;
  [I-11](issues/I-11-engine-skeleton.md)/[I-12](issues/I-12-engine-host-build.md) begin the engine integration path.
- Cleanup: removed the superseded resume and September 11 review summaries. Their applied
  corrections remain in the specifications and the historical progress entry below;
  unresolved questions remain in [`plan/11-risks.md`](plan/11-risks.md) and the hardware requests.

## P1-T4 / P3-T1 -- host-testable controller mapper
- Commits: this commit
- Verified: `cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build
  build-host && ctest --test-dir build-host --output-on-failure` -> 6/6 passed.
- What landed: an allocation-free frame latch and tic poller, caller-owned event ring,
  configurable engine key codes, direction/action mapping, always-run, B tap-versus-strafe,
  Select tap-versus-automap, Select+Start pause, sticky presses, and an optional cheat matcher.
- Left out: engine event adapter and context-specific menu mappings remain integration work in
  the engine fork; the mapper deliberately has no dependency on Doom headers.
- Plan changes: none; issue [I-07](issues/I-07-input-mapper.md) is complete.

## P0-T12 -- activate the host CI lane
- Commits: this commit
- Verified: the host CMake build and 5 C tests passed; the ASan/UBSan build and the same 5
  tests passed; `python3 -m pytest doom/tests doom/sim/pioemu -q` -> 339 passed, 1 skipped;
  generated protocol and Markdown link checks passed.
- What landed: one active, host-only GitHub Actions workflow with pinned Ubuntu, Python and
  action major versions, pip caching, ordinary C tests, sanitizer C tests, Python and PIO
  tests, generated-file validation, Markdown link validation, and an exact template diff.
- Left out: device, boot-ROM, full-chip, co-simulation and documentation workflows remain
  inactive until the issue named for each lane can produce a green job.
- Plan changes: none; issue [I-06](issues/I-06-ci-host-lane.md) is complete.

## P1-T2 (reference part) -- reference video pipeline tests
- Commits: this commit
- Verified: `python3 -m pytest doom/tests/tools -q` -> 237 passed.
- Measurements: the deterministic 320x200 greyscale-gradient conversion decodes at 13.02 dB
  PSNR against its decimated and letterboxed source; the regression floor is 13.0 dB.
- What landed: stage B decimation and letterboxing tests; stage E exact-colour and midpoint
  dither tests; stage C palette selection and hysteresis boundary tests; a stage D Bayer-index
  test; and an end-to-end v2 stream conversion decoded through `ppu_decode.py`.
- Left out: the device converter remains issue [I-17](issues/I-17-fcvideo-impl.md); this issue only establishes the tested
  reference against which that implementation will be compared.
- Plan changes: none; issue [I-03](issues/I-03-fcvideo-ref-tests.md) is complete.

## Plan review and issue set -- 2026-09-11
- Commits: this commit
- Verified: every derivable figure in `plan/` recomputed from the primary sources (the
  tutorial's `rp_system.cpp`, `SysPico.asm`, `fcppu.pio`, the PiPU sources) with a throwaway
  Python script; `python3 tools/check_md_links.py` -> 54 files, 35 links, 0 broken.
- Result: ten arithmetic and consistency defects found and corrected in 01, 02, 03, 04, 06,
  07 and 09, plus three design gaps. The separate review summary was retired on 2026-09-20;
  its original audit trail remains in Git history. The
  largest correction replaces the prior-art counting paragraph in 01 with an honest statement
  that `241 x 68 = 16388` consumed bytes and `15426` counted picture reads cannot both be
  read as bytes-per-line arithmetic, and names the cheapest hypothesis to test (CS1 may
  decode only `$0000`-`$0FFF`, excluding sprite fetches). That question is now gated on the
  hardware trace, issue [I-19](issues/I-19-hw-trace.md), and no plan figure derived from it should be trusted until then.
- Also: 07 gained an init step that parks every OAM Y byte at `$EF` once, because sprites are
  deliberately left enabled in PPUMASK for fetch-pattern fidelity while cold-boot OAM is
  zeroed, which would draw 64 sprites in the corner.
- Deliverable: `../issues/` -- 19 self-contained issues plus an index, each with an Owns file
  list so two workers never collide, and acceptance stated as an exact command. Lane A ([I-01](issues/I-01-ppubus-tests.md)
  to [I-07](issues/I-07-input-mapper.md)) is verifiable in this sandbox today; lane B ([I-08](issues/I-08-device-superbuild.md) to [I-17](issues/I-17-fcvideo-impl.md)) cannot be (no
  `arm-none-eabi-gcc`, no Wine, no .NET, and the proxy returns 403 for the ARM toolchain), so
  each of those issues carries the CI job that proves it; lane C ([I-18](issues/I-18-licensing.md), [I-19](issues/I-19-hw-trace.md)) needs a person.
- Left out: filing these as GitHub issues. They are files on the branch, not tracker entries.
- Plan changes: 01, 02, 03, 04, 06, 07, 09, 10 (status table now points at [`../issues/README.md`](issues/README.md)).

## P0-T5 (core), P0-T6, P0-T3 (shim) -- the bus, host-testable end to end
- Commits: `ed582e6`
- Verified: `cmake -S doom -B build -DFCPICO_HOST_ONLY=ON && cmake --build build && ctest
  --test-dir build` -> 5/5 passed (`test_mailbox`, `test_rx`, `test_sync`, `test_stream`,
  `test_host_roundtrip`), and the same suite ASan-clean.
- What landed: `fcbus/fcbus_core.c` (mailbox builder, rx dispatcher, sync decision, stream
  addressing, attribute/palette shadows, data-mode responder -- no device dependency at all),
  `fcbus/fcbus_host.c`, and `sim/host_shim/` with pthread implementations of all 18 missing
  `multicore_*` symbols plus the 4 alarm functions. That answers plan question Q3: pico-sdk's
  host platform provides none of them, and it disables alarm pools outright via
  `PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1`. The `get_core_num()` override links because the
  SDK marks it weak.
- Two defects worth recording, both found by tests rather than by reading:
  the core kept a single attribute/palette shadow, so a bulk v2 upload sent only the four
  bytes per frame that the v1 poke trickle had synced; the tutorial keeps `m_ATR_W` and
  `m_ATR_W_old` separately, so the shadow is now split into `*_want` / `*_sent`.
  And the host backend did not count reads taken while the DMA was stopped, so a stopped
  frame reported count 0 forever and the link could never regain phase; the real `fcppu_rna`
  counts regardless of whether `fcppu_r` is running, which is precisely how phase recovers.
- Left out: the device backend (issue [I-09](issues/I-09-fcbus-device.md)) and the test-pattern firmware ([I-10](issues/I-10-testpattern-firmware.md)). Neither can
  be compiled here.

## P2-T1, P4-T3 (tools parts) -- tests for the drafted NES and audio tools
- Commits: `ed582e6`
- Verified: `python3 -m pytest doom/tests/tools -q` -> 171 passed.
- What landed: the tool drafts from `9e4450d` now have tests -- `tools/audio/apus.py` and
  `dpcm.py` (register sequences, DPCM encode/decode round trips, the retrigger-avoidance rule
  the FC PICO GB port documents), `tools/nes/bincut.py`, `bin2c.py`, `check_fixbank.py`,
  `tools/respack.py`, and `tools/fcpico/stream.py`. `check_fixbank.py` asserts the permanent
  `$F000`-`$FFFF` bank is byte-identical, which is the one mistake that bricks a cartridge.
- Left out: `tools/fcvideo_ref.py` and `tools/ppu_decode.py` are drafted and still untested
  (issues [I-03](issues/I-03-fcvideo-ref-tests.md) and [I-02](issues/I-02-ppu-decode-tests.md)), and `sim/ppubus/ppubus.py` is untested and uncalibrated ([I-01](issues/I-01-ppubus-tests.md)).

## P2-T2 (harness part) -- NMI cycle-count harness, measured on the tutorial ROM
- Commits: see `git log -- doom/tests/bootrom`
- Verified: `python3 -m pytest doom/tests/bootrom -q` -> 17 passed
- Measurements (`doom/tests/bootrom/tutorial_nmi_cycles.json`, py65 1.2.0, NTSC vblank 2273):
  tutorial NMI critical section 577 cycles; totals 1149 / 1341 / 1533 / 1724 for 0 / 8 / 16 / 24
  APU pairs with sprite DMA (+513 modelled); 65 `$2007` reads; 23.7 cycles per APU pair.
- Left out: the Doom ROM does not exist yet; the harness is parameterised for its 128-byte
  mailbox (`$20`-`$5F` + `$C0`-`$FF`) but RAM presets for it are TBD (see the README).
- Plan changes: 01 (measured NMI row, APU cost 24 cycles/pair), 07 (validation note).

## Planning

Planning completed; implementation tasks proceed per [`plan/10-workplan.md`](plan/10-workplan.md).
