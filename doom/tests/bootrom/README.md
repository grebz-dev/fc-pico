<!-- SPDX-License-Identifier: BSD-3-Clause -->

# `tests/bootrom` -- NMI cycle measurement

`nmi_harness.py` runs a boot ROM's vertical-blank (NMI) handler under the
[py65](https://github.com/mnaberez/py65) 6502 simulator and measures how many
CPU cycles it takes, from the fixed `ROM_NMI_ENTRY` trampoline (`$ED00`)
through `RTI`. `test_tutorial_nmi.py` points it at the *tutorial* boot ROM
(`tutorial_project/BOOTROM/rom.NES`) and is what pins down the numbers used
in `doom/plan/07-bootrom.md` and `doom/plan/01-constraints.md`; a future
`test_doom_nmi.py` will point the same harness at `doom/bootrom/out/doom.nes`
once that ROM exists (see "Pointing this at the Doom ROM" below).

## Running

```sh
python3 -m pytest /home/user/fc-pico/doom/tests/bootrom -q -s
```

`-s` is what makes `test_cycle_table_report`'s printed table visible; without
it pytest captures stdout and you'll only see pass/fail dots. The command
works from any working directory. To run just one test, or see every test
name:

```sh
python3 -m pytest /home/user/fc-pico/doom/tests/bootrom -q -s -k cycle_table
python3 -m pytest /home/user/fc-pico/doom/tests/bootrom -v
```

Every run overwrites `tutorial_nmi_cycles.json` in this directory (see
below) -- that file is generated output, not checked-in fixture data.

## What gets measured

`NmiHarness.run_nmi(mailbox, ram_preset)` (in `nmi_harness.py`):

1. builds a fresh 64 KB address space with the ROM's 32 KB PRG mapped at
   `$8000`-`$FFFF` (mapper 0 / NROM-256, no CHR -- the whole address space is
   flat, no bank switching);
2. installs a fake PPU: `$2002` always reads `$80` (vblank set); `$2007`
   serves one dummy byte and then the bytes of `mailbox` in order, however
   many times it's read, regardless of what's been written to `$2006`
   (see "Why the fake `$2007` ignores `$2006`" below); every write to
   `$2000`-`$2007` or `$4000`-`$4017` is logged as `(cycle, address, value)`;
3. writes `ram_preset` (merged over `DEFAULT_RAM_PRESET`, caller wins) into
   the fresh RAM;
4. reproduces the CPU's own interrupt-entry sequence by hand -- pushes PCH,
   PCL, then P for a fake return address, sets `PC = $ED00` -- and then calls
   `mpu.step()` repeatedly until the opcode about to execute is `RTI`
   (`$40`), which it then executes too so its 6 cycles are counted.

The result (`NmiResult`) reports, among other things:

| Field | Meaning |
|---|---|
| `raw_cycles` | `mpu.processorCycles` from the `jmp NMi` at `$ED00` through `RTI`, inclusive. Exactly what py65 counted; no corrections. |
| `entry_cycles` | Constant `7`: the real 6502's fixed NMI-recognition + vector-fetch cost. `mpu.step()` never executes this (there's no instruction for it), so it can't be in `raw_cycles`; it's `ENTRY_CYCLES` in the module and always `7` on `NmiResult`. |
| `extra_cycles` | `513` for every `$4014` write observed. See "DMA stall" below. |
| `corrected_cycles` | `raw_cycles + extra_cycles`. |
| `total_cycles` | `corrected_cycles + entry_cycles` -- the full real-hardware-equivalent cost, from the NMI line's falling edge through `RTI`'s last cycle. **This is the number to compare against a real vblank budget** (`NTSC_VBLANK_CYCLES = 2273`). |
| `critical_section_cycles` | The `cycle` recorded at the *second* write to `$2005` (same 0-based-at-`$ED00` basis as `writes`), or `None` if there weren't two. This is "everything that can affect what gets rendered next frame has now happened" -- see `doom/plan/07-bootrom.md`'s "vblank-critical subtotal". |
| `writes` | `(cycle, address, value)` for every `$2000`-`$2007` / `$4000`-`$4017` write, in order. |
| `apu_writes` | Just the `$4000`-`$4013` writes from `writes`, as `(address, value)` -- the decoded APU register replay. |
| `reads_2007` | How many `$2007` reads happened (dummy included). |
| `final_pc` | `mpu.pc` right after `RTI`. Equals `SENTINEL_RETURN` (`$BEEF`) iff the handler's stack use was balanced -- a sanity check that it didn't run off into the weeds. |

`cycle_table()` runs the harness over `apu_pairs in {0, 8, 16, 24}` x
`sprite_dma in {on, off}` (driven through `PALFADE_VAL`, which is what the
tutorial NMI's own `$4014` write is conditioned on) and returns one row per
combination; `test_cycle_table_report` prints this as a table and always
writes it to `tutorial_nmi_cycles.json` as:

```json
{
  "rom": "tutorial_project/BOOTROM/rom.NES",
  "ntsc_vblank_cycles": 2273,
  "rows": [
    {"apu_pairs": 0, "sprite_dma": "on", "raw_cycles": 636,
     "corrected_cycles": 1149, "total_cycles": 1156,
     "critical_section_cycles": 577, "entry_cycles": 7, "extra_cycles": 513,
     "reads_2007": 65, "num_writes": 10},
    ...
  ]
}
```

(That's the actual current output for row 1 -- see the checked run's printed
table for all 8 rows.) Note `critical_section_cycles` is the same across
every row: nothing that varies by APU-pair-count or sprite-DMA setting runs
before the second `$2005` write in `SysNMI.asm` -- both the `PALFADE_VAL`/
`$4014` check and the APU replay loop come later. That invariance is a good
sanity check that the measurement is wired up correctly, not a bug.

## Known approximations

- **DMA stall is a flat `+513`, not `513` or `514`.** Real hardware's OAM
  DMA takes 513 cycles when `$4014` is written on an even CPU cycle and 514
  on an odd one; py65's `step()` only ever counts the 4-cycle `sta $4014`
  itself (a plain absolute store -- see `MPU.opSTA`), knowing nothing about
  DMA. This harness always adds 513, which can under-count a real run by up
  to 1 cycle. Given a roughly 2000-cycle budget this is not worth chasing
  further, but it means `total_cycles`/`corrected_cycles` are a lower bound,
  not an exact figure, whenever sprite DMA fires.

- **A write's logged `cycle` is the count at the *start* of the instruction
  that performs it, not the exact bus cycle within that instruction.** py65
  only adds an instruction's cost to `mpu.processorCycles` after the whole
  instruction (`MPU.step()`) has run, so a write logged mid-instruction sees
  the *pre*-instruction running total. This is a fixed, consistent offset
  (at most one instruction's worth of cycles, 2-7 here) and does not affect
  ordering between writes, only how literally to read an individual
  timestamp.

- **`entry_cycles` (7) is a napkin constant, not something py65 executed.**
  Real 6502 hardware spends 7 cycles recognising an asserted NMI line and
  fetching the vector before the first instruction of the handler runs; there
  is no corresponding instruction for `step()` to execute, so this harness
  never measures it -- it just adds the textbook constant on top of what was
  actually simulated (see `doom/plan/01-constraints.md`'s "NMI entry + `rti`
  ~13 cycles" row: `7 + 6`, the 6 being `RTI`'s own, already-measured cost).

- **Interrupt entry is faked directly at `$ED00`, not via `$FFFA`/`$FFFB` or
  `mpu.nmi()`.** The harness pushes a fake PCH/PCL/P by hand and sets
  `mpu.pc = $ED00` itself, rather than pointing the CPU at the real NMI
  vector and letting it read `ROM_NMI_ENTRY` from there. For both the
  tutorial ROM (verified byte-for-byte against `rom.NES`: `$FFFA`/`$FFFB` =
  `$ED00`) and the planned Doom ROM (`ROM_NMI_ENTRY` is a fixed fix-bank
  contract, `doom/plan/07-bootrom.md`) these are the same address, so this
  is a shortcut, not a divergence -- but it's a shortcut that assumes the
  vector really does point at `$ED00`, worth re-checking if this harness is
  ever pointed at a ROM that might not honour that contract.

- **No nested NMI, no IRQ.** The real handler guards against re-entrancy
  itself (`NMI_FLG`, checked and set at the very top of `SysNMI.asm`); this
  harness runs exactly one call to completion and never asserts a second
  interrupt mid-handler, matching the "should not happen" status of that
  guard rather than exercising it.

- **RAM starts zero-filled; only what a run's traced path reads is preset.**
  Real hardware RAM is undefined at power-on. `DEFAULT_RAM_PRESET` pins
  every address the tutorial NMI's *measured* path reads before writing it
  (`NMI_FLG`, `PALFADE_VAL`, `PAL_CHG_FG`, `FLG_2000`, `FLG_2001`, `KEY_NEW`,
  `PICO_COM`, `NMI_CALL_ADR` high byte); anything else it touches (the
  `NMI_SVx` scratch slots, `SYS_TIMER`, the mailbox zero page itself) is
  written by the handler before being read, so a zero-filled start has no
  effect on the cycle count either way.

- **Page-crossing cycle costs are py65's problem, correctly, and this
  harness doesn't touch them.** Checked directly against the installed py65
  1.2.0 (`py65/devices/mpu6502.py`):
  - Taken relative branches add the page-cross cycle in `BranchRelAddr`
    (line ~166), gated by each branch opcode's declared `extracycles=2`.
  - Indexed *loads* (`LDA`/`LDX`/`LDY`/`CMP`/`ADC`/`SBC`/... in `abs,X`,
    `abs,Y`, `(zp),Y`) add it conditionally in `AbsoluteXAddr`/
    `AbsoluteYAddr`/`IndirectYAddr` (lines ~146-165), again gated per-opcode.
  - Indexed *stores* (`STA abs,X` = opcode `0x9D`, `STA abs,Y` = `0x99`,
    `STA (zp),Y` = `0x91`) are declared with a fixed `cycles=5` or `6` and
    no `extracycles` at all (lines 954/963/929) -- i.e. py65 already charges
    them the real, unconditional extra cycle real hardware always takes for
    an indexed store, rather than making it conditional on whether the
    address actually crossed a page the way it does for loads.

  The one indexed-absolute store in the handler this harness exercises,
  `sta $4000,y` in the APU replay loop, therefore gets its correct fixed
  5-cycle cost regardless of `y`. Nothing here needed a workaround.

- **Why the fake `$2007` ignores `$2006`.** This isn't a simplification of
  the *protocol* -- per `docs/pages/protocol.md`, `$2006` is not
  addressing real memory here at all; parking the PPU address in
  pattern-table space just asserts the cartridge's chip select, and the
  cartridge then answers a flat stream of qualifying reads it's already
  counting for frame sync. Modelling `$2007` as "the next byte of a fixed
  stream, whatever `$2006` says" is the accurate model, not an approximate
  one; it's called out here only so it's clear the harness isn't
  accidentally ignoring something it should track.

## Pointing this at the Doom ROM

`doom/bootrom/out/doom.nes` does not exist yet. Once it does, per
`doom/plan/07-bootrom.md`:

- The entry point is unchanged: `ROM_NMI_ENTRY = $ED00`, same `jmp NMI`
  trampoline contract, same fixed fix-bank boundary. `NmiHarness`'s
  interrupt-entry setup and `NMI_ENTRY` constant need no changes.
- The mailbox grows to 128 bytes, split across two zero-page windows:
  bytes 0..63 land at `$20` (as today) and 64..127 at `$C0`. **This needs no
  harness change either.** The fake `$2007` model only ever serves "the
  next byte of a flat stream" -- it has no idea which zero-page address the
  ROM's `sta` eventually targets, so `build_mailbox(pairs, size=128)` (or a
  Doom-specific builder, if the v2 layout turns out to need different
  content past offset 64) is enough; `harness.run_nmi()` will correctly
  serve 1 dummy + 128 bytes to however many `lda $2007`s the v2 NMI issues.
- `NmiHarness(load_ines(doom_nes_path))` also needs no change, provided
  `doom.nes`'s PRG is still exactly 32 KB mapped flat at `$8000`-`$FFFF`
  (`doom/plan/07-bootrom.md`'s bank layout: 4 x 8 KB banks, no mapper
  switching) -- if that ever stops being true, `NmiHarness.__init__`'s
  32 KB-exactly check is the first thing to revisit.
- The write log, `critical_section_cycles` (second `$2005` write),
  `raw_cycles`/`entry_cycles`/`extra_cycles`/`corrected_cycles`/
  `total_cycles` accounting, and the step-until-`RTI` loop are all
  protocol-agnostic and reusable as-is.

What a new `test_doom_nmi.py` **will** need to supply itself, rather than
inheriting from this one:

- **Its own RAM preset.** Only what the fix bank owns across the bank
  boundary is pinned by `doom/plan/07-bootrom.md`: `KEY_NEW` (`$85`),
  `FLG_2000`/`FLG_2001`/`NMI_FLG` (`$B0`/`$B1`/`$B3`) are confirmed to stay
  at the tutorial's addresses. Nothing in the plan docs yet pins where
  `PICO_COM`, the magic-byte offset, or an `ATTR_VALID`/`PAL_VALID`-style
  flag will live for Doom -- `DEFAULT_RAM_PRESET` in `nmi_harness.py` is
  tutorial-specific and should not be assumed to apply; build a
  Doom-specific preset dict from the real `doom/bootrom` source once it
  exists (e.g. its `defs/ram.inc`).
- **A different post-mailbox shape to assert against.** Per
  `doom/plan/07-bootrom.md`'s "v2 NMI" table, Doom's handler does not do
  sprite DMA at all (`$0200`-`$02FF` is explicitly "unused ... Doom never
  does sprite DMA", and `01-constraints.md` lists `$4014` as "not needed by
  Doom -- reclaimed") -- there is no `PALFADE_VAL` check to key a test on.
  Instead there are conditional attribute-table (`$23C0`, 64 bytes) and
  palette (`$3F00`, 16 bytes) upload blocks, each with their own `$2006`
  pair, gated on flags this harness's write log will show once the real
  addresses are known.
- **Confirmation, not assumption, that the magic byte is still at mailbox
  offset 1 with value `$FC`.** The v2 RAM map table describes bytes 0-63 as
  "v1 layout: flags, magic, commands, APU pairs, palette", which suggests
  it is, but this harness should not be pointed at `doom.nes` and expected
  to pass tutorial-shaped assertions without checking that against the
  actual source first.

A sketch of what that test file's setup will look like:

```python
from pathlib import Path
from nmi_harness import NmiHarness, load_ines, build_mailbox

DOOM_ROM = Path(__file__).resolve().parents[2] / "bootrom" / "out" / "doom.nes"
harness = NmiHarness(load_ines(DOOM_ROM))
mailbox = build_mailbox(apu_pairs, size=128)  # or a doom-specific mailbox builder
result = harness.run_nmi(mailbox, doom_ram_preset)  # doom_ram_preset: TBD, see above
```
