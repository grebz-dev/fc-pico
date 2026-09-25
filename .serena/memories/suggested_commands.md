# Suggested commands

Run from repository root; prefer task-specific acceptance commands in `doom/plan/10-workplan.md`.

- Host build: `cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON`; `cmake --build build-host`; `ctest --test-dir build-host --output-on-failure`.
- Python/PIO: `python3 -m pytest doom/tests doom/sim/pioemu -q`; use `doom/.venv/bin/python` when the repository virtualenv is present.
- Generated protocol: `python3 doom/tools/gen_protocol.py --check`; Markdown links: `python3 doom/tools/check_md_links.py doom`.
- Device: `cmake -S doom -B build-rp2350 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_SDK_PATH=<sdk-2.1.1> -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s`; `cmake --build build-rp2350`; `python3 doom/tools/flash_layout_check.py build-rp2350/port/fcpico_doom.elf`.
- Boot ROM: set `NESASM_BIN` to the pinned native assembler; run `doom/bootrom/ci/tutorial_md5_gate.sh`, then `doom/bootrom/build.sh`, then `python3 -m pytest doom/tests/bootrom -q -s`.
- MesenCE: `doom/sim/mesen2/build.sh` and `doom/sim/mesen2/run_scenario.sh S0`; choose other scenarios from `doom/plan/09-testing-ci.md`. Host frame/golden commands are in `doom/tests/goldens/README.md`.
- Full Ubuntu setup: `doom/tools/setup_env.sh` provisions dependencies when appropriate.