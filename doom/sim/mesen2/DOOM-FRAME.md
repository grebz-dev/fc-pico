# D0/D1 fixed-frame Doom co-simulation

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

For D1, assemble the Doom ROM first and pass `--rom doom/bootrom/out/doom.nes`.
The runner additionally checks all 128 received mailbox bytes and samples the
assembled `NMI_RTI` address in Mesen. The measured worst-case py65 NMI is
1614 cycles through the second `$2005` write and 2021 cycles total with 15 APU
pairs; Mesen observed its RTI on scanline 255 in 30 post-startup frames.
This is still a fixed stream, not a dynamic engine-to-console run.

The mapper now selects the stream by address (`$0800-$0FFF`) without dropping
fetches at particular PPU cycles. The tutorial's tile `$80` main nametable,
tile `$00` adjacent nametable, and final PPU address `$0801` naturally produce
the measured 66 pre-render / 64 visible-line reads. Doom must preserve those
conditions. Earlier mapper versions selected all `$0000-$0FFF` addresses and
forced this cadence with cycle filtering, which hid the Doom setup errors.
The runner also checks every completed post-startup heartbeat: count 15490
for v1 or 15554 for v2, with no additional DMA stops. It rejects the old
`0001` Doom ROM at count 128, matching the observed black-screen failure.

S1 starts the console from one ROM while the cartridge serves another, so the
fix bank erases and reprograms the mapper's emulated PRG flash:

```sh
doom/.venv/bin/python doom/sim/mesen2/run_doom_frame.py \
  /tmp/fcpico-doom-streams/frame000100.bin --rom tutorial_project/BOOTROM/rom.NES \
  --serve-rom doom/bootrom/out/doom.nes --frames 3000 --output /tmp/fcpico-s1
```

It passes only if the console's final PRG equals the served image and the D1
picture, table, mailbox and NMI checks pass afterwards. Swap the two ROMs to
check recovery to the tutorial ROM.

The corrected v1 bulk palette upload mirrors the first 16 bytes into the
second 16: on the NES, `$3F10/$14/$18/$1C` alias the backdrop entries. The
host Mesen adapter models the lead byte before a bulk attribute response that
the tutorial ROM's extra dummy read consumes. Strict S0 remains a separate
regression gate.
