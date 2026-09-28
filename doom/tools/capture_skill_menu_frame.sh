#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
build=${FCPICO_UI_HOST_BUILD:-/tmp/fcpico-native-ui-host-engine}
sdk_path=${PICO_SDK_PATH:-$HOME/.local/fcpico/pico-sdk}
engine="$build/rp2040-doom/src/fcpico_doom_host"
if [[ ! -x $engine ]]; then
    cmake -S "$repo/doom" -B "$build" -DPICO_PLATFORM=host \
        -DFCPICO_BUILD_ENGINE=ON -DPICO_SDK_PATH="$sdk_path" -G Ninja
fi
cmake --build "$build" --target doom_tiny_fcpico -j 8

pads=/tmp/fcpico-native-ui-skill.pads
frames=/tmp/fcpico-native-ui-skill-frames
"$repo/doom/.venv/bin/python" - "$pads" <<'PY'
from pathlib import Path
import sys

pads = [0] * 210
for start, end in ((65, 80), (90, 105)):
    for index in range(start, end):
        pads[index] = 128  # A: select New Game, then Episode 1
Path(sys.argv[1]).write_text("\n".join(map(str, pads)) + "\n")
PY
"$engine" --whx "$repo/doom/rp2040-doom/doom1.whx" --warp 1 1 \
    --frames 210 --lockstep --menu-at 60 --pads "$pads" \
    --dump-stream "$frames" >/tmp/fcpico-native-ui-skill-host.log 2>&1
cp "$frames/frame000200.bin" /tmp/fcpico-native-ui-skill-frame.bin
echo "Host difficulty frame: /tmp/fcpico-native-ui-skill-frame.bin"
