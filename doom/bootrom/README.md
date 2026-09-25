# bootrom -- the Doom boot ROM for the console

`src/PG_main.asm` is the display-first Doom v2 ROM: it uploads the 64-byte
attribute table and 16-byte palette on every valid mailbox, sends an explicit
three-byte controller heartbeat, and replays up to 15 APU pairs. It parks all
sprites once, then uses BG+sprite rendering to preserve the measured PPU read
count. The permanent fix bank (`tutorial_project/BOOTROM/bootrom_fixr.bin`) is
included unchanged at `$F000`. This is not yet the full audio/input/data-mode
ROM described in `../plan/07-bootrom.md`.

Build `20DOOM-02-0002` restores the tutorial's stream selection: nametable 0
uses tile `$80` (pattern `$0800`), and nametable 1 uses tile `$00`. The NMI
restores the current PPU address to `$0801` after its three-byte heartbeat,
preserving the measured pre-render count. Both details are necessary for
the address-based Mesen model to reach the expected v2 count of 15554.

The pinned native [NESASM CE](https://github.com/ClusterM/nesasm/tree/6fc41cda37b934aa29aa2639d0baa74424268e31)
assembles the tutorial PRG byte-for-byte. It writes two NES 2.0 header bytes
that the vendor image leaves zero; `tools/nes/normalize_ines.py` checks the
whole expected header and restores those two bytes, after which the tutorial
ROM MD5 matches exactly. Build from the repository root:

```sh
NESASM_BIN=/path/to/pinned/nesasm doom/bootrom/ci/tutorial_md5_gate.sh
NESASM_BIN=/path/to/pinned/nesasm doom/bootrom/build.sh
python3 -m pytest doom/tests/bootrom -q -s
```

The build stamp in `version.inc` must start with `20`: the fix bank's
`CHK_ROMVER` treats any other reply as PICO NOT FOUND instead of reflashing.

`out/doom.nes` is a 32,784-byte mapper-0 iNES image; the device build embeds
it automatically. `out/doom.raw.nes.0.nl` records the `NMI_RTI` address used
by the Mesen timing check. The local `out/` directory is ignored; CI publishes
it as an artifact.

Never edit anything under `tutorial_project/`; copy from it.
