# 09 -- Testing and CI

Hardware sessions are the scarce resource. The pyramid below is built so that every class of
failure that has been observed or predicted has a level that can catch it *before* a console
is involved, and so that most of it runs on every push.

```
 L7  Hardware rig (Famicom + cartridge + serial CLI)         manual, per milestone
 L6  Mesen2 co-simulation (real 6502 boot ROM + firmware model)   nightly + on demand
 L5  Full-chip simulation of the PIO/DMA/ISR firmware (rp2040js)  optional, on demand
 L4  PIO program tests (pioemu)                              every push
 L3  PPU-bus model + golden frames (host)                     every push
 L2  Engine host build: deterministic demo runs               every push
 L1  Unit tests: fcbus core, fcvideo, fcapu, input, tools     every push
 L0  Builds, size, generated-file freshness, static checks    every push
```

## L0 -- builds and static checks

- Device build (`rp2350-arm-s`, MinSizeRel) of `fcpico_testpattern` and `fcpico_doom`; host
  build of `fcpico_doom_host`, libraries, `whd_gen`, `mus2mid`, tests.
- `-Wall -Wextra -Werror` on all *new* code (`fcbus`, `fcvideo`, `fcapu`, `port`, `sim`,
  `tools`); the engine keeps its own warning policy.
- `tools/gen_protocol.py --check`: generated 6502 include and Python constants match the header.
- `tools/flash_layout_check.py <elf>`: flash end < `TINY_WAD_ADDR`; WHX end < archive start.
- Size report (`arm-none-eabi-size -A`, top 30 symbols by `nm --size-sort`) as an artifact.
- Boot ROM: reproducible assembly; fix-bank bytes identical to `bootrom_fixr.bin`; tutorial ROM
  MD5 gate proves the Wine toolchain.

## L1 -- unit tests (ctest, pytest)

| Suite | What it pins down |
|-------|-------------------|
| `tests/fcbus/` | mailbox builder (v1/v2 layouts, terminators, overflow refusal), rx dispatcher (all 256 byte values in every state), sync decision table (count = VAL-3..VAL+3 -> stop/nudge2/nudge1/arm), stream word addressing (`fcbus_stream_word(line,tile)` equals `convVram()`'s `31 + 34*y + x`), publish/swap semantics, data-mode DRQ ordering (palette, attributes, step, none) |
| `tests/fcvideo/` | decimation table; block cost against a brute-force Python reference; LUT/dither/pack against a reference implementation (`tools/fcvideo_ref.py`) on synthetic and captured frames; hysteresis; palette-set generation; letterbox and prefetch words |
| `tests/apu/` | see 06 |
| `tests/input/` | see 05 |
| `tests/tools/` (pytest) | `ppu_decode.py` round trip with `fcvideo_ref.py`; `sfx2dpcm.py` decode-back RMS; `vgm2apus.py` cap enforcement; `respack.py` index; `bincut.py`/`bin2c.py` byte-identical to the tutorial's committed outputs |
| `tests/protocol/` | 03's obligations |
| `tests/bootrom/` (pytest + `py65`) | NMI cycle count and register order; `FP_COM_KEY` packet bytes; init sequence writes |

All C unit tests link the `*_host` libraries and run under `ctest --output-on-failure`; ASan/UBSan
in a second configuration.

## L2 -- engine host runs

`fcpico_doom_host` (the full engine with the fcpico platform layer and the `fcbus` host
backend) supports:

```
fcpico_doom_host --whx doom1.whx --demo 1 --frames 600 --dump-8bit out/ --dump-stream out/ --pads pads.txt
```

- `--demo N --frames F`: run DEMO N for F console frames with a synthetic 60.1 Hz heartbeat
  (the host backend calls the ISR-equivalent on a timer or, in `--lockstep` mode, once per
  converted frame for determinism).
- `--dump-8bit`: the composed 320x200 8-bit frame + `PLAYPAL` as PNG per frame.
- `--dump-stream`: per frame, the 17,344/17,408-byte stream, the mailbox, and the expected
  count, as `.bin` + JSON.
- `--pads`: scripted controller input per frame.
- Determinism: two identical runs produce identical dumps (Doom's demo playback is
  deterministic; the host backend must be too -- no wall-clock dependence).

Golden tests (`tests/goldens/`):

- `demo1/`: SHA-256 of every 10th frame's stream+mailbox for the first 600 frames, plus the
  PNG of every 100th decoded frame for humans.
- PSNR gate: `ppu_decode.py` decodes the stream to 256x240 RGB using the NES palette table; the
  8-bit source is decimated and mapped through `PLAYPAL`; PSNR must not fall below the stored
  per-frame threshold (stored on first acceptance; may only be raised).
- `tools/update_goldens.py` regenerates hashes and thresholds; the diff is reviewed like code.

## L3 -- PPU-bus model (`sim/ppubus/`)

A C library with a Python (`ctypes`) wrapper that models the **console side** of the bus at the
level of qualifying read strobes and `$2007` traffic, without emulating the 6502 or the PPU's
video output:

- `ppubus_frame_reads(params)`: the sequence of qualifying reads in one NTSC frame (pre-render
  line, 240 visible lines, then the NMI's dummy + mailbox reads), parameterised by
  `reads_per_line`, `prerender_reads`, `cs1_mask`, `mailbox_len`.
- `ppubus_run(cart, frames)`: pulls bytes from the `fcbus` host backend exactly as the PPU would
  (through `fcbus_host_ppu_read()`), delivers the NMI-time `$2007` writes (`FP_COM_KEY` packet
  or raw byte), and reconstructs the image from the bytes it received, the attribute table and
  palette carried in the mailbox, using the same tile/plane/quadrant rules as the PPU.
- Fault injection: drop or duplicate N reads in frame K, hold the heartbeat for M frames, send
  a v1 raw byte in v2 mode -- to test the resync logic and the state machine.

**Calibration** (P0-T10): the parameters must reproduce `PPU_COUNT_VAL` = 15,490 for v1 and the
measured per-line strobe timeline from the hardware trace (`tests/fixtures/hw_trace_ntsc.json`).
Until calibrated, the defaults reproduce the firmware's buffer layout and the model is marked
`UNCALIBRATED` in its output; golden hashes produced by an uncalibrated model are still valid as
regression hashes but not as correctness evidence.

`tools/ppu_decode.py` is the same reconstruction in pure Python, used by L2 and for humans.

## L4 -- PIO program tests (`sim/pioemu/`, pytest)

Using `rp2040-pio-emulator` (Apache-2.0, `pip install rp2040-pio-emulator`; assembles with
Adafruit's `pioasm`), one state machine at a time:

| Program | Test | Emulator caveat |
|---------|------|-----------------|
| `fcppu_w` | drive `/WR` and `CS1` waveforms; assert one word pushed per `/WR` rising edge only when `CS1` was low at the falling edge; low byte equals the data pins | -- |
| `fcppu_rna` | N qualifying reads and M non-qualifying -> ISR value after `push` equals N (accounting for the `~x` encoding) | -- |
| `fcppu_r` | with autopull, bytes appear on pins in order only during qualifying `/RD`-low windows | the `irq set` instruction is **not supported by pioemu**: assemble a variant with it replaced by `nop`; the bus-direction handshake is covered at L5/L6/L7 |
| `fcppu_dir` | not testable in pioemu (needs `irq`/`wait irq`) | document; L5 covers it |

If pioemu's instruction coverage grows (it is actively developed and claims RP2350 support),
extend. Alternative simulators surveyed: the browser-based RP2040/RP2350 PIO simulator noted by
Adafruit in May 2026 (name and headless capability to be confirmed), Renode_RP2040's C++ PIO
model, rp2040js's PIO (tested against silicon).

## L5 -- full-chip simulation (optional, `sim/fullchip/`)

Purpose: validate the PIO + DMA + IRQ integration of the real firmware binary, which L4 cannot
(single SM, no DMA) and L6 does not (host backend). Candidates as of this writing:

| Simulator | Language / licence | PIO | DMA | RP2350 | Notes |
|-----------|--------------------|-----|-----|--------|-------|
| `wokwi/rp2040js` | TypeScript, MIT | yes (tested against silicon) | yes | **no** | runs UF2 headless in Node; GPIO listeners |
| `matgla/Renode_RP2040` | Renode + C++ PIO, MIT | external C++ model | yes | no | "WIP and frozen"; `.resc` scripting |
| `PicoSimulator/PicoSimulator` | C++, WIP | yes | yes | no | headless CLI, external device hooks |

None simulates the RP2350, so this level runs an **RP2040 build of `fcpico_testpattern`** (the
bus layer is chip-agnostic per `docs/pages/hardware.md`; `PICO_BOARD=pico`, `PICO_PLATFORM=rp2040`).
The harness (`sim/fullchip/rp2040js/`) drives GP17/GP20/GP21 from the L3 model's timeline,
samples GP6-GP13 and their direction on each `/RD` low, sends `$2007` writes via GP21, and
asserts the received stream equals what the host backend would produce for the same frames.
Time-boxed; if rp2040js's GPIO timing makes the bus-turnaround check unreliable, keep only the
byte-order check.

## L6 -- Mesen2 co-simulation (`sim/mesen2/`)

The pre-hardware gate. A cycle-accurate NES emulator (Mesen2, GPL-3, cross-platform, headless
test runner: `Mesen --testRunner script.lua rom.nes --timeout=N`, exit code from `emu.exit(n)`)
runs the **real boot ROM**; a custom mapper forwards the cartridge's side of the PPU bus to the
**firmware model** (the host build of `fcbus` + `fcvideo` + the engine, as a static library).

### The mapper

- Source under `sim/mesen2/mapper/` and applied to a pinned Mesen2 checkout by a patch or a
  fork submodule (`sim/mesen2/Mesen2`). Registered under a private NES 2.0 mapper number
  (choose one Mesen does not implement, e.g. 4093; the `.nes` header of `doom.nes` is patched
  by `tools/nes/set_mapper.py` for co-sim only -- the console image stays mapper 0).
- `EnableCustomVramRead() -> true`; `MapperReadVram(addr, type)`: for `addr & cs1_mask`
  (default `< 0x2000`; parameter from calibration) and rendering/CPU reads only (not debugger
  reads), return `fcpico_cart_ppu_read()`; else fall through to the base mapper's CHR RAM.
  `MapperWriteVram(addr, value)`: same predicate -> `fcpico_cart_ppu_write(value)`.
- The cart model (`sim/cartmodel/`, C API) wraps the `fcbus` host backend: reads pull from the
  current stream buffer and count; writes go through the rx dispatcher; the heartbeat runs the
  ISR-equivalent synchronously on the emulator thread (swap buffers, count check); the engine
  runs on its own thread(s) with the same semaphores as on device. `fcpico_cart_create(mode)`
  selects `testpattern` or `doom` (the latter needs `--whx`).

### Lua scenarios (`sim/mesen2/lua/`)

| ID | Scenario | Checks |
|----|----------|--------|
| S0 | tutorial boot ROM + test-pattern model | pattern visible within 120 frames (screenshot hash); `ppu_count` histogram is a single value |
| S1 | tutorial boot ROM + Doom model | reflash path reaches `UR_MAIN_SETUP` of the Doom ROM; `FP_COM_HELLO 2` observed; title screen hash |
| S2 | Doom ROM + Doom model, DEMO1, 3000 frames | every 100th frame screenshot vs golden (perceptual hash distance <= T); NMI `rti` scanline < 261 every frame; `$23C0`-`$23FF` and `$3F00`-`$3F0F` equal the model's mailbox for that frame; zero resync events |
| S3 | scripted play (`emu.setInput`) | walk, open door, change weapon; assert engine state via the model's log |
| S4 | audio | `$4000`-`$4017` write log per frame equals the sequencer's log; <= 16 pairs per NMI |
| S5 | save/load | save in E1M1, reset, load; `SAVING` video type shows the frozen frame; no DMA under-run |
| S6 | soak (nightly) | 100,000 frames of demos looping; counters: resyncs, drops, conversion max |

Lua API used: `emu.addEventCallback(fn, emu.eventType.endFrame)`, `emu.addMemoryCallback(fn,
emu.callbackType.write, 0x4000, 0x4017)` and `callbackType.exec` on the `rti`, `emu.getState()`
(scanline), `emu.read()`, `emu.setInput()`, `emu.takeScreenshot()`, `emu.log()`, `emu.exit(code)`.
Verify exact names against the pinned Mesen2's `LuaDocumentation.json`.

### CI

`ci/workflows/doom-cosim.yml`: nightly and `workflow_dispatch`; builds Mesen2 (Linux, .NET 8,
`make`) with a cache keyed on the Mesen2 commit and the mapper sources; builds the cart model;
runs S0-S5; uploads screenshots and logs; fails on any assertion. Run time target < 20 min.

## L7 -- hardware

Rig: Famicom (a genuine one; the FC PICO manual notes clones may fail) or AV Famicom, the
cartridge, USB-C to a PC for the serial CLI (115200), a TV or capture device, optionally a
NES front-loader with a 60-to-72-pin adapter that provides a CIC (or a console with the CIC
disabled) for the NES claim.

Serial CLI (part of `port/cli.c`, both firmwares):

| Command | Output |
|---------|--------|
| `stats` | fps, conversion min/avg/max us, `ppu_count` histogram (last 256 frames), resync events, heartbeat timeouts, apu drops, free zone |
| `pattern N` | test-pattern firmware: select pattern (bars, checker, attribute alternation, counter) |
| `trace` | capture one frame of CS1//RD//WR samples via PIO1 + DMA (sample period ~40 ns, packed 10 samples per word), dump as hex; decoded by `tools/trace_decode.py` |
| `dump attr` / `dump pal` / `dump mailbox` | last published values |
| `reboot`, `bootsel` | |

Checklist per milestone lives in `10-workplan.md`; results are appended to
`doom/HARDWARE-LOG.md` by the human running the session (template provided). An agent that
cannot run hardware records what it needs in `doom/HARDWARE-REQUESTS.md` and continues with
everything that does not depend on it (see 12).

## GitHub Actions

Templates in `doom/ci/workflows/`, activated in P0-T12 by copying to `.github/workflows/`:

| Workflow | Trigger | Jobs |
|----------|---------|------|
| `doom-build.yml` | push/PR touching `doom/**` | `firmware` (arm-none-eabi 13.2.Rel1, pico-sdk 2.1.1, UF2/ELF/map/size artifacts, layout check), `host` (host libs + engine host build + ctest + pytest + L2 goldens), `bootrom` (Wine + nesasm, MD5 gate, reproducibility, py65 tests) |
| `doom-sim.yml` | push/PR | `pioemu` (L4); `fullchip` (L5, `continue-on-error: true` until stable) |
| `doom-cosim.yml` | nightly 03:00 UTC + manual | Mesen2 build (cached) + S0-S5 |
| `doom-docs.yml` | push touching `docs/**`, `doom/**/*.md` | Doxygen with the repo's zero-warning rule, artifact `docs/html` |

Pinned action versions and toolchain tags are in the templates; bump deliberately.
