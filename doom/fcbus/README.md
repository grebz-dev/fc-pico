# fcbus -- the cartridge bus library (planned)

Plain-C, pico-sdk implementation of the FC PICO cartridge bus, wire-compatible with the shipped
v1 protocol and extended to v2. Specification: `../plan/02-architecture.md`,
`../plan/03-protocol-v2.md`; origin: `tutorial_project/tuto1_hw/sys/{rp_system,rp_dma}.*` and
`sys/pio/fcppu.pio` (copied verbatim here at task P0-T5).

Planned files:

| File | Role |
|------|------|
| `fcbus_protocol.h` | the single source of truth for opcodes, mailbox layouts, counts, key bits (P0-T4) |
| `fcppu.pio` | four PIO programs, unchanged from the tutorial; header generated at build time |
| `fcbus_core.c/.h` | hardware-independent logic: mailbox builder, rx dispatcher and state machine, sync decision, stream addressing, shadows, data-mode responder |
| `fcbus_device.c` | PIO/DMA/IRQ glue (`init`, ISR, DMA re-arm, ROM/version serving) -- RAM-resident where the ISR needs it |
| `fcbus_host.c` | host backend: `fcbus_host_ppu_read()`, `fcbus_host_ppu_write()`, `fcbus_host_publish()`; used by tests, the PPU-bus model and the Mesen2 cart model |
| `CMakeLists.txt` | `fcbus` (device) and `fcbus_host` (host) targets |

Tests: `../tests/fcbus/`.
