# Licences of components in `doom/`

To be completed in task P0-T13. Known at planning time:

| Component | Origin | Licence | Notes |
|-----------|--------|---------|-------|
| Doom engine | `rp2040-doom` submodule (Chocolate Doom derivative) | GPLv2 for Chocolate-Doom-derived code; BSD-3-Clause for RP2040-Doom-specific code not implementing Chocolate Doom interfaces; MIT for modified emu8950; BSD-3 for ADPCM-XA | see the submodule's README |
| `fcppu.pio` | `tutorial_project/tuto1_hw/sys/pio/fcppu.pio` | BSD-3-Clause header (Raspberry Pi (Trading) Ltd.) with impact soft modifications; impact soft terms for the modifications are unclear (2025 (c) impact soft) | question H1 in `plan/11-risks.md` |
| `fcbus`, `fcvideo`, `fcapu`, `port`, `sim`, `tools` | new code in this repository, implemented from the protocol documentation | BSD-3-Clause (intended) | |
| Doom boot ROM (erasable bank) | derived from `tutorial_project/BOOTROM/` | impact soft terms | question H1 |
| Fix bank binary `bootrom_fixr.bin` | `tutorial_project/BOOTROM/` | impact soft; redistributed unmodified as shipped with the product | |
| `doom1.whx` | shareware `DOOM1.WAD` converted by `whd_gen` | id Software shareware terms | decision H2 |
| Mesen2 (co-simulation only) | pinned checkout under `sim/mesen2/` | GPL-3.0 | not part of the firmware |
| rp2040-pio-emulator | pip | Apache-2.0 | test tooling |
| FamiStudio | external tool | MIT | asset pipeline |
