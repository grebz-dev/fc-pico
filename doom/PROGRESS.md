# Progress log

One entry per task from `plan/10-workplan.md`, newest first. Format:

```
## <task id> -- <title>
- Commits: <range or list>
- Verified: <command> -> <result summary>
- Measurements: <fps / cycles / bytes / ms, with the command>
- Left out: <what and why>
- Plan changes: <documents touched>
```

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
- Left out: the replacement UF2 still needs an NES reflash to confirm a visible pattern and
  collect three decoder-valid traces. Strict Mesen S0 remains intentionally uncalibrated
  until the real trace determines which strobes the PIO counter omits.
- Plan changes: HR-1, I-10, the hardware log, and the new hardware fixture directory record
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
  Mesen pixels remain open. Strict S0 is still gated by I-19's hardware count calibration.

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
  Mesen pixel goldens. S0 DMA remains stopped pending HR-1/I-19; no displayed-pixel
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
  neither trace is a pixel golden or a hardware calibration. HR-1/I-19 must settle the
  counter before the display path can make those claims.
- Next: implement I-17's host converter stages B-E against the Python reference, then feed
  engine frames through it after I-11/I-12/I-16. Keep co-simulation as the picture gate.

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
- Left out: engine sound/music adapters and asset conversion remain P4-T2/P4-T3. No
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
- Left out: hardware acceptance still needs HR-1/I-19; strict S0 remains unresolved.
  I-04 remains the next independent implementation task.

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
- Left out: strict S0 remains gated on HR-1/I-19; no protocol or count constant was changed.

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
  so I-08, I-09 and I-10 stay short of their hardware-facing claims and HR-1/I-19 still
  gates the bus model. The device lane was already active in `.github/workflows`.
- Plan changes: `port/CMakeLists.txt`, both `doom-device.yml` copies, `ci/README.md`,
  `issues/README.md`.

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
  is recorded as a sharpened HR-1 rather than as an edit to `plan/01` (I-19 owns that file).
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
  frozen either. Strict S0 stays failing until I-19 answers the counter question.
  Doom-mode cartridge and scenarios S1-S6 remain future work behind I-12.
- Local toolchain, no root required (WSL2 Ubuntu 22.04, the sandbox has no sudo):
  `curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0` installs the
  SDK into `~/.dotnet`; `apt-get download libsdl2-dev` plus `dpkg -x` into
  `~/.local/fcpico/sdl2` supplies the headers, after pointing that copy's `sdl2-config`
  `prefix` at itself, adding `-I.../include` and `-I.../include/x86_64-linux-gnu` to its
  `--cflags` (the Debian headers indirect through `SDL2/_real_SDL_config.h`) and `-L` to its
  `--libs`, and repointing `libSDL2.so` at the system runtime `libSDL2-2.0.so.0.18.2`, which
  was already installed. Then
  `PATH="$HOME/.dotnet:$HOME/.local/bin:$PATH" DOTNET_ROOT="$HOME/.dotnet"`.
- Plan changes: `plan/09-testing-ci.md` gains "S0 as measured" and a corrected CI paragraph;
  `ci/workflows/doom-cosim.yml` is rewritten against the commands that actually work and
  stays inactive; `ci/README.md`, `issues/I-15`, `issues/README.md` and
  `plan/10-workplan.md` updated. `plan/01` still carries the uncorrected prior-art
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
  synthetic trace tests (I-08/I-09/I-10). `c9c4e12` corrected DMA rearming and trace word
  alignment. `e9526b9` exposed boot-ROM emulator memory and added protocol assertions.
  These commits do not record device build acceptance. The device workflow exists only
  at `ci/workflows/doom-device.yml`, so I-08/I-09/I-10 remain partial pending validation
  and review against their acceptance criteria.
- Milestone: M0 remains incomplete. No hardware session is recorded; HR-1/I-19 still gates
  bus-model calibration and board facts. Licensing enquiry HA-1/I-18 is also pending.
- Next: restore Python test dependencies and run the suite; finish device build/CI
  acceptance for I-08 through I-10. I-04 (APU sequencer) is independent host work;
  I-11/I-12 begin the engine integration path.
- Cleanup: removed the superseded resume and September 11 review summaries. Their applied
  corrections remain in the specifications and the historical progress entry below;
  unresolved questions remain in `plan/11-risks.md` and the hardware requests.

## P1-T4 / P3-T1 -- host-testable controller mapper
- Commits: this commit
- Verified: `cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build
  build-host && ctest --test-dir build-host --output-on-failure` -> 6/6 passed.
- What landed: an allocation-free frame latch and tic poller, caller-owned event ring,
  configurable engine key codes, direction/action mapping, always-run, B tap-versus-strafe,
  Select tap-versus-automap, Select+Start pause, sticky presses, and an optional cheat matcher.
- Left out: engine event adapter and context-specific menu mappings remain integration work in
  the engine fork; the mapper deliberately has no dependency on Doom headers.
- Plan changes: none; issue I-07 is complete.

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
- Plan changes: none; issue I-06 is complete.

## P1-T2 (reference part) -- reference video pipeline tests
- Commits: this commit
- Verified: `python3 -m pytest doom/tests/tools -q` -> 237 passed.
- Measurements: the deterministic 320x200 greyscale-gradient conversion decodes at 13.02 dB
  PSNR against its decimated and letterboxed source; the regression floor is 13.0 dB.
- What landed: stage B decimation and letterboxing tests; stage E exact-colour and midpoint
  dither tests; stage C palette selection and hysteresis boundary tests; a stage D Bayer-index
  test; and an end-to-end v2 stream conversion decoded through `ppu_decode.py`.
- Left out: the device converter remains issue I-17; this issue only establishes the tested
  reference against which that implementation will be compared.
- Plan changes: none; issue I-03 is complete.

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
  hardware trace, issue I-19, and no plan figure derived from it should be trusted until then.
- Also: 07 gained an init step that parks every OAM Y byte at `$EF` once, because sprites are
  deliberately left enabled in PPUMASK for fetch-pattern fidelity while cold-boot OAM is
  zeroed, which would draw 64 sprites in the corner.
- Deliverable: `../issues/` -- 19 self-contained issues plus an index, each with an Owns file
  list so two workers never collide, and acceptance stated as an exact command. Lane A (I-01
  to I-07) is verifiable in this sandbox today; lane B (I-08 to I-17) cannot be (no
  `arm-none-eabi-gcc`, no Wine, no .NET, and the proxy returns 403 for the ARM toolchain), so
  each of those issues carries the CI job that proves it; lane C (I-18, I-19) needs a person.
- Left out: filing these as GitHub issues. They are files on the branch, not tracker entries.
- Plan changes: 01, 02, 03, 04, 06, 07, 09, 10 (status table now points at `../issues/README.md`).

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
- Left out: the device backend (issue I-09) and the test-pattern firmware (I-10). Neither can
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
  (issues I-03 and I-02), and `sim/ppubus/ppubus.py` is untested and uncalibrated (I-01).

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

Planning completed; implementation tasks proceed per `plan/10-workplan.md`.
