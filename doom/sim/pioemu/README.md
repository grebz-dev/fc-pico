<!-- SPDX-License-Identifier: BSD-3-Clause -->

# L4 -- PIO program tests (`sim/pioemu/`)

pytest tests for the four PIO programs in `doom/fcbus/fcppu.pio` (`fcppu_w`, `fcppu_r`,
`fcppu_dir`, `fcppu_rna`), using `rp2040-pio-emulator` (`pioemu` 0.88.0) to assemble and run one
state machine at a time. See `doom/plan/09-testing-ci.md`, section "L4", for where this sits in
the overall test pyramid, and `doom/fcbus/fcppu.pio` itself for what each program does.

Every test assembles the **real** `fcbus/fcppu.pio` through `conftest.py`'s `load_program()` --
never a copy of the source -- so an edit to the real file is what these tests actually exercise.

## Run it

```sh
python3 -m pytest doom/sim/pioemu -q
```

Requires Python 3.11+, `rp2040-pio-emulator` 0.88 (`pip install rp2040-pio-emulator`,
`import pioemu`), `adafruit-circuitpython-pioasm` (`import adafruit_pioasm`) and `pytest`.
Current status: 13 passed, 1 skipped (`fcppu_dir`'s behavioural test -- see below).

## Coverage

| Program | File | Covered here | Not covered here |
|---|---|---|---|
| `fcppu_w` | `test_fcppu_w.py` | One word pushed to the RX FIFO per selected `/WR` pulse, none for unselected pulses; the pushed byte matches D0..D7; the byte is sampled at (just after) the `/WR` rising edge, not an earlier value present during the low phase | -- |
| `fcppu_rna` | `test_fcppu_rna.py` | X decrements once per qualifying `/RD`; the ISR holds the running count as `mov isr, !x` would leave it; unaffected by non-qualifying reads and by `/WR` activity | -- |
| `fcppu_r` | `test_fcppu_r.py` | With the OSR primed (steady state) and the TX FIFO preloaded, qualifying reads shift bytes onto D0..D7 low-byte-first, in FIFO order; unselected reads consume nothing; the `mov osr,null`-driven cold-start transient (first 32 bits shifted are zero) is pinned down as its own test | The `irq set IRQ_DIR` -> `fcppu_dir` handshake (see below) |
| `fcppu_dir` | `test_fcppu_dir.py` | Only that the program still assembles | Everything behavioural: needs `irq wait`/`irq clear`, unsupported by pioemu 0.88 -- covered by L5 (full-chip simulation) and L7 (hardware) instead, per plan/09 |

`fcppu_r`'s and `fcppu_dir`'s IRQ handshake (`irq set IRQ_DIR` / `wait 1 irq IRQ_DIR` /
`irq clear IRQ_DIR`) is the one piece of behaviour no test in this directory exercises, for the
reasons in "Emulator limitations" below.

## Files

| File | Contents |
|---|---|
| `conftest.py` | `load_program(name)`: reads and assembles one `.program` block from the real `fcppu.pio`, working around the adafruit_pioasm quirks below. Also `pin_defines()` (the file's `.define`d GPIO numbers, as ints), `timeline_source()` (turns a per-cycle GPIO-word list into a pioemu `input_source`), and `NOP_OPCODE`/`patch_opcode()` (for neutralising the one unsupported instruction in `fcppu_r`). |
| `test_fcppu_w.py` | Receive-path (byte capture on `/WR`) tests. |
| `test_fcppu_rna.py` | Read-counter tests. |
| `test_fcppu_r.py` | Transmit-path (byte-out on `/RD`) tests, with the `irq` instruction patched out. |
| `test_fcppu_dir.py` | Assembly-only check; the behavioural test is `skip`ped. |

## Emulator API facts relied on (`pioemu` 0.88.0)

Found by reading `pioemu`'s installed source (`python3 -c "import pioemu; print(pioemu.__file__)"`
-> `.../site-packages/pioemu/`; no `docs/` directory ships in the wheel) and confirmed empirically
against small hand-built programs before relying on them. `emulate()`'s full signature is in
`emulation.py`:

```python
def emulate(opcodes, *, stop_when, initial_state=None, input_source=None,
            auto_pull=False, auto_push=False, pull_threshold=32, push_threshold=32,
            shift_isr_right=True, shift_osr_right=True,
            out_base=0, out_count=32, set_base=0, set_count=5,
            side_set_base=0, side_set_count=0, jmp_pin=0,
            wrap_target=0, wrap_top=0) -> Generator[Tuple[State, State], None, None]
```

- **How pins are driven over time -- the only mechanism there is.** `input_source` is a callable
  invoked once per emulated clock cycle, *before* that cycle's instruction is decoded. It is
  called with the current `State` if its single parameter is annotated `State`, or with
  `state.clock` (an `int`) if annotated `int` or left unannotated (the latter also logs a
  warning). Its 32-bit return value only takes effect on GPIOs where `pin_directions` is 0
  (configured as inputs); bits where `pin_directions` is 1 keep whatever the state machine last
  wrote (`emulate()`: `pin_values = (old_pin_values & pin_directions) | (input_source(state) &
  ~pin_directions)`). None of the four fcppu.pio programs ever write `pindirs`, so
  `pin_directions` stays 0 throughout every test here, and `input_source`'s return value lands in
  `pin_values` verbatim each cycle -- see `conftest.timeline_source()`.

  **Gotcha:** `from __future__ import annotations` (PEP 563) breaks this. With it active, a
  parameter annotated `state: State` reports its annotation as the *string* `"State"` at
  runtime, not the class; `emulate()` compares the annotation with `==` against the actual
  `State` class and, finding a string, raises `ValueError: Unsupported signature for
  input_source`. Confirmed by adding the future-import to an early draft of `conftest.py` and
  watching a previously-working `input_source` start raising. None of the files in this
  directory use that import, for exactly this reason.

- **`in pins, N` has no base pin -- it always starts at GPIO0.** `emulate()` has no
  `in_base`/`in_count` parameter at all. `in pins, N`'s source is `read_from_pins`, which returns
  the raw `state.pin_values` word unmodified (`primitive_operations.py`); `_decode_in`
  (`instruction_decoder.py`) never applies an offset. So `in pins, 32` in `fcppu_w` reads
  GPIO0-31 as one word, and the data byte (driven on GPIO6-13 on real hardware, where
  `sm_config_set_in_pins(PI_D0_BIT)` rebases the group) instead lands at bits 6-13 of the pushed
  word. `test_fcppu_w.py`'s `_byte_of()` shifts it back down before comparing.

- **`out pins, N` / `out pindirs, N` *do* honour a configurable base**, via `out_base`/`out_count`
  (default 0/32) -- wired to `write_to_pins(out_base, out_count, ...)`. `test_fcppu_r.py` passes
  `out_base=6, out_count=8` (`fcppu_r`'s real `sm_config_set_out_pins(&cr, PI_DATA_SHIFT, 8)`),
  and the byte lands on GPIO6-13 correctly. Verified empirically: with the OSR preloaded to
  `0x000000AB` and `out_base=6, out_count=8`, one `out pins, 8` produces `pin_values == 0xAB <<
  6`.

- **`irq` (set/wait/clear) is not decoded at all.** `decoding/instruction_decoder.py`'s top-level
  dispatch has `# TODO: Add support for MOV, IRQ and SET instructions` and returns `None` for
  IRQ's opcode class; the older `instruction_decoder.py` (still consulted as a fallback for MOV
  and SET, which *are* supported through it) has an explicit `lambda _: None` in the IRQ slot.
  Back in `emulate()`: `if emulation is None: return` -- the generator ends **immediately, with
  no exception and no further yields**, the moment the program counter reaches an `irq`
  opcode. Verified empirically: a 2-instruction program (`irq 7` then `mov y, y`) run with
  `stop_when=clock_cycles_reached(5)` yields zero tuples, not the requested five, and not a
  partial run either -- the `irq` cycle itself never gets a chance to yield.

- **`wait <pol> irq N` assembles, but is emulated exactly like `wait <pol> gpio N`.**
  `WaitInstruction.source` (0=gpio, 1=pin, 2=irq) is decoded but `_decode_wait()` never reads it
  -- it always calls `gpio_high(index)`/`gpio_low(index)`. Verified empirically: assembling
  `wait 1 irq 7` and driving GPIO7 high (leaving the real PIO IRQ flag concept entirely out of
  the picture) unstalls it on the same cycle GPIO7 is observed high. This is why
  `test_fcppu_dir.py` cannot get a *meaningful* result even for the one instruction
  (`wait 1 irq IRQ_DIR`) that does assemble and run -- it would silently test the wrong thing
  (GPIO7, one of the data-bus pins) rather than erroring.

- **`jmp pin, target` branches (`program_counter := target`) exactly when the `jmp_pin` GPIO
  reads high, and falls through otherwise** -- `jmp_conditions[6] = partial(gpio_high, jmp_pin)`
  in `instruction_decoder.py`. Verified empirically with a 3-instruction program where the
  branch and fall-through targets differ (`jmp pin -> 2 / mov y,y / mov x,x`): pin high jumps
  straight `0 -> 2`, pin low falls through `0 -> 1 -> 2`. Since CS1 is active-low, every fcppu.pio
  program's `jmp PIN skip_...` reads as "skip the access when CS1 is high (deselected)" --
  matched by `jmp_pin=CS1` (GPIO17) in every test here.

- **FIFOs are always 4 deep; there is no `PIO_FIFO_JOIN_RX`/`TX` equivalent.**
  `State.receive_fifo`/`transmit_fifo` are plain `collections.deque`s, and the autopush/autopull
  stall checks in `emulation.py` hard-code `len(fifo) >= 4` / `== 4`. There is no `emulate()`
  keyword to widen this to 8 words even though `rp_system.cpp` joins the RX FIFO for `fcppu_w`
  and `fcppu_rna` and the TX FIFO for `fcppu_r`. `test_fcppu_w.py` keeps its selected-write count
  at 3 per run to stay well clear of the 4-word stall.

- **A qualifying `in`/`out` executes one cycle after the `wait` that gated it, not on the same
  cycle.** Each pioemu cycle runs exactly one instruction; a satisfied `wait` advances the
  program counter that cycle, but the instruction it lands on still needs its own following
  cycle to execute. `test_fcppu_w.py`'s "value at the rising edge wins" test times its data
  changes around this explicitly (see that test's docstring); get it wrong and a test can end up
  asserting an off-by-one-cycle value instead of catching a real regression.

- **A `mov` into ISR or OSR always resets that register's shift counter to 0** (`write_to_isr`/
  `write_to_osr`'s default `count=0` in `primitive_operations.py`), so it never by itself counts
  toward `push_threshold`/`pull_threshold`. Two consequences that shaped the tests:
  - `fcppu_rna`'s `mov isr, !x` never triggers autopush, no matter how `in_shift`/autopush is
    configured -- by design, matching the firmware, which reads the running count with an
    explicit `pio_sm_exec(pio0, SM_TRCNT, 0x8000)` ("push noblock") once per frame rather than
    relying on autopush at all. `test_fcppu_rna.py` reads `input_shift_register.contents`
    directly, standing in for that manual `push`.
  - `fcppu_r`'s initial `mov osr, null` leaves the OSR's shift counter at 0, so autopull only
    refills it once 32 bits' worth of `out`s (four qualifying reads) have shifted that
    `null`-loaded zero content out -- *not* on the first qualifying read. Confirmed empirically
    against the real, assembled `fcppu_r` (with the `irq` patched out and the TX FIFO preloaded
    with two words): the first four selected reads output `0x00`, and only the fifth shows the
    real first byte. `test_fcppu_r.py`'s `test_cold_start_shifts_out_zero_before_first_autopull`
    pins this down; the other `fcppu_r` tests start from a hand-built `initial_state` with the
    OSR pre-loaded and the program counter placed at `wrap_target` (*past* that one-time `mov`,
    which would otherwise immediately clobber the priming) to test steady-state byte order in
    isolation -- mirroring how the real firmware primes the FIFO/OSR by hand before the first
    read of a frame (`rp_system.cpp`'s `pio_sm_put_blocking()` calls and the
    `pio_sm_exec(pio0, SM_TRAN, 0x6008)` priming in `ppu_dma()`).

## adafruit_pioasm quirks worked around (`adafruit_pioasm` 1.3.8)

`fcppu.pio` is written for the official, case-insensitive Raspberry Pi `pioasm`. Adafruit's
pure-Python assembler is close but not a drop-in replacement; `conftest.py`'s `load_program()`
preprocesses the extracted text to bridge the gap. Each point below was confirmed by calling
`adafruit_pioasm.Program(text)` directly and reading the exception:

| Input | Result |
|---|---|
| Two `.program` blocks in one `Program(text)` call | `RuntimeError: Multiple programs not supported` -- `fcppu.pio`'s four programs must be sliced apart and assembled one at a time. |
| `.define public FOO 21` / `.define FOO 7` | `RuntimeError: Unknown instruction: .define` -- not implemented at all. Symbolic pin names (`PI_WR_BIT`, `IRQ_DIR`, ...) are expanded to their literal decimal values by text substitution before assembling. |
| `jmp PIN 0` (as fcppu.pio spells every `jmp` predicate) | `ValueError: Invalid jmp condition 'PIN'` -- only lower-case `pin` is recognised. |
| `IRQ clear 7` (as fcppu.pio spells it once, in `fcppu_dir`) | `RuntimeError: Unknown instruction: IRQ` -- only lower-case `irq` is recognised. |
| `irq set 7` (as fcppu.pio spells it, in `fcppu_r`) | `ValueError: invalid literal for int() with base 0: 'set'` -- adafruit_pioasm has no "set" modifier token, only bare/`wait`/`clear`. Dropping "set" (`irq 7`) assembles to the same opcode (`0xC007`) as the default/"set" encoding, so this is a lossless rewrite, not a behaviour change. |
| `.wrap_target` / `.wrap` | Assemble correctly and surface as `Program.pio_kwargs["wrap_target"]` / `["wrap"]`, which map directly to `emulate()`'s `wrap_target=`/`wrap_top=`. |
| `mov y, y` | Assembles to `0xA042`; used as the NOP substitute for the unsupported `irq` opcode in `fcppu_r` (see `conftest.NOP_OPCODE`). |

`load_program()` applies, in order: symbol substitution (`.define` expansion, scanned across the
whole file), then `PIN`->`pin` and `IRQ`->`irq` (word-boundary, so `IRQ_DIR` is untouched), then
`irq set`->`irq`. Ordinary `;`-comments (including fcppu.pio's Doxygen-style `;///` lines) need
no special handling: adafruit_pioasm already strips everything from the first `;` on each line
before parsing it, so a `;///` line reduces to an empty line and is skipped like any blank one. A
trailing `% c-sdk { ... %}` block (present once, after the last program's `.wrap`) is truncated
by `load_program()`'s block-slicer before it ever reaches the assembler, though today it falls
outside every `.program` block anyway.

Every one of the four programs' assembled opcodes was hand-verified once against
`adafruit_pioasm`'s own encoding tables (`CONDITIONS`, `IN_SOURCES`, `OUT_DESTINATIONS`,
`WAIT_SOURCES`, `MOV_SOURCES`/`MOV_DESTINATIONS_V0`) while building this suite, bit for bit --
see the git history of this directory for the working notes if that derivation is ever needed
again.

## When pioemu gains IRQ support

Re-check by re-running the probes above (or just try assembling+emulating `irq set N` /
`wait 1 irq N` / `irq clear N` directly) against the new version. If IRQ instructions decode and
execute:

1. Drop the opcode patch in `test_fcppu_r.py` (`IRQ_INSTRUCTION_INDEX` / `patch_opcode` /
   `NOP_OPCODE`) and assert the real `irq set IRQ_DIR` executes, updating this README's coverage
   table and the "not covered here" cell for `fcppu_r`.
2. Un-skip `test_fcppu_dir.py`'s `test_bus_turns_around_only_during_a_qualifying_read`, implement
   it for real (drive a `/RD` pulse, have a companion `fcppu_r`-shaped IRQ source raise
   `IRQ_DIR`, and assert `pin_directions` for D0..D7 goes to all-1s only for that window), and
   remove its `pytest.mark.skip`.
3. Ideally, run both SMs' opcodes through pioemu *together* (two independent `emulate()`
   generators stepped in lockstep, sharing a single set of GPIO values each cycle) so the actual
   `fcppu_r` -> `fcppu_dir` IRQ handshake is exercised end-to-end at L4, rather than only at L5/L7
   as today. Check whether `emulate()` has grown any multi-state-machine or shared-IRQ-bank
   support first; if not, driving two generators by hand and manually OR-ing an `irq` scratch bit
   between them is the fallback.
4. Update the FIFO-depth note above if joined-FIFO depth (8 words) has also been added --
   `test_fcppu_w.py` and `test_fcppu_rna.py` could then drop their "stay under 4" constraint.
