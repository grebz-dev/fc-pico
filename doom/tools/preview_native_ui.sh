#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
scenario=${1:-pause}
gui=${2:-}
case "$scenario" in
    pause) args=(--menu) ;;
    title) args=(--menu --no-status) ;;
    episode) args=(--menu --menu-id 2) ;;
    difficulty) args=(--menu --menu-id 3) ;;
    options) args=(--menu --menu-id 4) ;;
    status) args=() ;;
    stale) args=(--stale-menu-id) ;;
    *) echo "usage: $0 {pause|title|episode|difficulty|options|status|stale} [--gui]" >&2; exit 2 ;;
esac
[[ -z $gui || $gui == --gui ]] || { echo "expected --gui" >&2; exit 2; }
if [[ $scenario == difficulty ]]; then
    if [[ -z ${FCPICO_UI_HOST_FRAME:-} ]]; then
        bash "$repo/doom/tools/capture_skill_menu_frame.sh"
    fi
    host_frame=${FCPICO_UI_HOST_FRAME:-/tmp/fcpico-native-ui-skill-frame.bin}
    args+=(--host-frame "$host_frame")
fi
out="/tmp/fcpico-native-ui-$scenario"
"$repo/doom/.venv/bin/python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$out" "${args[@]}"
echo "Screenshot: $out/mesen/run/final.png"
echo "NES ROM: $out/mesen/console-cosim.nes"
if [[ $gui == --gui ]]; then
    export FCPICO_STREAM_FILE="$out/frame.bin"
    export FCPICO_CS1_MASK=0xf800
    export SDL_AUDIODRIVER=dummy
    if [[ -z ${DOTNET_ROOT:-} && -d $HOME/.dotnet/host/fxr ]]; then
        export DOTNET_ROOT="$HOME/.dotnet"
    fi
    cd "$out/mesen/run"
    exec "$out/mesen/emulator/Mesen" "$out/mesen/console-cosim.nes"
fi
