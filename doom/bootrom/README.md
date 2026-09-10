# bootrom -- the Doom boot ROM for the console

Present now: `version.inc` (the build stamp) and `gen/protocol.inc` (generated from
`fcbus/fcbus_protocol.h`). The assembly sources are task P2-T1.

A new erasable bank (`$8000`-`$EFFF`) assembled with `nesasm 2.51+autozp` (run under Wine on
Linux), derived from `tutorial_project/BOOTROM/` at task P2-T1. The permanent fix bank
(`tutorial_project/BOOTROM/bootrom_fixr.bin`) is included unchanged at `$F000`.

Specification: `../plan/07-bootrom.md`. Generated includes land in `gen/` (`protocol.inc` from
`tools/gen_protocol.py`, `dpcm_table.inc` + `dpcm_bank.bin` from `tools/audio/dpcm_pack.py`,
`version.inc`). Output: `out/doom.nes` (32,784 bytes, iNES mapper 0) and `out/doom_bootrom.h`.

Never edit anything under `tutorial_project/`; copy from it.
