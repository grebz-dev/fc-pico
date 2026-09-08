# tools -- host-side tooling (planned, Python 3.11+)

| Tool | Task | Purpose |
|------|------|---------|
| `gen_protocol.py` | P0-T4 | `fcbus/fcbus_protocol.h` -> `bootrom/gen/protocol.inc`, `tools/fcpico/protocol.py`; `--check` |
| `flash_layout_check.py` | P0-T1 | ELF flash end vs `port/flash_layout.h` |
| `ppu_decode.py` | P0-T7 | stream + mailbox -> PNG (pure-Python PPU reconstruction) |
| `fcvideo_ref.py` | P1-T2 | reference implementation of decimation, block cost, dither, pack |
| `trace_decode.py` | P0-T9 | PIO sampler dump -> per-line strobe counts, `/RD` width histogram |
| `update_goldens.py`, `tests/goldens/check.py` | P1-T2 | golden hashes and PSNR thresholds |
| `whx2uf2.py` | P1-T6 | wrap `doom1.whx` as a UF2 at `0x10080000` |
| `nes/bincut.py`, `nes/bin2c.py`, `respack.py`, `nes/check_fixbank.py`, `nes/set_mapper.py` | P2-T1, P0-T11 | replacements for the Windows-only `bin_catcut`/`Bin2C`/`binlink`; co-sim header patch |
| `audio/wadextract.py`, `audio/vgm2apus.py`, `audio/mus2apus.py`, `audio/sfx2dpcm.py`, `audio/sfx2apu.py`, `audio/dpcm_pack.py` | P4-T3 | the audio pipeline (`../plan/06-audio.md`) |
| `palette_eval.py`, `nes_palette.py` | P2-T6 | preset evaluation; NES RGB table |
| `check_md_links.py` | P0-T12 | documentation link check used by `doom-docs.yml` |
| `setup_env.sh`, `requirements.txt` | P0-T1 | Ubuntu 24.04 environment |
