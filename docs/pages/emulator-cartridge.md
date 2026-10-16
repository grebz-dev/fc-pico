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

To add one, fill in an `fcpico_provider_t` and pass it in `fcpico_cart_config_t`.

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
