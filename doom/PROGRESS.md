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

## Plan review and issue set -- 2026-09-11
- Commits: this commit
- Verified: every derivable figure in `plan/` recomputed from the primary sources (the
  tutorial's `rp_system.cpp`, `SysPico.asm`, `fcppu.pio`, the PiPU sources) with a throwaway
  Python script; `python3 tools/check_md_links.py` -> 54 files, 35 links, 0 broken.
- Result: ten arithmetic and consistency defects found and corrected in 01, 02, 03, 04, 06,
  07 and 09, plus three design gaps. The audit trail is `../REVIEW-2026-09-11.md`. The
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
