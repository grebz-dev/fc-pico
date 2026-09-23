# D0 fixed-frame Doom co-simulation

<!-- SPDX-License-Identifier: BSD-3-Clause -->

D0 replays one real converted Doom stream through Mesen's NES PPU with the
existing tutorial v1 boot ROM. It checks the visible 256x240 frame against the
independent PPU decoder and compares all 16 palette and 64 attribute bytes
observed in console VRAM with the stream mailbox. It does **not** run the ARM
engine in Mesen or validate the future v2 NMI timing.

From the repository root, after building the SDL-free host engine and MesenCE:

```sh
mkdir -p /tmp/fcpico-doom-streams
/tmp/fcpico-engine-host/rp2040-doom/src/fcpico_doom_host \
  --whx doom/rp2040-doom/doom1.whx --demo 1 --frames 101 --lockstep \
  --dump-stream /tmp/fcpico-doom-streams
doom/.venv/bin/python doom/sim/mesen2/run_doom_frame.py \
  /tmp/fcpico-doom-streams/frame000100.bin --output /tmp/fcpico-d0
```

The screenshot is `/tmp/fcpico-d0/run/final.png`; the console VRAM captures
are `palette.bin` and `attributes.bin` beside it. Set `DOTNET_ROOT` to the
local .NET 10 installation if Mesen cannot find its runtime.

The corrected v1 bulk palette upload mirrors the first 16 bytes into the
second 16: on the NES, `$3F10/$14/$18/$1C` alias the backdrop entries. The
host Mesen adapter models the lead byte before a bulk attribute response that
the tutorial ROM's extra dummy read consumes. Strict S0 remains a separate
regression gate.
