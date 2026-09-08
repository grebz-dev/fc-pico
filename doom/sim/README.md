# sim -- simulation harnesses (planned)

| Directory | Level | Contents |
|-----------|-------|----------|
| `ppubus/` | L3 | C model of the console side of the bus (qualifying reads, NMI-time `$2007` traffic, image reconstruction, fault injection) + Python wrapper |
| `pioemu/` | L4 | pytest tests of `fcppu.pio` programs with `rp2040-pio-emulator` |
| `fullchip/rp2040js/` | L5 | optional: Node harness driving an RP2040 build of the test-pattern firmware |
| `mesen2/` | L6 | pinned Mesen2 checkout, the FC PICO mapper, `build.sh`, `run_scenario.sh`, `lua/S*.lua`, `results/` |
| `cartmodel/` | L6 | C API wrapping the host backend + test pattern or engine, linked into the Mesen2 mapper |
| `host_shim/` | L2 | pthread `multicore`/`sem` shims so the engine's host build needs no SDL |

See `../plan/09-testing-ci.md`.
