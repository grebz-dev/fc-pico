#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Rebuild and validate the native v4 HUD, including the menu-close transition.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
output=${OUTPUT_DIR:-/tmp/fcpico-native-hud-repro}
repeat=0
if [[ ${1:-} == --verify-twice ]]; then repeat=1; shift; fi
[[ $# == 0 ]] || { echo "usage: $0 [--verify-twice]" >&2; exit 2; }
python="$repo/doom/.venv/bin/python"
[[ -x $python ]] || { echo "Missing $python" >&2; exit 2; }
mkdir -p "$output"

# The committed header must be reproducible from the edited reference, WHX,
# and the prototype's approved font templates.
before=$(sha256sum "$repo/doom/port/video/native_status_panel.h" | cut -d ' ' -f 1)
"$python" "$repo/doom/tools/build_live_hud_art.py" | tee "$output/art.log"
after=$(sha256sum "$repo/doom/port/video/native_status_panel.h" | cut -d ' ' -f 1)
[[ $before == "$after" ]] || { echo "Generated HUD header changed" >&2; exit 1; }
before=$(sha256sum "$repo/doom/port/video/native_menu_font.h" | cut -d ' ' -f 1)
"$python" "$repo/doom/tools/build_native_menu_font.py" | tee "$output/menu-font.log"
after=$(sha256sum "$repo/doom/port/video/native_menu_font.h" | cut -d ' ' -f 1)
[[ $before == "$after" ]] || { echo "Generated menu font changed" >&2; exit 1; }

"$python" -m pytest -q \
    "$repo/doom/tests/tools/test_stream.py" \
    "$repo/doom/tests/tools/test_doom_menu_art.py" \
    "$repo/doom/tests/bootrom/test_native_status_probe.py" \
    "$repo/doom/tests/fcvideo/test_converter.py" | tee "$output/python-tests.log"
cmake -S "$repo/doom" -B "$output/host-tests" -DFCPICO_HOST_ONLY=ON \
    -DCMAKE_BUILD_TYPE=Release > "$output/host-configure.log"
cmake --build "$output/host-tests" --target test_fcvideo test_fcui -j4 \
    > "$output/host-build.log"
ctest --test-dir "$output/host-tests" -R 'test_fcvideo|test_fcui' \
    --output-on-failure | tee "$output/host-tests.log"

"$python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$output/mesen-status" | tee "$output/mesen-status.log"
"$python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$output/mesen-menu-close" --menu --close-menu-frame 165 \
    | tee "$output/mesen-menu-close.log"
"$python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$output/mesen-title" --menu --no-status \
    | tee "$output/mesen-title.log"
"$python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$output/mesen-episode" --menu --menu-id 2 \
    | tee "$output/mesen-episode.log"
"$python" "$repo/doom/sim/mesen2/run_native_status_probe.py" \
    --output "$output/mesen-options" --menu --menu-id 4 \
    | tee "$output/mesen-options.log"

BUILD_DIR="$output/device-first" \
    "$repo/doom/tools/build_bg_text_probe_device.sh" --native-status \
    > "$output/device-first.log"
rom_first="$output/device-first/artifacts/doom.nes"
uf2_first="$output/device-first/artifacts/fcpico_doom_native_status_whx.uf2"
if [[ $repeat == 1 ]]; then
    BUILD_DIR="$output/device-second" \
        "$repo/doom/tools/build_bg_text_probe_device.sh" --native-status \
        > "$output/device-second.log"
    rom_second="$output/device-second/artifacts/doom.nes"
    uf2_second="$output/device-second/artifacts/fcpico_doom_native_status_whx.uf2"
    cmp "$rom_first" "$rom_second"
    cmp "$uf2_first" "$uf2_second"
    printf 'ROM and merged UF2 are byte-identical across two independent build trees.\n' \
        | tee "$output/rebuild-check.txt"
fi

"$python" - "$repo" "$output" "$repeat" <<'PY'
import csv
import hashlib
import json
import subprocess
import sys
from pathlib import Path

repo, output, repeat = Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3])

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def revision(path):
    return subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"],
                                   text=True).strip()

first = output / "device-first/artifacts"
trace = output / "mesen-menu-close/mesen/run/trace.csv"
with trace.open() as source:
    rows = list(csv.DictReader(source))
steady = [row for row in rows if row["event"] == "frame" and
          int(row["ppu_frame"]) >= 150]
manifest = {
    "source_commit": revision(repo),
    "engine_commit": revision(repo / "doom/rp2040-doom"),
    "rom_stamp": (first / "doom.nes").read_bytes()[16 + 0x6ff0:16 + 0x6ffe].decode(),
    "byte_identical_rebuild": bool(repeat),
    "sha256": {
        "edited_reference": sha(repo / "doom/assets/hud_edit_reference/full-frame-reference-edited.png"),
        "generated_header": sha(repo / "doom/port/video/native_status_panel.h"),
        "logo_chr": sha(repo / "doom/assets/doom_menu_logo.chr"),
        "whx": sha(repo / "doom/rp2040-doom/doom1.whx"),
        "rom": sha(first / "doom.nes"),
        "merged_uf2": sha(first / "fcpico_doom_native_status_whx.uf2"),
    },
    "mesen_menu_close": {
        "trace_rows": len(rows),
        "steady_heartbeats": int(steady[-1]["heartbeats"]) - int(steady[0]["heartbeats"]),
        "last_count": int(steady[-1]["last_count"]),
        "last_dma_stops": int(steady[-1]["dma_stops"]),
        "open_capture": "mesen-menu-close/mesen/run/menu-open.png",
        "closed_capture": "mesen-menu-close/mesen/run/final.png",
        "bus_trace": "mesen-menu-close/mesen/run/trace.csv",
    },
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))
PY
echo "Reproducible HUD build: $output/manifest.json"
