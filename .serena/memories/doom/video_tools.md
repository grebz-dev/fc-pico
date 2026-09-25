# Video tool conventions

- `doom/tools/fcpico/` is a pure Python buffer/array implementation shared by tools and tests. `protocol.py` is generated from `doom/fcbus/fcbus_protocol.h`; import it instead of retyping protocol numbers.
- In `stream.py`, pixel values are 2-bit sub-palette indices (0..3), not NES palette indices or RGB unless an API names `nes` or `rgb`; arrays are row-major `[y][x]`. Its module docstring explains the picture/mailbox-tail overlap.
- `doom/tools/fcvideo_ref.py` is the reference for stages B-E; `doom/tools/ppu_decode.py` reconstructs a stream. Change goldens through `doom/tools/update_goldens.py`, recording why the image changed. `doom/tests/goldens/README.md` describes the current sample set and commands.
- The engine's raw 3D `frame_buffer` omits status/menu/wipe overlays; stage A must compose those into a full 320x200 indexed frame before the NES converter. See `doom/plan/04-video.md` for the rendering contract.