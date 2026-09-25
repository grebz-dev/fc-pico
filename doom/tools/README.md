# tools -- host-side tooling (planned, Python 3.11+)

| Tool | Task | Purpose |
|------|------|---------|
| `gen_protocol.py` | [P0-T4](../plan/10-workplan.md#p0-t4-protocol-single-source-of-truth) | `fcbus/fcbus_protocol.h` -> `bootrom/gen/protocol.inc`, `tools/fcpico/protocol.py`; `--check` |
| `flash_layout_check.py` | [P0-T1](../plan/10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware) | ELF flash end vs `port/flash_layout.h` |
| `check_device_ram.py` | [P1-T3](../plan/10-workplan.md#p1-t3-device-integration-cores-irqs-semaphores) | ELF post-zone RP2350 heap-margin gate |
| `whx2uf2.py` | [P1-T6](../plan/10-workplan.md#p1-t6-flash-layout-whx-placement-loading-screen) | place `doom1.whx` at `0x10080000` in a UF2, optionally merged with the firmware UF2 |
| `nes/normalize_ines.py` | [P2-T1](../plan/10-workplan.md#p2-t1-doom-boot-rom-source-tree-and-linux-build) | validate native NESASM CE header and normalize its two NES 2.0 bytes to vendor iNES |
| `ppu_decode.py` | [P0-T7](../plan/10-workplan.md#p0-t7-ppu-bus-model-and-decoder) | stream + mailbox -> PNG (pure-Python PPU reconstruction) |
| `fcvideo_ref.py` | [P1-T2](../plan/10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox) | reference implementation of decimation, block cost, dither, pack |
| `trace_decode.py` | [P0-T9](../plan/10-workplan.md#p0-t9-hardware-trace-capture-hw) | PIO sampler dump -> per-line strobe counts, `/RD` width histogram |
| `update_goldens.py`, `tests/goldens/check.py` | [P1-T2](../plan/10-workplan.md#p1-t2-stages-b-and-d-grey-lut-letterbox) | golden hashes and PSNR thresholds |
| `whx2uf2.py` | [P1-T6](../plan/10-workplan.md#p1-t6-flash-layout-whx-placement-loading-screen) | wrap `doom1.whx` as a UF2 at `0x10080000` |
| `nes/bincut.py`, `nes/bin2c.py`, `respack.py`, `nes/check_fixbank.py`, `nes/set_mapper.py` | [P2-T1](../plan/10-workplan.md#p2-t1-doom-boot-rom-source-tree-and-linux-build), [P0-T11](../plan/10-workplan.md#p0-t11-mesen2-co-simulation-skeleton) | replacements for the Windows-only `bin_catcut`/`Bin2C`/`binlink`; co-sim header patch |
| `audio/wadextract.py`, `audio/vgm2apus.py`, `audio/mus2apus.py`, `audio/sfx2dpcm.py`, `audio/sfx2apu.py`, `audio/dpcm_pack.py` | [P4-T3](../plan/10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap) | the audio pipeline ([`../plan/06-audio.md`](../plan/06-audio.md)) |
| `palette_eval.py`, `nes_palette.py` | [P2-T6](../plan/10-workplan.md#p2-t6-palette-preset-evaluation) | preset evaluation; NES RGB table |
| `check_md_links.py` | [P0-T12](../plan/10-workplan.md#p0-t12-activate-ci) | documentation link check used by `doom-docs.yml` |
| `setup_env.sh`, `requirements.txt` | [P0-T1](../plan/10-workplan.md#p0-t1-superbuild-skeleton-and-led-blink-firmware) | Ubuntu 24.04 environment |
