#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
native_status=0
if [[ ${1:-} == --native-status ]]; then native_status=1; shift; fi
[[ $# == 0 ]] || { echo "usage: $0 [--native-status]" >&2; exit 2; }
probe_source="$repo/doom/bootrom/src/PG_bg_text_device_tmp.asm"
[[ ! -e $probe_source ]] || { echo "Temporary probe source already exists: $probe_source" >&2; exit 2; }
cleanup() { rm -f "$probe_source"; }
trap cleanup EXIT

if [[ $native_status == 1 ]]; then
    PYTHONPATH="$repo/doom/tools" python3 -c \
      'from pathlib import Path; from build_native_status_probe import native_status_source; import sys; Path(sys.argv[1]).write_text(native_status_source())' \
      "$probe_source"
    export BUILD_DIR=${BUILD_DIR:-/tmp/fcpico-native-status-device}
    expected_stamp=20DOOM-04-9012
    output_name=fcpico_doom_native_status_whx.uf2
else
    PYTHONPATH="$repo/doom/tools" python3 -c \
      'from pathlib import Path; from build_bg_text_probe import text_source; import sys; Path(sys.argv[1]).write_text(text_source())' \
      "$probe_source"
    export BUILD_DIR=${BUILD_DIR:-/tmp/fcpico-bg-text-device}
    expected_stamp=20DOOM-04-9001
    output_name=fcpico_doom_bg_text_probe_whx.uf2
fi
export FCPICO_BOOTROM_SOURCE=${probe_source##*/}
rm -f "$BUILD_DIR/device/rp2040-doom/src/fcpico/doom_bootrom/doom.nes"
"$repo/doom/tools/build_shadow_detail.sh"

rom="$BUILD_DIR/artifacts/doom.nes"
PYTHONPATH="$repo/doom/tools" python3 -c \
  'from pathlib import Path; import sys; rom=Path(sys.argv[1]).read_bytes(); fix=Path(sys.argv[2]).read_bytes(); assert rom[16+0x6ff0:16+0x6ffe] == sys.argv[3].encode(); assert rom[16+0x7000:16+0x8000] == fix' \
  "$rom" "$repo/tutorial_project/BOOTROM/bootrom_fixr.bin" "$expected_stamp"
candidate="$BUILD_DIR/artifacts/$output_name"
cp "$BUILD_DIR/artifacts/fcpico_doom_shadow_detail_delay_whx.uf2" "$candidate"
sha256sum "$candidate" "$rom"
echo "Native status device artifact: $candidate"
