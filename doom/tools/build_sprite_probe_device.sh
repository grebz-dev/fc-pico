#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

# Build a hardware candidate with the setup-only sprite probe in the erasable
# ROM bank. The authored production source and permanent fix bank stay intact.
repo=$(cd "$(dirname "$0")/../.." && pwd)
diagnostic=0
if [[ ${1:-} == --diagnostic ]]; then diagnostic=1; shift; fi
[[ $# == 0 ]] || { echo "usage: $0 [--diagnostic]" >&2; exit 2; }
probe_source="$repo/doom/bootrom/src/PG_sprite_probe_tmp.asm"
[[ ! -e $probe_source ]] || { echo "Temporary probe source already exists: $probe_source" >&2; exit 2; }
cleanup() { rm -f "$probe_source"; }
trap cleanup EXIT

PYTHONPATH="$repo/doom/tools" python3 -c \
  'from pathlib import Path; from build_sprite_probe import probe_source; import sys; Path(sys.argv[1]).write_text(probe_source(bool(int(sys.argv[2])))[0])' \
  "$probe_source" "$diagnostic"
export FCPICO_BOOTROM_SOURCE=${probe_source##*/}
if [[ $diagnostic == 1 ]]; then
    export BUILD_DIR=${BUILD_DIR:-/tmp/fcpico-sprite-diagnostic-device}
    expected_stamp=20DOOM-02-9002
    output_name=fcpico_doom_sprite_diagnostic_whx.uf2
else
    export BUILD_DIR=${BUILD_DIR:-/tmp/fcpico-sprite-probe-device}
    expected_stamp=20DOOM-02-9001
    output_name=fcpico_doom_sprite_probe_whx.uf2
fi
rm -f "$BUILD_DIR/device/rp2040-doom/src/fcpico/doom_bootrom/doom.nes"
"$repo/doom/tools/build_shadow_detail.sh"

expected="$BUILD_DIR/artifacts/doom.nes"
if [[ ! -f $expected ]]; then
    echo "Missing embedded probe ROM: $expected" >&2
    exit 1
fi
PYTHONPATH="$repo/doom/tools" python3 -c \
  'from pathlib import Path; import sys; rom=Path(sys.argv[1]).read_bytes(); fix=Path(sys.argv[2]).read_bytes(); assert rom[16+0x6ff0:16+0x6ffe] == sys.argv[3].encode(); assert rom[16+0x7000:16+0x8000] == fix' \
  "$expected" "$repo/tutorial_project/BOOTROM/bootrom_fixr.bin" "$expected_stamp"
probe_uf2="$BUILD_DIR/artifacts/$output_name"
cp "$BUILD_DIR/artifacts/fcpico_doom_shadow_detail_delay_whx.uf2" "$probe_uf2"
sha256sum "$probe_uf2"
echo "Sprite probe device artifact: $probe_uf2"
