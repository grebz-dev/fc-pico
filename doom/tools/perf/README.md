# Offline performance checks

These tools execute the real C routines as Cortex-M33 machine code in Unicorn.
Instruction counts measure CPU work, not RP2350 cycles, flash/cache contention,
interrupt latency, or console FPS. Use Arm GCC 13.2.1 from the project toolchain
and `doom/.venv` (includes Unicorn).

## World sprites

From the repository root:

```sh
export PATH="$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH"
doom/.venv/bin/python doom/tools/perf/m33_world_cost.py \
  --atlas .build/doom-world-perf/artifacts/world_atlas.bin \
  --max-instructions 3500000 --max-irq-masked-instructions 7500
```

The shareware atlas with SHA-256
`60720d36310772613521faaee5880e3ecb2a5efb04dad514d5ff13a839c990d8`
drives 72 cases: scale changes, flips, transparency, occlusion, partial tiles,
multiple actors sharing a family, and successive budgeted generations. The
host and ARM runs must produce identical raster pixels, tile/OAM data, palettes,
actor metadata and reuse decisions. The extended workload also prepares a
26-tile generation against a full cache: total exact output is 37,596 bytes.
Unexpected emulator faults,
failure to return, empty workloads, mismatches and instruction-budget excesses
fail the command. The budget is specific to this fixture and compiler.

`--reference-source DIR` compiles the host oracle from a saved pre-change
`fcworld.c` and `fcworld.h`. `--source DIR` selects the ARM source. Without a
reference override, host/ARM agreement checks portability; it does not establish
equivalence to an older implementation. `--output FILE` saves counts, hashes,
compiler version and per-function stack use (excluding callees).

The initial 2026-10-13 run saved its original sources and reports under
`.build/doom-world-perf/evidence/`. The same 3.4M instruction gate fails on those
original sources (5,108,680) and passes on the optimized sources (3,234,675),
with identical output. See [the performance plan](../../plan/14c-world-sprite-performance.md)
for firmware, emulator results and remaining hardware checks.

The distance follow-up extends the fixture with submission timing. Before it,
the firmware masked interrupts around 168,809 instructions of cache search and
OAM preparation. It now masks only ownership reservation (14 instructions in
this benchmark, excluding the IRQ wrapper and stall-flag check); the same
168K preparation instructions run with bus interrupts enabled. The separate
7,500-instruction gate fails on the saved pre-fix source and passes after.
It is a regression budget, not a measured console deadline or proof of flash
elimination. The sources and reports are preserved in
`.build/doom-world-distance/evidence/`. `test_fcworld_interrupt` injects real
heartbeats at C function boundaries during preparation and verifies that
partially prepared packets and held commits stay hidden.

## Background converter

```sh
doom/.venv/bin/python doom/tools/perf/m33_convert_cost.py \
  --playpal /path/to/PLAYPAL.lmp --frame /path/to/frame000100.raw
```

Extract PLAYPAL with `doom/tools/audio/wadextract.py`; record a 320x200 indexed
frame with `fcpico_doom_host --dump-8bit DIR`. The converter checks picture and
attribute bytes against a host result and requires normal emulation completion.
Its millisecond figure uses the old converter's hardware calibration and remains
an estimate. Use `run_world_runtime.py` for ROM/transport FPS modeling; its JSON
report records the timing assumptions and marks them as non-hardware measurements.
