# tests (planned)

`ctest` C tests linking the `*_host` libraries (`fcbus/`, `fcvideo/`, `apu/`, `input/`,
`protocol/`), `pytest` tests for tools and the boot ROM (`tools/`, `bootrom/` with `py65`),
and `goldens/` (hashes, PSNR thresholds, reference PNGs, `check.py`). Fixtures from hardware
sessions go under `fixtures/`. Levels L1-L3 of `../plan/09-testing-ci.md`.
