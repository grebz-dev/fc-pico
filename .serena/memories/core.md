# Core

- FC PICO uses an RP2350 cartridge to present CHR data on the NES PPU bus and exchange a frame-tail mailbox with the 6502 ROM.
- The repository has a shipped/reference project at root and an active Doom port under `doom/`. For port architecture, repository boundaries, and the current evidence/issue sources, read `mem:doom/core`.
- For protected paths, generated files, licensing, plan correction, and commit rules, read `mem:conventions`.
- For build topology and dependency pins, read `mem:tech_stack`; for routine commands read `mem:suggested_commands`; for acceptance and evidence discipline read `mem:task_completion`.
- Serena memory policy is in `mem:memory_maintenance`: store durable, non-obvious guidance, while specifications, status, measurements and human-facing docs remain in the repository.