# Resume notes

Written at the end of a working session (2026-09-10) so that the next session -- human or
agent -- can pick up without re-deriving context. Read this, then `PROGRESS.md`, then
`plan/10-workplan.md`. Delete or trim this file once the items below are absorbed into
`PROGRESS.md`.

## Where things stand

Branch `claude/doom-fc-pico-nes-2bb1bx` in `grebz-dev/fc-pico` (this repo) and in
`grebz-dev/rp2040-doom` (submodule `doom/rp2040-doom`, pinned at `d8a20ca`, which only adds
`FCPICO-PORT.md` and `src/fcpico/README.md`). No hardware session has happened; every
"(inferred)" and "(empirical)" row in `plan/01-constraints.md` is still unconfirmed.

### Done, tested, committed

| Item | Where | Verify |
|------|-------|--------|
| Plan documents 00-12, refined with prior art (PiPU, FC PICO GB), API sketches, v2 NMI draft, `.apus` format, Mesen2 mapper sketch, trace sampler | `plan/` | `python3 tools/check_md_links.py doom` |
| Protocol single source of truth + generator + generated files | `fcbus/fcbus_protocol.h`, `tools/gen_protocol.py`, `bootrom/gen/protocol.inc`, `tools/fcpico/protocol.py` | `python3 tools/gen_protocol.py --check`; `pytest tests/protocol` (72) |
| Flash layout header + checker; Markdown link checker; requirements; setup script | `port/flash_layout.h`, `tools/` | `pytest tests/tools` (20) |
| py65 NMI cycle harness, tutorial ROM measured (577 critical / 1724 worst) | `tests/bootrom/` | `pytest tests/bootrom` (17) |
| pioemu tests for `fcppu_w`, `fcppu_rna`, `fcppu_r` (`fcppu_dir` skipped) | `sim/pioemu/` | `pytest sim/pioemu` (13 passed, 1 skipped) |
| `fcppu.pio` byte-identical copy; boot ROM version stamp | `fcbus/fcppu.pio`, `bootrom/version.inc` | `cmp` against `tutorial_project/tuto1_hw/sys/pio/fcppu.pio` |

Run everything: `python3 -m pytest doom/tests doom/sim/pioemu -q` from the repo root.

### Committed as WORK IN PROGRESS, untested (this commit)

The agent writing these was stopped before writing tests; the files parse but have never been
run. Treat as drafts against the specs in `plan/06-audio.md` and `plan/07-bootrom.md`:

- `tools/audio/apus.py` -- `.apus` writer/reader and `thin_register_writes()`. Known open fix
  the author had identified: within one frame, judge register writes against the register
  state at the *start* of that frame (the low-period register must not be updated before the
  high-period register's "period changed" check runs).
- `tools/audio/dpcm.py`, `sfx2dpcm.py`, `dpcm_pack.py` -- 1-bit DPCM encoder/decoder, lump/WAV
  CLI, bank packer for `$C000`-`$ECFF`.
- `tools/nes/bin2c.py`, `bincut.py`, `check_fixbank.py` -- replacements for `Bin2C.exe`,
  `bin_catcut.exe`, plus the fix-bank equality check.
- `tools/respack.py` -- replacement for `binlink.exe` (may be incomplete).

Tests still to write for them (spec in the original task): `tests/tools/test_apus.py`,
`test_dpcm.py`, `test_respack.py` (must reproduce `tutorial_project/tuto1_hw/res/res.bin`
byte for byte: index of int32 (offset,size) pairs then concatenated files, no padding;
entries at 32/32816/40148/44244), `test_nes_tools.py` (`bincut` of 4096 bytes at `0x7010` of
`BOOTROM_FIX/PG_main.nes` must equal `BOOTROM/bootrom_fixr.bin`, MD5
`1A9A2AC85F1E7A15AF9DF47013432BF9`).

### Started but NOT delivered (directories exist and are empty; redo from the plan)

Three delegated jobs ended without output. Their specifications are complete in the plan and
in the prompts summarised here:

1. **PPU-bus model + decoder + reference quantizer** (`sim/ppubus/ppubus.py`,
   `tools/fcpico/stream.py`, `tools/fcpico/nes_palette.py`, `tools/ppu_decode.py`,
   `tools/fcvideo_ref.py`, tests under `tests/tools/`): plan 04 and 09 L3, tasks P0-T7 and
   P1-T2. Layout facts: word index `31 + 34*y + x`; low byte = bitplane 0, high byte =
   bitplane 1, pixel 0 in bit 7; words 32-33 = next line's first 16 px; mailbox at byte
   15426; buffers 17344 (v1) / 17408 (v2). Model parameters: `reads_per_line` (default 64),
   `prerender_reads`, `cs1_mask`, `mailbox_len`, and `osr_prelude_bytes` (default 4, see
   01 "A four-byte prelude") -- defaults must give 15490 (v1) / 15554 (v2) reads per frame.
2. **`fcbus` core + host backend + C tests + host-only CMake** (`fcbus/fcbus_core.[ch]`,
   `fcbus/fcbus_host.[ch]`, `tests/fcbus/*.c`, `doom/CMakeLists.txt` with
   `-DFCPICO_HOST_ONLY=ON`): plan 02 "API sketches", 03, 09 L1; tasks P0-T5 (core part) and
   P0-T6. The host backend must serve the 4-byte OSR prelude (minus nudges) after each
   re-arm, then the front buffer; every served byte counts as a qualifying read.
3. **Host multicore shim** (`sim/host_shim/`): plan 08 "host build" paragraph; task P0-T3.
   pico-sdk 2.1.1 host platform (a checkout was at `/home/user/pico-sdk`, not preserved)
   provides only the `pico_multicore` header; implement launch/FIFO/lockout with pthreads,
   build standalone with `PICO_PLATFORM=host`, test FIFO + `sem_t` + `time_us_64`.

## Facts learned this session that are not yet in the work plan

- `plan/10-workplan.md` task statuses were not updated. Done or partly done: P0-T4 (done),
  P0-T8 (done), P2-T2 harness (done for the tutorial ROM), P0-T1/T12 (CI templates only),
  P4-T3 and P2-T1 tool replacements (WIP above). Update the table and `PROGRESS.md`.
- Prior-art clones used for reading (not preserved): `axsann/fc_pico_gb`, `rasteri/pipu`.
  Everything extracted from them is in `plan/01`, `04`, `06`, `09`, `11`.
- Open questions Q3, Q4, Q10 are partly answered in `plan/11-risks.md`; Q5 (FamiStudio
  CLI option names) needs `FamiStudio -help` on a machine that can run it.

## Human-only items outstanding

`HARDWARE-REQUESTS.md`: HR-1 (trace capture; needs the test-pattern firmware from P0-T5,
which does not exist yet) and HA-1 (licensing enquiry to impact soft).

## Suggested order for the next session

1. Write the four missing test files for the WIP tools; fix `apus.py` as noted; commit.
2. Redo item 2 (fcbus core, host backend, CMake host-only build) -- it unblocks everything
   downstream (model, co-sim, test-pattern firmware).
3. Redo item 1 (PPU-bus model + decoder + reference quantizer).
4. Redo item 3 (host shim), then P0-T1/P0-T2 (superbuild + engine skeleton), which need
   pico-sdk and arm-none-eabi-gcc 13.2.Rel1 (`tools/setup_env.sh`).
5. Update `plan/10-workplan.md` statuses and `PROGRESS.md`; activate CI (P0-T12).
