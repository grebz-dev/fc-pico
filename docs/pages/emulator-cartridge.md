@page emulator_cartridge Emulator cartridge model

`sim/cartmodel/` lets an NES emulator run a console ROM against a simulated FC PICO
cartridge on the same PC. It answers each CS1-qualified PPU read and write the way the
cartridge firmware would, using the @ref shared_bus_core for the protocol.

It models the protocol, not the hardware: there is no RP2350, PIO timing or bus
electrical behaviour, so a passing emulator run does not prove the timing of a real
cartridge.

## Interface

`fcpico_cart.h` is a small C ABI, versioned by `FCPICO_CART_ABI_VERSION` (currently 1).
An emulator mapper:

1. creates one cartridge per loaded game with `fcpico_cart_create()`, passing the ABI
   version, an application provider, its options and the 32 KiB boot ROM PRG the
   firmware should serve (the image `FP_COM_VER`/`FP_COM_ROM` deliver);
2. calls `fcpico_cart_ppu_read()` for every CS1-qualified pattern fetch and
   `fcpico_cart_ppu_write()` for every console write to `$2007`;
3. calls `fcpico_cart_reset()` on power cycle and `fcpico_cart_destroy()` on unload.

Reads return `0xFF` while the cartridge would not drive the bus (after a stopped
heartbeat). Each cartridge owns its state, so several can coexist. `fcpico_cart_create()`
reports why it rejected a configuration. Save states are not supported in ABI 1.

## Application providers

A provider stands in for the firmware's application code (`ap.main()` in the
tutorial). It gets `console_init()` when the console sends `FP_COM_INI` and `frame()`
once per heartbeat with the controller byte. It draws into `fcbus_core_stream_back()`,
calls `fcbus_core_publish()`, and uses the palette, attribute, command and APU calls of
the bus core. After each frame the cartridge sends palette/attribute changes and queued
APU writes, as `rp_system::update()` and the sound job do.

| Provider | Options | What it shows |
|---|---|---|
| `pattern` | `0`..`3` | Static and animated test patterns |
| `demo` | none | A square moved with the D-pad; A changes its colour, B plays a tone |

The tutorial boot ROM ships with its `PF_COM_VRAM`, `PF_COM_SE` and `PF_COM_BGM`
handlers disabled (`.if 0` in `BOOTROM/SysPico.asm`), so on that ROM single-byte VRAM
pokes, including the palette/attribute differences `fcbus_core_update_tables()` sends,
have no effect. Like the tutorial, change the palette or attribute table with
`fcbus_core_set_palette()`/`fcbus_core_set_attr_table()` followed by
`fcbus_core_request_data_mode()`; the console blanks the display briefly while it
uploads them.

To add one, fill in an `fcpico_provider_t` and pass it in `fcpico_cart_config_t`.

## MesenCE

`sim/mesen/` adds the cartridge to [MesenCE](https://github.com/SourMesen/Mesen2)
without a fork. `manifest.json` pins the upstream commit and the SHA-256 of its
source archive; `patches/` holds the FC PICO changes as an ordered series:

| Patch | Change |
|---|---|
| 0001 | `Core/NES/Mappers/Homebrew/FcPico.h`: the mapper, which forwards selected PPU reads and writes to the cartridge model and models the PRG flash used for reflashing |
| 0002 | Registers it as NES 2.0 mapper 4093 when built with `FCPICO` |
| 0003 | `makefile`: optional `FCPICO_INCLUDE`/`FCPICO_LIBS`, and rebuilding the mapper factory when the mapper, the model's headers or those settings change |

```sh
python3 sim/mesen/mesen.py check-patches   # the series applies with no fuzz or offsets
python3 sim/mesen/mesen.py build           # build/mesen/fcpico/src/bin/.../Mesen
python3 sim/mesen/mesen.py build --stock   # the same source without the patches
python3 sim/mesen/check_mapper.py          # headless check of the built emulator
```

`mesen.py` downloads the archive into `.cache/mesen/` (or takes `--archive` for
offline builds), verifies the checksum, extracts it under `build/mesen/` and applies
the patches. To change the integration, edit a git checkout of the pinned commit with
the series applied as commits, then run `mesen.py regenerate CHECKOUT` to rewrite
`patches/` and the manifest.

The emulator needs an emulator-only copy of the boot ROM with the private mapper
number: `python3 tools/nes/set_mapper.py rom.NES rom-fcpico.nes` keeps every PRG byte.
Never flash that copy. 4093 is not an assigned mapper number. The mapper reads its
configuration from the environment:

| Variable | Meaning |
|---|---|
| `FCPICO_PROVIDER`, `FCPICO_PROVIDER_OPTIONS` | Application provider (default `pattern`) |
| `FCPICO_SERVE_ROM` | iNES file whose PRG the firmware serves when the console reflashes (default: the loaded ROM) |
| `FCPICO_CS1_MASK` | `0xf800` selects `$0800-$0FFF`, as measured on an NES-001; `0xf000`/`0xe000` are wider diagnostic masks |
| `FCPICO_TRACE` | CSV of cartridge writes, flash operations and frames |

`check_mapper.py` runs the tutorial boot ROM three times. With the calibrated mask, a
plain run and a run that reads every pattern-table and nametable address through the
debugger each frame must both settle at the 15490 read count with identical traces, so
real fetches reach the cartridge once and debugger reads never do. The wider mask must
change the count.

## Scenarios and interactive play

`sim/mesen/scenario.py` runs complete scenarios headless in an isolated copy of the
emulator: NTSC, game database off, all-zero RAM, fixed CPU/PPU alignment, no rewind
or autosave, hardware sprite limits on, and Lua file access only for the test runner.
Every scenario runs twice and requires identical traces, mailboxes and screenshots.

```sh
python3 sim/mesen/scenario.py run S0       # strict: test pattern on the tutorial boot ROM
python3 sim/mesen/scenario.py run S1       # the console reflashes itself from the cartridge
python3 sim/mesen/scenario.py run demo     # the live demo provider with scripted input
python3 sim/mesen/scenario.py run S0 --diagnostic --cs1-mask 0xf000
python3 sim/mesen/scenario.py play --provider demo
```

| Scenario | Passes when |
|---|---|
| S0 | After startup every heartbeat reports 15490 reads, the mailbox is valid on every frame, no DMA stops; results are identical with and without debugger reads; the screenshot matches the reviewed golden in `sim/mesen/goldens/` |
| S1 | Booting the tutorial ROM while the cartridge serves a rebuilt ROM with a new stamp (`tools/build/build_rom.sh --stamp`), the console reflashes to exactly the served PRG with the permanent bank unchanged and then streams; the reverse direction restores the tutorial ROM |
| demo | Scripted input moves the square and the A button's palette change reaches the console's palette RAM; after the data-mode upload the link streams again |

`--diagnostic` reports the same measurements without failing on them, for example
with a wider CS1 mask. Results go to `build/mesen/scenarios/<name>/`: screenshots
(PNG and raw ARGB), palette RAM, the console PRG for S1, cartridge and mailbox
traces, console logs, and `report.json` with the verdict and the hashes of the
source tree, emulator, patch series, ROMs and settings.

`play` launches the GUI with the same isolated settings and MesenCE's default key
bindings (arrow keys, plus an Xbox pad) for the chosen provider. Scripted and
interactive runs both keep the hardware sprite limits. The emulator models the
protocol only; a passing scenario does not prove hardware timing.

## Tests

`sim/cartmodel/tests/test_cartmodel.c` drives a cartridge the way the tutorial boot
ROM does (`FP_COM_INI`, the data-mode exchange, heartbeats with controller input) and
checks the picture, the uploaded tables, mailbox pokes, APU writes, resets and
independent instances. It builds and runs with the other platform tests:

```sh
cmake -S platform -B build/platform
cmake --build build/platform
ctest --test-dir build/platform --output-on-failure
```
