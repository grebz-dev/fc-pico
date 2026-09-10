# port -- firmware entry point and board glue

Present now: `flash_layout.h` (the single source of truth for flash addresses; checked by
`tools/flash_layout_check.py`). Everything else below is planned.

`main.c` (clock/voltage, `stdio`, launching the engine), `flash_layout.h` (the single source
of truth for flash addresses), `cli.c` (serial debug CLI: `stats`, `pattern`, `trace`, `dump`),
`testpattern/` (the M0 firmware without the engine), `apu/` (the `fcapu` register sequencer
core, engine-independent), `resources/` (packed boot ROM image, music streams, SFX scripts via
`tools/respack.py`).

Specifications: `../plan/02-architecture.md`, `../plan/06-audio.md`, `../plan/08-build.md`.
