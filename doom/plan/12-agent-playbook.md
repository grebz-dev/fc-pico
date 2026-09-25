# 12 -- Agent playbook

Operating rules for an AI coding agent executing this plan. A human
following the same rules will not go wrong either.

## Before the first task

1. Read [`doom/README.md`](../README.md), then 00, 01, 02 fully. Skim 03-09. Open 10 and find the first task
   whose dependencies are done (tasks record completion in [`doom/PROGRESS.md`](../PROGRESS.md), see below).
2. Read the FC PICO documentation the plan was derived from: [`README.md`](../../README.md),
   [`docs/pages/architecture.md`](../../docs/pages/architecture.md), [`nes-doom-technique.md`](../../docs/pages/nes-doom-technique.md), [`protocol.md`](../../docs/pages/protocol.md), [`hardware.md`](../../docs/pages/hardware.md),
   [`graphics.md`](../../docs/pages/graphics.md), [`boot-and-reflash.md`](../../docs/pages/boot-and-reflash.md), [`build-pipeline.md`](../../docs/pages/build-pipeline.md). They are short and precise.
3. Read [`doom/rp2040-doom/README.md`](../rp2040-doom/README.md) and [`FCPICO-PORT.md`](../rp2040-doom/FCPICO-PORT.md), then `src/pico/i_video.c`,
   `src/pico/i_system.c`, `src/pd_render.cpp` (at least `pd_end_frame`, `pd_core1_loop`),
   `src/CMakeLists.txt`.
4. Check current tools and dependencies; historical sandbox limitations are not permanent.
   Set up the environment with `doom/tools/setup_env.sh` (once it exists; until then follow
   08 "Toolchain pins"). Confirm `arm-none-eabi-gcc --version` reports 13.2.

## The loop

For each task in [10](10-workplan.md):

1. **Restate** the task's acceptance test in your own words at the top of your working notes.
   If you cannot state how it will be verified by a command, the task is not ready: fix the
   plan first (see "Changing the plan").
2. **Read the spec** documents the task names, and the existing code they reference. Do not
   start from memory of how "Doom ports usually work"; this one is unusual.
3. **Implement** in the smallest slices that keep every build green. Commit each slice.
4. **Verify** with the task's acceptance command(s). Paste the command and the relevant
   output into the commit message body or [`PROGRESS.md`](../PROGRESS.md). Never mark a task done on the
   strength of "should work".
5. **Record** in [`doom/PROGRESS.md`](../PROGRESS.md): task ID, commit range, what was verified and how, what
   was left out and why, measurements (fps, cycles, bytes) with the command that produced them.
6. If the task needs hardware you do not have, do every part that does not, then write a
   precise request in [`doom/HARDWARE-REQUESTS.md`](../HARDWARE-REQUESTS.md) (what to flash, what to run, what to record,
   where to put the result) and move to the next task whose dependencies are satisfied.

## Hard rules

- **Never modify `tutorial_project/`**, `docs/pages/` (except to add Doom pages or correct an
  error you have verified), `Doxyfile`, or anything on the `main`/`docs/*` branches. The Doom
  boot ROM *copies* files out of `tutorial_project/BOOTROM/`.
- **Never modify the fix bank** (`BOOTROM_FIX/`, `bootrom_fixr.bin`, the `$F000`+ region).
- **Never change a v1 protocol constant.** v2 adds; it does not alter.
- **One source of truth** for protocol constants (`fcbus/fcbus_protocol.h`) and for the flash
  layout (`port/flash_layout.h`). If you find yourself typing `0x10080000` or `15490` in a
  second place, stop and include the header.
- **No allocation on core 1 after init**, no `printf` from the bus ISR, nothing reachable from
  the ISR or the SAVING-time converter may live in flash. The engine's `hard_assert` and the
  `__not_in_flash_func` audit in [P1-T3](10-workplan.md#p1-t3-device-integration-cores-irqs-semaphores) enforce this; do not weaken them.
- **Preserve the measured PPU accounting.** The console selects 66 pre-render reads and
  64 reads per visible line; `fcppu_rna` reports the final read's zero-based index.
  `VRAM_LINE_WORDS=34` describes the original converter's batch size, while the selected
  stream advances 32 words per visible line from the 31-word head. The one- and two-byte
  `out pins, 8` nudges consume buffer bytes. Change these facts only with new hardware
  evidence and update 01.
- **Engine changes go in the fork**, under `src/fcpico/` or behind `#if FCPICO` in shared files,
  with the smallest diff that works. Do not reformat engine files. Commit to the engine branch
  first, then advance the submodule pin in `fc-pico` in a separate commit that says which
  engine commit it points at.
- **Generated files are committed** (`bootrom/gen/*`, `tools/fcpico/protocol.py`) and checked
  by CI; regenerate with the tool, never by hand.
- **Goldens change only through `tools/update_goldens.py`**, and the commit message must say
  why the picture changed.
- **Binary blobs**: only `doom.nes`-sized artifacts, fixtures and goldens under a few MB total.
  Never commit `doom1.whx` or WAD files (the engine repo already carries `doom1.whx`; reference
  it via the submodule path).
- **Licence headers** on every new file: BSD-3-Clause for `fcbus`, `fcvideo`, `fcapu`, `port`,
  `sim`, `tools`; GPLv2 for engine files that are derived from Chocolate Doom code (the
  `i_*_fcpico.c` files derived from `src/pico/i_*.c` are GPLv2). Say which in the header.
- **Commit hygiene**: imperative subject <= 72 chars, body says what was verified. Tag the task
  ID (`P1-T2`). No model names or generated-by lines in code or docs pushed to the repository.
- **Ask a human** only for the items in [11](11-risks.md) "Decisions that need a human". Everything else,
  decide, record the decision in [`PROGRESS.md`](../PROGRESS.md), and move on.

## Changing the plan

The documents in `doom/plan/` are specifications, and they will be wrong in places. When
execution shows a document is wrong:

1. Fix the document in the same commit as the code that revealed it, with a one-line note in
   the document's changelog section (add one at the bottom: `## Changelog`).
2. If the change affects a decision in 00's log, update the log entry and list the documents
   revised.
3. Do not leave contradictions between documents; grep for the number or name you changed.

## Verification commands you will use constantly

```
# builds
cmake -S doom -B build-rp2350 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s
cmake --build build-rp2350
cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON
cmake --build build-host && ctest --test-dir build-host --output-on-failure
python3 -m pytest doom/tests doom/sim/pioemu -q
# layout, generated files
python3 doom/tools/flash_layout_check.py build-rp2350/port/fcpico_doom.elf
python3 doom/tools/gen_protocol.py --check
# planned boot rom (requires I-13)
doom/bootrom/build.sh && md5sum doom/bootrom/out/doom.nes
# planned engine host demo run and goldens (requires engine and video integration)
build-host/rp2040-doom/src/fcpico_doom_host --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 600 --dump-stream /tmp/out
python3 doom/tools/ppu_decode.py /tmp/out/frame_0100.bin -o /tmp/f100.png
python3 doom/tests/goldens/check.py /tmp/out
# planned co-sim (requires I-15)
doom/sim/mesen2/run_scenario.sh S2
```

## Files the agent maintains

| File | Purpose |
|------|---------|
| [`doom/PROGRESS.md`](../PROGRESS.md) | task-by-task log of what was done and verified; the source for milestone gates |
| [`doom/HARDWARE-REQUESTS.md`](../HARDWARE-REQUESTS.md) | precise, self-contained requests for a human with the console; each with a "result" slot |
| [`doom/HARDWARE-LOG.md`](../HARDWARE-LOG.md) | filled by the human; the agent reads it and updates 01 and [`PROGRESS.md`](../PROGRESS.md) |
| [`doom/LICENSES.md`](../LICENSES.md) | component licences |

## What good looks like

A reviewer opening any commit on this branch can see: the task ID, the spec it implements, the
command that proves it, and its output. A reviewer opening [`PROGRESS.md`](../PROGRESS.md) can see exactly which
milestone gate is next and what blocks it. A reviewer opening any document in `plan/` finds it
consistent with the code.

## Changelog

- 2026-09-20: corrected the host-only build command, labelled planned verification commands,
  and required checking current environment capabilities on resume.
