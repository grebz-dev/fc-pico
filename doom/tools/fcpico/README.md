# fcpico -- Python package for the FC PICO wire format and video pipeline

Pure-Python (numpy-only) building blocks shared by the tools under
`doom/tools/` and by the test suites under `doom/tests/`. No pico-sdk, no
C++, no hardware access -- everything here operates on plain byte buffers
and numpy arrays.

## Contents

- `protocol.py` -- **generated** (`tools/gen_protocol.py`) from
  `fcbus/fcbus_protocol.h`, the single source of truth for the wire
  protocol's constants. Do not edit; import it (`from fcpico import
  protocol`) instead of retyping any of its numbers.
- `stream.py` -- the frame-stream buffer format: `word_index()`,
  `pack_run()`/`unpack_run()` (the 2bpp bitplane packing), `encode_frame()`/
  `decode_frame()` (a full 256x240 picture <-> the raw buffer bytes,
  byte-for-byte matching `rp_system::convVram()`/`ppu_dma()`), and the
  64-byte attribute table helpers (`attr_index()`, `attr_shift()`,
  `attr_get()`, `attr_set()`, matching `rp_system::setAtr()`). See its
  module docstring for the mailbox-tail/picture-data overlap this format
  has, and why that is harmless in practice.
- `nes_palette.py` -- the NES 2C02's 64-entry NTSC RGB table
  (`NES_PALETTE_RGB`) and `nearest_nes_color()`.

## Used by

- `tools/ppu_decode.py` -- stream buffer -> PNG.
- `tools/fcvideo_ref.py` -- the reference implementation of the video
  pipeline's decimation/letterbox/dither/palette-choice stages (plan 04).
- `doom/sim/ppubus/` -- shares `stream.pack_run`/`unpack_run`'s bit
  convention, though not `stream`'s buffer byte offsets (see that
  package's README).
- `doom/tests/tools/` and `doom/tests/protocol/`.

## Conventions

- Pixel/colour values are always the 2-bit index (0..3) into whichever
  4-entry sub-palette is in effect, never a raw NES palette index or RGB
  triple, unless a function's name says `rgb` or `nes` (e.g.
  `nearest_nes_color`, `ppu_decode.decode_to_rgb`).
- Arrays are `[y][x]` (row-major, numpy's default), matching how a
  console frame is naturally indexed.
- Every constant that exists in `fcbus/fcbus_protocol.h` is imported from
  `protocol.py`, never re-typed as a literal -- if a new magic number shows
  up while extending this package, check `protocol.py` for it first.
