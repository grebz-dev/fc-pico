# port -- firmware entry point and board glue

Present now: `flash_layout.h` (the single source of truth for flash addresses; checked by
`tools/flash_layout_check.py`), `main_testpattern.c`, serial `cli.c`, PIO/DMA `trace.c`,
the device `CMakeLists.txt`, and the host-tested controller mapper in `input/`.
Device build acceptance remains outstanding; see [`../PROGRESS.md`](../PROGRESS.md).

The remaining layout below describes planned engine and audio integration.

`main.c` (clock/voltage, `stdio`, launching the engine), `flash_layout.h` (the single source
of truth for flash addresses), `cli.c` (serial debug CLI: `stats`, `pattern`, `trace`, `dump`),
`testpattern/` (the M0 firmware without the engine), `apu/` (the `fcapu` register sequencer
core, engine-independent), `resources/` (packed boot ROM image, music streams, SFX scripts via
`tools/respack.py`).

Specifications: [`../plan/02-architecture.md`](../plan/02-architecture.md), [`../plan/06-audio.md`](../plan/06-audio.md), [`../plan/08-build.md`](../plan/08-build.md).
