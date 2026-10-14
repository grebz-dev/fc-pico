# 15 — Restructure: DOOM as an rp2040-doom fork (`fc-pico` branch)

Date: 2026-10-13. Status: proposed; nothing in this document has been executed.
Related: [13 — upstream FC PICO contributions](13-upstream-contributions.md).

## Decision and goal

The FC PICO Doom port is primarily a Doom port, so its home becomes the
`grebz-dev/rp2040-doom` fork, not the `grebz-dev/fc-pico` fork. Deliver it as
one new branch, `fc-pico`, in `grebz-dev/rp2040-doom`:

- based on the upstream `kilograham/rp2040-doom` default branch `rp2`;
- holding a short, curated history: logical commits, not session commits;
- containing only source, build, test, CI and runtime-asset files, plus exactly
  two documents: a port README and a known-issues file;
- carrying no photos, screenshots, plans, issue trackers, progress/hardware logs,
  scratch probes, prototypes, Serena memories, agent notes or task IDs.

`grebz-dev/fc-pico` stays a fork of `impact-soft/fc-pico` (its `main` already
equals upstream `9e225f3`). New work there goes only to the plan 13 platform
contribution branches. The existing `doom*` branches stay unchanged as
extraction sources (D6).

## Source baseline (2026-10-13)

| Repository | Ref | SHA | Notes |
| --- | --- | --- | --- |
| `kilograham/rp2040-doom` | `rp2` (upstream HEAD) | `29a453c9` | Base for `fc-pico`; also the merge base of the current fork branch |
| `grebz-dev/rp2040-doom` | `claude/doom-fc-pico-nes-2bb1bx` | `c1145f02` | 23 commits ahead of `rp2`: 35 files, +2889/−51, mostly `src/fcpico/` |
| `grebz-dev/fc-pico` | `doom-sprite_billboards` | `3183340` | 81 commits on `9e225f3`; ~450 tracked files under `doom/` |
| `grebz-dev/MesenCE-FC-PICO` | submodule pin | `9688726c` | Only 3 files differ from base `20f497c9` (see plan 13 U05) |
| `impact-soft/fc-pico` | `main` | `9e225f32` | Supplies tutorial `BOOTROM/` fix bank and `rom.NES` |

Before you start, re-fetch every row and record the actual SHAs. Treat the table
as a snapshot only.

## Target layout on `rp2040-doom@fc-pico`

```text
<upstream rp2040-doom tree>        unchanged except the engine changes below
src/fcpico/                        engine platform layer (already in the fork)
src/doom/*, src/*.c, CMakeLists    small FCPICO-guarded engine changes (already in the fork)
fcpico/
  README.md                        the port README (only doc #1)
  KNOWN-ISSUES.md                  known technical issues (only doc #2)
  CMakeLists.txt                   cartridge superbuild entry (was doom/CMakeLists.txt)
  cmake/                           board, version pins
  fcbus/                           cartridge bus core, host/device backends, protocol, PIO
  port/                            device glue: video, input, apu, audio, world, cli, trace
  bootrom/                         Doom erasable bank sources + native build
  testpattern/                     bring-up/diagnostic firmware
  assets/                          hand-edited CHR/BIN/JSON art sources (no PNG)
  sim/                             cartmodel, host_shim, pioemu, ppubus, mesen runners/Lua
  sim/mesen/MesenCE/               MesenCE fork submodule, `fc-pico` branch (D2, interim)
  tools/                           build/convert/inspect tools needed by the build or tests
  tests/                           ctest + pytest suites and small fixtures
  external/fc-pico/                pristine impact-soft/fc-pico submodule (see D1)
.github/workflows/fcpico-*.yml     host, bootrom, device, cosim lanes
```

Engine files stay where upstream has them, so the fork's diff against `rp2`
stays readable and rebaseable. Everything that came from `fc-pico/doom/` sits
under one `fcpico/` directory, so it never mixes with Chocolate Doom sources.

## Decisions

| ID | Decision | Resolution (2026-10-13) |
| --- | --- | --- |
| D1 | Source of the tutorial `BOOTROM/bootrom_fixr.bin`, `rom.NES` and the `rp_system` sources that tests compare against | **Accepted.** Submodule `fcpico/external/fc-pico`, pinned to **pristine** `impact-soft/fc-pico` (no fork commits). This avoids redistributing files whose licence is unclear. The port can later consume upstreamed platform code (plan 13 U01/U02, U05) from the same pin. |
| D2 | MesenCE integration | **Accepted: option A.** Use the MesenCE fork as a submodule at `fcpico/sim/mesen/MesenCE`, pinned to a new `fc-pico` branch of the fork. That branch carries one extra commit making the makefile paths configurable. See [MesenCE integration](#mesence-integration-d2). |
| D3 | Shape of the history | **Accepted.** 12 logical commits (below), each checked with `git rebase --exec`. |
| D4 | Top-level README | **Accepted.** Add a short banner at the top of upstream `README.md` linking to `fcpico/README.md`. Change nothing else in it. |
| D5 | Committed art | **Accepted.** Keep the hand-edited art sources only. Drop the reference/working art (see [File disposition](#file-disposition)). Never commit generated world-sprite sheets or IWAD-derived atlases. The work needed for user-supplied WADs is in [World sprites from user-supplied WADs](#world-sprites-from-user-supplied-wads-d5). |
| D6 | Old branches | **Accepted: delete nothing.** Leave every existing branch and submodule pin in both forks untouched, so plan 13 extraction can keep using them. Archive tags are optional and additive. Whether to change the fork's default branch is left to the user. |
| D7 | Commit authorship | **Accepted.** Author and committer: `grebz-dev`. No AI co-author trailers; the user makes AI disclosures manually in PRs and at repo level. |

## MesenCE integration (D2)

**What the fork changes.** It differs from upstream base `20f497c9` in three files:

| File | Change | Needed by any FC PICO build? |
| --- | --- | --- |
| `Core/NES/Mappers/Homebrew/FcPico.h` (new, 212 lines) | The mapper model. It handles address selection, CHR RAM for unselected reads, the PRG flash command state machine, and watchdog ticks. Its PPU reads and writes go to the native `fcpico_cart_*` backend. | Yes |
| `Core/NES/MapperFactory.cpp` (+4 lines) | Registers private NES 2.0 mapper ID 4093 under `#ifdef FCPICO` | Yes, unavoidable: Mesen only creates mappers through this switch |
| `makefile` (+9 lines) | When `FCPICO_ROOT` is set, adds `-DFCPICO`, include paths and four static libraries to the core shared library | Only because the backend is linked statically. It also hard-codes `doom/…` paths, so it must change after the restructure no matter which option is chosen. |

Nothing else is patched. The cartridge "definition" is just an iNES/NES 2.0 header
written by `tools/nes/set_mapper.py`. Settings, the Lua probes and the runners
are all external to Mesen. The C# UI is unchanged.

**How Mesen handles extensions.** Mesen has no plugin or extension API for
mappers or cartridge hardware:

- Mappers are C++ classes compiled into the core and chosen by the mapper-ID
  switch in `MapperFactory`. A new mapper means either an upstream PR (Mesen has
  a `Homebrew/` folder for homebrew boards that have assigned mapper numbers)
  or a modified build.
- Lua scripting is Mesen's only user extension point. It is a debugging and
  automation interface: memory callbacks, drawing, input, savestates. It runs in a
  sandbox that cannot load native code, and calling it for every PPU fetch
  (~15.5k selected reads per frame) is not practical. So it cannot carry the cartridge model.
- Emulators that do have native plugin systems (for example libretro cores) are
  a different, coarser level: they replace the whole emulator, not one mapper.

So any FC PICO support in Mesen means carrying modified Mesen source. The only
choice is how to carry it:

| Option | What it involves | Trade-off |
| --- | --- | --- |
| A. Fork submodule (current) | Pin `grebz-dev/MesenCE-FC-PICO` on a `fc-pico` branch. Add one fork commit replacing the hard-coded `doom/` makefile paths with `FCPICO_INCLUDE`/`FCPICO_LIBS` variables. | Simplest. The fork history already exists and stays reviewable. Rebasing onto newer MesenCE is a normal git rebase. The cost is a gitlink, which is acceptable in this personal fork. |
| B. Patch bundle | Track `FcPico.h` plus two small patches and a manifest naming a checksum-verified pristine archive. Build into an ignored cache. | No gitlink. Patch maintenance is minimal at this size, but every MesenCE bump needs a re-check that the patches still apply. This is what plan 13 U05 must use for upstream fc-pico anyway. |
| C. Runtime-loaded backend + upstream PR | Change `FcPico.h` to `dlopen` a backend library named in configuration instead of linking it. The makefile patch then disappears entirely, leaving one new file and a 4-line registration. A generic "external cartridge backend" could then be proposed to MesenCE. | Shrinks the change the most and is the only route to zero carried changes. But upstream acceptance is uncertain, and an unassigned mapper ID is unlikely to be accepted. Needs a stable versioned C ABI (plan 13 U04). |

**Decision: A.** Use it for the restructure: it is one fork commit and changes
no behaviour. Do not duplicate the integration. When plan 13 U05 lands in fc-pico,
build from its tracked integration via the D1 `external/fc-pico` submodule
(which is option B, maintained once, upstream) and drop the MesenCE submodule.
Consider C as a later follow-up together with U04's ABI.

**Independent fix needed under every option:** `tools/build_world_atlas.py` reads
the NES palette from the MesenCE source checkout
(`Core/NES/NesDefaultVideoFilter.cpp`). Device and atlas builds must not need an
emulator checkout. See work item W2 below.

## World sprites from user-supplied WADs (D5)

**Today.** Upstream's `whd_gen <wad> <whx>` converts a WAD into the engine's WHX
format. It knows nothing about NES sprites, and FC PICO does not need it to.
The native world-sprite atlas comes from a separate Python step,
`build_world_atlas.py`, run by CMake when `FCPICO_WORLD_WAD` is set. The firmware
keys the atlas by the engine's `SPR_*` enum, which it parses from `info.h`. Any
sprite family missing from the atlas falls back to the streamed background path
(`streamed_lookup`), so a WAD with unknown actors still renders. Measured on the
shareware IWAD: 30 of the 43 scoped families are present, and the atlas is
407,997 B of the 512 KiB `FLASH_WORLD_MAX`.

**Gaps that block "convert any supported WAD" with rp2040-doom's tooling:**

| ID | Gap | Work |
| --- | --- | --- |
| W1 | The atlas reads its family list, reviewed palette overrides and exclusivity rules from `doom/plan/world-sprite-scope.json`, which the restructure removes | Move the file to `fcpico/assets/world-sprite-scope.json` as a tracked build input. Update CMake and tool defaults. Required for the restructure itself. |
| W2 | The atlas reads Mesen's 2C02 palette from the emulator source tree. The video converter uses a different table (`tools/fcpico/nes_palette.py`, FCEUX/nesdev). | Snapshot the exact Mesen table into tracked data with provenance, so the atlas stays byte-identical. Required for the restructure. Unifying the two palettes is a separate, visually reviewed change. |
| W3 | WHX input is hard-coded to the bundled `doom1.whx` (`CMakeLists.txt`, `port/audio`, `port/world`), and the atlas and audio take separate `FCPICO_WORLD_WAD`/`FCPICO_AUDIO_WAD` variables | Add one `FCPICO_WAD` input that drives the WHX (via `whd_gen`, built as a host tool with ExternalProject because the device build cross-compiles), the atlas and the audio bank. Keep `FCPICO_WHX` as an override, and default to the bundled shareware WHX. |
| W4 | The firmware compiles in `FCWORLD_ATLAS_IDENTITY`, so every new WAD needs a firmware rebuild. Upstream's model is one firmware plus a separately loaded WHX. | Validate at runtime instead: the atlas header records the WHX checksum and an engine sprite-table hash, and the firmware checks both at boot. On a mismatch it disables native actors and streams them, rather than failing. One release UF2 then works with any converted WAD. |
| W5 | There is no one-shot conversion command | Add `fcpico/tools/convert_wad.py <wad> -o <dir>`: run `whd_gen`, build the atlas and audio bank, then use `whx2uf2.py` without `--firmware` to emit a data-only UF2 plus a manifest. This is the counterpart of upstream's `whd_gen` + `picotool load` flow. |
| W6 | Non-shareware IWADs. The 13 scoped families missing from shareware (`HEAD`, `SKUL`, `BSKU`, `PLAS`, `BFUG`, …) would push the atlas past 512 KiB: a linear estimate is about 585 KB. Doom II actors (`CPOS`, `SKEL`, `FATT`, …) are not in the scope at all. A registered WHX may exceed the 2 MB `FLASH_WHX_MAX`. | Make the atlas builder budget-aware: use per-family priority and let overflow fall back to streaming, with a report. Measure the Ultimate Doom and Doom II WHX and atlas sizes, then revise `flash_layout.h` and its checks if needed. Add Doom II families to the scope only with palette review. Upstream notes `whd_gen` is only tested on id WADs; keep that limitation in KNOWN-ISSUES. |
| W7 | CI cannot use id assets | Test the atlas builder with a small synthetic WAD fixture (original redistributable art) covering present, missing and over-budget families |

W1 and W2 are part of the restructure: without them the build breaks once
`plan/` is dropped and the MesenCE pin moves. W3–W7 are follow-up features
committed on `fc-pico` after it is published, in the order W3, W4, W5, W7, W6.
Until they land, KNOWN-ISSUES must say that native world sprites need a firmware
build from the user's own IWAD, and that only shareware sprite families are tuned.

## File disposition

The rule is reachability. A file is kept only if one of these reaches it: the
cartridge CMake build, a CI lane, the single reproduction script, or a kept
test. It is also kept if it is a named developer tool (marked *dev* below).
Before committing, check the inventory below against that rule
(`git ls-files` vs CMake/CI/script references) and resolve any differences
explicitly.

| Source (`fc-pico/doom/…`) | Disposition |
| --- | --- |
| `fcbus/`, `port/`, `bootrom/`, `testpattern/`, `cmake/`, `CMakeLists.txt` | Keep → `fcpico/…`; rewrite paths |
| `sim/cartmodel`, `sim/host_shim`, `sim/pioemu`, `sim/ppubus` | Keep → `fcpico/sim/…` |
| `sim/mesen2/` runners, `lua/`, `goldens/`, `build.sh` | Keep the generic runners plus the `run_doom_frame`/world/UI scenario runners used by CI. Drop the `run_*_probe.py` scripts (they hold hard-coded local paths and are one-off experiments). Replace the `Mesen2` gitlink according to D2 (`fcpico/sim/mesen/MesenCE`, pinned to a `fc-pico` branch of the MesenCE fork). |
| `tools/fcpico/`, `tools/nes/`, `tools/audio/`, `tools/perf/` (fps/census reporter only) | Keep |
| `gen_protocol.py`, `respack.py`, `whx2uf2.py`, `flash_layout_check.py`, `check_device_ram.py`, `check_sprite_layout.py` | Keep (build/verification) |
| `ppu_decode.py`, `trace_decode.py`, `fcvideo_ref.py`, `export_hud_ppu_dump.py` | Keep (*dev* inspection, tested) |
| `build_world_atlas.py`, `build_world_sprite_art.py`, `build_doomguy_faces.py`, `build_large_face_art.py`, `build_doom_menu_art.py`, `build_live_hud_art.py`, `build_native_menu_font.py`, `native_status_art.py`, `overlay_native_episode.c`, `render_native_status_panel.c`, `edit_sprite_sheets.py` | Keep (asset pipeline / *dev* art editing) |
| `setup_env.sh`, `requirements.txt`, `build_shadow_detail.sh`, `reproduce_native_hud.sh` | Merge into one `fcpico/tools/build.sh` (setup/host/rom/device/cosim/`--verify-twice`). Drop the milestone-named scripts. |
| `build_*_probe*.{py,sh}`, `build_ui_transport_probe.py`, `build_world_sprite_rom.py`*, `world_sprite_scene.py`*, `capture_skill_menu_frame.sh`, `preview_native_ui.sh`, `prototypes/`, `check_md_links.py` | Drop (probes, prototypes, plan-doc tooling). *Keep these if reachability shows CI depends on them, and rename them by function, not milestone. |
| `tests/**` | Keep the tests that cover kept code; drop tests of dropped tools/prototypes. Keep `fixtures/hw_trace_ntsc` (small calibration fixture) and the goldens |
| `assets/doomguy_faces{,_large}.{chr,json}`, `doomguy_faces_large_{tiles,flips}.bin`, `doom_menu_logo{,_small}.chr`, `doom_episode_pairs.chr`, `doom_menu_art.json` | Keep: the hand-edited art sources consumed by the build (D5). Fold the `EDITING.md`/`*.md` instructions into the README "Editing native art" section. `edit_sprite_sheets.py` regenerates edit PNGs from the `.chr` files on demand. |
| `plan/world-sprite-scope.json` | Keep → `fcpico/assets/world-sprite-scope.json` (W1): it is a build input, not a plan |
| `assets/**/*.png`, `assets/hud_edit_reference/`, `hud_layout_variants/`, `hud_live_plate/`, `hud_playback_candidates/`, `menu_native_9016/`, `palette_eval.*`, `shadow_detail_frame300.png` | Drop: reference/working art, previews, captures and comparison variants. Their `.bin`/`.oam`/`.ppu` files are outputs of Lua probes and exporters, not build inputs. |
| `*.jpg`, `StatusBar.png` (all hardware photos) | Drop |
| `plan/`, `issues/`, `PROGRESS.md`, `HARDWARE-LOG.md`, `HARDWARE-REQUESTS.md` | Drop. Carry only the durable facts into README/KNOWN-ISSUES (see below). |
| `LICENSES.md` | Fold into a README "Licensing" section; drop the task references |
| `reference/scaling-wip/` | Drop: a third-party reference with unclear licence, not built |
| `ci/workflows/*.yml`, fc-pico `.github/workflows/doom-*.yml` | Re-home as `.github/workflows/fcpico-*.yml`. Remove the hard-coded local paths. |
| fc-pico `.serena/**`, `docs/pages/doom-port.md`, `docs*/pages/nes-doom-technique.md` edits | Not carried. Keep Serena state out with a local `.git/info/exclude`, not a committed ignore rule. |
| Engine fork `FCPICO-PORT.md`, `src/fcpico/README.md` | Fold the still-accurate integration notes into `fcpico/README.md` → "Engine integration". Delete both. |

## History construction

Work in a new worktree. Never rewrite or force-push existing branches.

1. `git fetch upstream` in the engine checkout and record `upstream/rp2`. Run
   `git worktree add ../rp2040-doom-fcpico -b fc-pico upstream/rp2`.
2. Produce the target end state first, in a staging directory:
   - engine: `git diff 29a453c9 c1145f02 -- . ':!FCPICO-PORT.md' ':!src/fcpico/README.md'`;
   - fcpico tree: `git -C fc-pico archive doom-sprite_billboards doom/`
     filtered by the disposition table, then moved to `fcpico/`.
   - Rewrite all paths:
     - `doom/` → `fcpico/`;
     - `../../tutorial_project` and `REPO/"tutorial_project"` → a single
       `FCPICO_UPSTREAM_DIR` (CMake cache variable / Python constant)
       defaulting to `fcpico/external/fc-pico`;
     - superbuild inversion: the root `CMakeLists.txt` gains
       `option(FCPICO …)` that adds `fcpico/` and sets `FCPICO_SUPERBUILD`.
       The original VGA targets must still configure with it OFF.
   - Apply W1 (scope manifest into `fcpico/assets/`) and W2 (tracked Mesen
     palette snapshot). The world atlas must stay byte-identical.
   - Add one commit to the MesenCE fork's `fc-pico` branch that replaces the
     makefile's hard-coded `doom/` paths with `FCPICO_INCLUDE`/`FCPICO_LIBS`.
3. Scrub the staged tree. Each item is a gate that must return nothing:
   - `git grep -nE 'plan/[0-9]|issues/I-|HARDWARE-|PROGRESS\.md|\bHR-[0-9]+|\bP[0-9]-[TUW][0-9]+|\bI-[0-9]{2}\b|\bU[0-9]\b'`
     (48 fcpico files and 1 engine file match today). Reword comments to state
     the constraint itself, not where it was planned.
   - `git grep -nE '/home/|/tmp/fcpico|claude/doom-fc-pico|\.serena|codex'`.
   - `git ls-files | grep -iE '\.(jpg|jpeg|png|gif)$'` must list only the upstream
     engine's own files (`m_*.png` etc.).
   - `git diff --stat upstream/rp2` touches no upstream file outside the
     `FCPICO`-guarded set, plus the README banner (D4).
4. Commit the staged state as a curated series (D3), each with an imperative
   subject and a body that explains *why*:
   1. Guard the engine build for an external cartridge superbuild
   2. Add the FC PICO engine platform layer and SDL-free host runner
   3. Add the cartridge bus library, protocol definitions and PIO program
   4. Add the Doom boot ROM bank and native ROM toolchain with fix-bank gates
   5. Compose indexed frames and convert them to the NES pattern stream
   6. Map NES controller input to Doom events
   7. Drive the console APU: sequencer, converters and engine sound modules
   8. Render the status bar, face and menus as native NES tiles and sprites
   9. Render enemies and pickups as native NES sprites (protocol v5)
   10. Add host cartridge model, PIO emulation and MesenCE cosimulation
   11. Add device packaging, flash layout checks and serial BOOTSEL
   12. Add CI lanes, the port README and known issues
   Author and commit every commit as `grebz-dev`, with no AI co-author trailers (D7).
   Run `git rebase upstream/rp2 --exec 'fcpico/tools/build.sh host --test'` over the
   series. Commits 1–2 run the engine-only host check.
5. Tree equivalence: after path rewriting, the final tree must equal the staged
   tree (`git diff --exit-code <staged-tree> HEAD`). Each kept file must match
   its source byte-for-byte except for documented path/comment rewrites; list
   those with a review script.

## Verification before publishing

| Gate | Expectation |
| --- | --- |
| Engine-only | Upstream VGA/`doom_tiny` targets still configure and build with `FCPICO=OFF` |
| Host | `ctest` + `pytest` pass from a fresh clone with `--recurse-submodules` (except the engine's `3rdparty/tinyusb`) |
| ROM | Doom bank assembles reproducibly; fix bank byte-identical to the pinned upstream; tutorial MD5 gate passes |
| Streams | Fixed-demo host stream/golden outputs match the pre-restructure build bit-for-bit |
| Cosim | Strict S0, D0/D1 and S1 pass with the patch-bundle MesenCE; world-sprite scenario fps within the noise of the current baseline |
| Device | The UF2 builds via picotool. The flash/RAM layout checks pass. Embedded ROM bytes equal the current `20DOOM-05-9031` build. Differences in the firmware image are explained (paths/`__FILE__` only) |
| Reproducibility | `build.sh --verify-twice` matches in two independent trees |
| Physical | The user flashes the restructured UF2 for one smoke run (boot, play, audio, BOOTSEL). Until that run, the branch is not described as hardware-verified. |
| Cleanliness | The scrub gates in step 3 return nothing; the only docs are `fcpico/README.md`, `fcpico/KNOWN-ISSUES.md` and the upstream engine's own docs |

## Content of the two documents

**`fcpico/README.md`** covers:
- what the port is;
- hardware requirements (FC PICO cartridge, RP2350, NTSC NES-001 tested);
- quick start for flashing a release UF2;
- build from source (prerequisites and pins, `build.sh` stages);
- running in MesenCE;
- controls;
- repository map;
- engine integration notes (from `FCPICO-PORT.md`);
- protocol/bus summary with measured constants (from `mem:doom/bus_protocol` and plan 01, as facts, not history);
- editing native art;
- licensing (from `LICENSES.md`);
- credits (rp2040-doom, Chocolate Doom, impact soft FC PICO, MesenCE).

**`fcpico/KNOWN-ISSUES.md`** lists current limitations of the port as a user or
contributor would hit them. Every entry has a symptom, scope/conditions, status
and a workaround if one exists. It contains no session IDs, hardware-request
numbers, task IDs or dated progress narrative. Re-verify each candidate against
current evidence when writing it. Candidates found today:

- Frame rate: background conversion is ~38 ms/frame, against an 8 ms design target. Cosim gives 13–14.5 fps with native actors and ~17.7 streamed. Hardware fps has not been measured.
- World sprites: occasional flashing/desync of native actors is unresolved, and long soak runs have not been done.
- Display: precise picture geometry has not been checked against an independent reference.
- Input: the full context-dependent controller mapping is incomplete beyond movement, B use/strafe and menus.
- Saves/loads have not been validated on the cartridge.
- Startup: a diagnostic startup countdown is still required in the device build profile, and nobody yet knows why.
- Coverage: only an NTSC NES-001 has been tested. PAL consoles and the Famicom have not been tested.
- Upstream targets: after the engine changes, the original VGA/I2S targets have only been configure-checked, not run.
- Licensing: the terms for the impact-soft-derived `fcppu.pio` modifications and the Doom boot ROM bank are unconfirmed.

## Effect on plan 13 (upstream fc-pico contributions)

- Plan 13's extraction sources move. `fc-pico/doom/X` becomes
  `rp2040-doom@fc-pico:fcpico/X`, and the engine paths do not change. Feature branches
  still target `impact-soft/fc-pico` and still contain only ordinary tracked files.
- Execute this restructure first. It is mechanical and gives plan 13 one clean,
  scrubbed source to extract from.
- Once U01/U02 merge upstream, the port should build `fcbus` and its protocol
  definitions from the `fcpico/external/fc-pico` pin rather than carry its own copy.
  This applies plan 13's "one implementation" contract across both repositories.
- No fc-pico contribution branch may reference the rp2040-doom fork. Provenance
  is recorded in PR descriptions, not gitlinks.

## Execution checklist

1. All decisions D1–D7 are settled.
2. Fetch, record SHAs and create the worktree/branch locally (step 1).
3. Stage, rewrite and scrub (steps 2–3). Review the disposition diff.
4. Build the commit series and run the per-commit checks (step 4–5).
5. Run the verification gates, then the user's physical smoke run.
6. Push `fc-pico` to `grebz-dev/rp2040-doom` (and the MesenCE fork's `fc-pico` branch).
   Delete, rewrite or repoint nothing: the fc-pico `doom*` branches, their submodule pins and
   rp2040-doom `claude/doom-fc-pico-nes-2bb1bx` remain plan 13 extraction sources (D6).
7. Follow-up features on `fc-pico`: W3, W4, W5, W7, W6.
