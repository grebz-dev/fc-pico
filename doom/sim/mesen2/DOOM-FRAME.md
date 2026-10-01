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

## U0 setup-time sprite probe

With the pinned NESASM CE and Mesen build present, run from the repository root:

```sh
DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python \
  doom/sim/mesen2/run_sprite_probe.py --output /tmp/fcpico-sprite-probe-u0
```

The runner builds an isolated ROM variant; production `PG_main.asm` and the
permanent fix bank remain unchanged. It uploads six known tiles at `$1800`
and one at `$1FF0`, sets seven OAM entries and one sprite palette color during
setup with test-only ROM stamp `20DOOM-02-9001`, and compares the probe against
the normal v2 ROM on the same synthetic background stream. It checks exact
OAM, authored CHR bytes, palette, the
independent sprite mask, unchanged background pixels, heartbeat count and NMI
completion. Outputs include both screenshots, ROM, stream and PPU dumps under
the chosen directory. Mesen has 8 KiB CHR RAM; this run does not establish the
physical board's pattern capacity or mirroring. Leave the normal ROM path in
place until the NES-001 pattern-storage check is complete.

For a merged NES-001 candidate, run `doom/tools/build_sprite_probe_device.sh`;
the exact tested artifact and recovery steps are in [HR-7](../../HARDWARE-REQUESTS.md#hr-7-sprite-pattern-storage-and-visible-probe-task-p3-u2).
Use `--limit` to check a ninth same-scanline sprite is present in OAM but
absent from Mesen's image. Use `--diagnostic` for the HR-8 duplicated
A/diamond arrangement.

## Isolated v3 UI mailbox probe

```sh
DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python \
  doom/sim/mesen2/run_ui_transport_probe.py --output /tmp/fcpico-ui-transport-u0
```

This isolated v3 ROM (`20DOOM-03-9001`) reads a 144-byte mailbox and stores
the final 16 UI bytes at `$0300`. The runner requires 30 stable count-15570
heartbeats, exact UI bytes, unchanged BG tables and NMI completion in vblank.
It does not draw a production HUD. A separate py65 test exercises the deferred
OAM DMA path with both DMA parities and 15 APU pairs.
Add `--hud --health 0`, `--hud --health 200` or `--hud --health 999` to
assemble a dynamic health-digit slice (`20DOOM-03-9005`) and check its exact
OAM entries after the deferred DMA. The resulting screenshots show native
`H000`, `H200` and `H999` on a reserved dark strip; the full status and face
art are not in this test ROM.

## Isolated v4 native background text probe

```sh
DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python \
  doom/sim/mesen2/run_bg_text_probe.py --output /tmp/fcpico-bg-text-u0
```

The v4 probe ROM (`20DOOM-04-9001`) holds 28 font tiles in nametable row 29,
columns 2 through 29. It renders `NATIVE BACKGROUND TEXT WORKS` beneath the
health sprite slice. Removing those 448 selected pattern reads from the
picture stream moves the mailbox to byte 14978 and makes the frame count
15122. The runner checks the exact nametable row and UI packet, OAM, BG
tables, 30 stable heartbeats, and NMI completion. The RP2350 converter
compacts this region only after a v4 console negotiation. This is a fixed
message probe; game menus and dynamic text still need a native text update
path and layout work.

`doom/tools/build_bg_text_probe_device.sh` packages the same ROM with the
current engine in a merged UF2 for an NES-001 check. Its permanent fix bank
is checked byte for byte before the artifact is accepted.

## Native sprite status and face probe

```sh
DOTNET_ROOT=/home/josh/.dotnet doom/.venv/bin/python \
  doom/sim/mesen2/run_native_status_probe.py --output /tmp/fcpico-native-status-u0
```

This v4 ROM (`20DOOM-04-9016`) draws a 4×4 Doomguy face, one selected ARMS
yellow plate, and up to three matching colored keycards. The host converts
the edited concrete panel and red value font to native background pixels;
the fixed 28-tile row is now scanlines 232–239, leaving the fourth inventory
row and the labels visible in the stream. The runner checks CHR, OAM, palette,
CELL label, selected plate, status packet, initial fixed row, 30 stable
count-15122 heartbeats and NMI completion. Use `--health 0 --armor 999
--face 41` to check the dead-face layout.

`doom/tools/build_bg_text_probe_device.sh --native-status` builds the merged
NES-001 candidate. Its embedded ROM must match the Mesen ROM byte for byte.
The [face sheet](../../assets/doomguy_faces.md) documents the source
expressions and the resident representatives. A py65 test
checks that two native text tile writes defer OAM DMA and keep both NMI paths
inside the vblank timing limits.

To check the paused-menu transition and its OAM restoration, run:

```sh
doom/.venv/bin/python doom/sim/mesen2/run_native_status_probe.py \
  --output /tmp/fcpico-menu-close --menu --close-menu-frame 165
```

The runner captures the enlarged blue/yellow paused logo, then checks every
face row's X positions and the returned yellow ARMS plate after dismissal.
`doom/tools/reproduce_native_hud.sh --verify-twice` runs this along with the
test suites and two independent merged UF2 builds.

Episode selection now draws its full names in one centered row each by
compositing the resident NES menu font into the v4 background stream. The
three lines consume no OAM slots; the cursor remains a sprite. Run
`run_native_status_probe.py --menu --menu-id 2` to capture the episode page,
or `--menu --menu-id 4` to check the mouse-free options menu. The options
labels follow the compiled Doom menu: End Game, Message, and Sound.
