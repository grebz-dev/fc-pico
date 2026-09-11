# ppubus -- L3 PPU-bus model

Pure-Python model of the **console side** of the fcbus PPU bus: the sequence
of qualifying `$2007` read strobes and NMI-time `$2007` write traffic one
NTSC frame produces, without emulating the 6502 or the PPU's own video
timing. See `doom/plan/09-testing-ci.md` ("L3") and
`doom/plan/01-constraints.md` ("The bus contract", "An unresolved
discrepancy", "A four-byte prelude from the PIO itself") for the numbers
this is built from.

**UNCALIBRATED.** The split between "picture" and "pre-render" reads
(`reads_per_line`, `prerender_reads`) is a free parameter until P0-T10
captures a real hardware trace; the defaults only reproduce the firmware's
known *totals* (`PPU_COUNT_VAL_V1` = 15,490, `PPU_COUNT_VAL_V2` = 15,554),
not a claim about which bus fetches those totals actually count. See
`ppubus.py`'s module docstring for the exact accounting and for why
`PpuBus.reconstruct()`/`reference_buffer()` use their own buffer layout
rather than `fcpico.stream`'s byte offsets.

## Contents

- `ppubus.py` -- `PpuBus` (parameterised read/write sequence model:
  `frame_read_count()`, `frame_events()`, `run_frame()`, `reconstruct()`,
  `reference_buffer()`, fault injection) and `SimpleCart` (the simplest
  cartridge: serves a fixed buffer linearly after a prelude of zeros,
  restarts on any write).
- `__init__.py` -- re-exports `PpuBus`, `SimpleCart`.

## Usage

```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path("doom/sim")))  # so `import ppubus` finds the package
sys.path.insert(0, str(Path("doom/tools")))  # so ppubus.py can `from fcpico import ...`

from ppubus import PpuBus, SimpleCart

bus = PpuBus()  # defaults: v1, reads_per_line=64, prerender_reads=61, osr_prelude_bytes=4
assert bus.frame_read_count() == 15490

buf = bus.reference_buffer(pix, mailbox)  # pix: (240, 256) uint8, values 0..3
cart = SimpleCart(buf, osr_prelude_bytes=bus.osr_prelude_bytes)
received = bus.run_frame(cart)  # pulls exactly frame_read_count() bytes, then delivers the heartbeat
pix_out, mailbox_out = bus.reconstruct(received)
```

Fault injection: `bus.run_frame(cart, drop=5)` / `duplicate=3` change the
byte count by exactly that amount (a simulated missed/double-counted
strobe); `hold_heartbeat=True` skips the `cart.ppu_write()` call (a stalled
heartbeat). `PpuBus.run_frames(cart, n, faults={k: {...}})` runs several
frames with per-frame fault kwargs.

## Relationship to `tools/fcpico/stream.py`

`fcpico.stream` is the real firmware buffer layout (byte-exact,
`convVram()`/`ppu_dma()`-derived) that `tools/ppu_decode.py` and
`tools/fcvideo_ref.py` produce and consume. `ppubus` is a *different*,
coarser model of *how many bus reads a frame takes and in what order* --
the two share bit-packing conventions (`fcpico.stream.pack_run`/
`unpack_run`) but not buffer byte offsets, because that mapping is exactly
what plan 01 calls unresolved. Don't feed a `fcpico.stream.encode_frame()`
buffer straight to `SimpleCart` expecting `PpuBus.reconstruct()` to recover
it byte-for-byte; use `PpuBus.reference_buffer()` instead.
