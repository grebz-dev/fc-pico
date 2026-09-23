# Composed-frame goldens

<!-- SPDX-License-Identifier: BSD-3-Clause -->

`doom_demo1_600.sha256` records every tenth raw 320x200 indexed frame from the first 600
frames of the checked-in `doom1.whx` DEMO1. `check.py` checks all 600 frame dimensions,
the 60 hashes and PNG previews at frames 0, 100, 200, 300, 400 and 500.
`test_title_menu.py` separately checks frame 100 of the title sequence, with and without
a menu opened at frame 60.

From the repository root:

```sh
cmake -S doom -B /tmp/fcpico-engine-host -G Ninja -DPICO_PLATFORM=host \
  -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build /tmp/fcpico-engine-host
ctest --test-dir /tmp/fcpico-engine-host --output-on-failure
/tmp/fcpico-engine-host/rp2040-doom/src/fcpico_doom_host \
  --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 600 --lockstep \
  --dump-8bit /tmp/fcpico-demo1-frames
python3 doom/tests/goldens/check.py /tmp/fcpico-demo1-frames
```

The PNGs use PLAYPAL palette 0 for inspection. The title, menu, status bar and melt-wipe
samples have been visually inspected across the DEMO1 and title/menu captures. These are engine-composition
goldens, not an independent pixel match against Chocolate Doom or the NES PPU. Such a
comparison, and conversion into NES stream bytes, remain separate gates.
