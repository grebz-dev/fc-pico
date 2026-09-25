# Doom port core

- The active shareware Doom port lives under `doom/`: `fcbus/` bus and protocol, `port/` device glue, `bootrom/` 6502 bank, `tools/`, `sim/`, `tests/`, and `rp2040-doom/` engine submodule. `doom/sim/mesen2/Mesen2/` is another submodule.
- `doom/README.md` indexes the human-facing plan. `doom/plan/00-overview.md`, `01-constraints.md`, and `02-architecture.md` establish the design; `10-workplan.md` and `doom/issues/README.md` define tasks and acceptance. Live status/evidence is in `doom/PROGRESS.md`; console requests and results are in `doom/HARDWARE-REQUESTS.md` and `doom/HARDWARE-LOG.md`. Do not copy their transient status into memory.
- For measured PPU selection, stream layout, and protocol source of truth, read `mem:doom/bus_protocol`.
- For ROM assembly, stamp/reflash constraints, and artifact layout, read `mem:doom/bootrom_build`.
- For pico-sdk host shim limitations, PIO emulator limits, and confidence of simulation versus hardware, read `mem:doom/host_simulation`.
- For Python stream conventions, stage A composition, and golden generation, read `mem:doom/video_tools`.
- Build pins and commands are in `mem:tech_stack` and `mem:suggested_commands`; code/branch constraints and acceptance discipline are in `mem:conventions` and `mem:task_completion`.