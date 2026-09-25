#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
output_dir=${1:-$here/out}
assembler=${NESASM_BIN:-nesasm}
if ! command -v "$assembler" >/dev/null 2>&1; then
    echo "NESASM_BIN must point to native NESASM CE (see bootrom/README.md)" >&2
    exit 2
fi
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
(
    cd "$here/src"
    "$assembler" -s -f -o "$output_dir/doom.raw.nes" PG_main.asm
)
python3 "$repo/doom/tools/nes/normalize_ines.py" \
    "$output_dir/doom.raw.nes" "$output_dir/doom.nes"
python3 "$repo/doom/tools/nes/check_fixbank.py" \
    "$output_dir/doom.nes" "$repo/tutorial_project/BOOTROM/bootrom_fixr.bin"
