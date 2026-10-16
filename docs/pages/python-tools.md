@page python_tools Python inspection tools

`tools/fcpico/` is a small Python package for looking at what the cartridge sends:
stream geometry, picture decoding, colour indices and captures from the simulated
cartridge. It needs only the standard library; Pillow is optional, for PNG output and
one test.

```sh
python3 -m venv .venv
.venv/bin/pip install -e "tools[test]"
cmake -S platform -B build/platform && cmake --build build/platform   # native library
.venv/bin/python -m pytest tests platform/tests
```

| Module | Purpose |
|---|---|
| `fcpico.protocol` | Protocol constants, generated from `fcbus_protocol.h` (see @ref shared_bus_core) |
| `fcpico.layout` | Stream and mailbox geometry from the generated descriptor; rejects unknown versions |
| `fcpico.native` | `ctypes` bindings to `build/platform/libfcpico_native.so`: the core's stream encoder and layout, and the cartridge model |
| `fcpico.stream` | Portable reference: pack and unpack pictures, read the mailbox, apply attribute and palette rules |
| `fcpico.capture` | Drive a native cartridge the way the tutorial boot ROM does and keep what the console receives |
| `fcpico.ppu_decode` | Turn a stream plus palette and attribute table into NES colour indices or an image |
| `fcpico.nes_palette` | An approximate RGB table for viewing; not a measured or emulator palette |

## One implementation of the firmware behaviour

The Python code does not re-implement the firmware. Geometry comes from the
descriptor generated from the protocol header, and the native library (the same C
code the cartridge model runs) provides the encoder and the cartridge itself.
`fcpico.stream` is a readable reference that the tests compare byte for byte with
the native encoder. The library must report cartridge ABI 1 and a layout matching
the descriptor, or `fcpico.native.load()` refuses it. Set `FCPICO_NATIVE_LIB` to use
a library from another build directory.

## Examples

```python
from pathlib import Path
from fcpico import capture, native, ppu_decode

lib = native.load()
prg = Path("tutorial_project/BOOTROM/rom.NES").read_bytes()[16:]
with lib.cart("demo", None, prg) as cart:
    shot = capture.capture(cart, lib.layout, frames=20)
indices = ppu_decode.decode(shot.stream, shot.palette, shot.attributes, lib.layout)
ppu_decode.write_image(Path("demo.ppm"), indices)
```

```sh
python3 -m fcpico.ppu_decode stream.bin --palette pal.bin --attr attr.bin -o frame.png
```

## Checks against the emulator

`tests/tools/test_fcpico_capture.py` decodes the "pattern" provider's picture and
compares it with the MesenCE screenshot of the S0 scenario
(`tests/fixtures/mesen-s0-pattern0.png`, the reviewed golden). Every NES colour index
must map to exactly one screenshot colour and back, so the comparison does not
depend on the emulator's RGB palette. The capture console takes the same dummy read
before attribute uploads as the boot ROM.

Hardware bus traces are not covered here: the firmware that captures them is not
part of this repository.
