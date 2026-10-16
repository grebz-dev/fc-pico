@page shared_bus_core Shared bus core

`platform/fcbus/` is the cartridge side of the console protocol from
`tuto1_hw/sys/rp_system.cpp`, as portable C with no PIO, DMA, serial or watchdog
code: the mailbox builder, the receive dispatcher, the frame-sync decision, bulk
data mode, and the version and ROM replies. It implements exactly the tutorial
protocol (see @ref protocol); its constants are in `platform/fcbus/fcbus_protocol.h`.

The tutorial firmware does not use it yet. A separate change will move
`rp_system`'s protocol logic onto it, keeping `rp_system`'s API and its PIO/DMA code,
so the same tested logic runs on the cartridge, in host tests, and in emulators.

## Pieces

| File | Contents |
|---|---|
| `fcbus_protocol.h` | Opcodes, mailbox layout, key bits, stream geometry |
| `fcbus_core.h/.c` | The protocol state machine; no I/O, no allocation, one caller-owned struct per link |
| `fcbus_host.h/.c` | A host backend that stands in for the PPU (`fcbus_host_ppu_read()`) and the 6502 (`fcbus_host_ppu_write()`) |

`fcbus_core.h` maps each `rp_system` function to its counterpart. The core tells
its backend what to do through a small queue of actions: serve a reply (version
stamp, ROM page, data-mode header or page), restart the picture stream after a
heartbeat (with the one- or two-byte nudge), stop the DMA, or report
`FP_COM_INI`, `FP_COM_RST` and `FP_COM_LOG`.

## Differences from rp_system

The wire behaviour is the tutorial's. These implementation differences are
deliberate:

- **Picture handover.** `fcbus_core_publish()` swaps the new picture in at the next
  heartbeat that restarts the stream, and the back buffer stays reserved until then.
  `convVram()` swaps its pointers immediately; the tutorial avoids drawing into the
  streamed buffer by converting only once per heartbeat (`frame_draw`).
- **Data mode request.** `fcbus_core_request_data_mode()` returns at once and keeps
  `PF_COM_DMOD` in every mailbox until `FP_COM_DRQ` arrives, instead of
  `startDataMode()`'s blocking 50 ms retry loop.
- **Data-mode pages.** A `FP_COM_DLD` page beyond the 32-byte palette or 64-byte
  attribute table is served as zeros, not as whatever memory follows the table. The
  boot ROM only requests page 0.
- **Out of scope.** The hardware watchdog, serial logging, key trigger/repeat and the
  sound job stay in the firmware. The core adds diagnostic counters
  (`fcbus_core_t::stats`) that do not change behaviour.

## Building and testing

The library builds on any C11 host compiler with CMake:

```sh
cmake -S platform -B build/platform
cmake --build build/platform
ctest --test-dir build/platform --output-on-failure
python3 -m pytest platform/tests
```

The C tests cover the mailbox, palette and attribute updates, frame sync, the
receive dispatcher and an end-to-end run through the host backend. The Python test
checks every shared constant against `rp_system.h` and `BOOTROM/SysPico.asm`.
