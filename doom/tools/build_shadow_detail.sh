#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Rebuild the 2026-09-25 hardware milestone, including its startup diagnostics.
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
bootstrap=0
case "${1:-}" in
    --bootstrap-tools) bootstrap=1 ;;
    --help|-h)
        cat <<'EOF'
Usage: doom/tools/build_shadow_detail.sh [--bootstrap-tools]

Builds firmware, embedded console ROM and merged WHX UF2 under .build/doom-shadow.
--bootstrap-tools fetches/builds pinned native picotool and NESASM if absent.
Requires the project-local Arm GCC 13.2 toolchain and Pico SDK 2.1.1.

Overrides: BUILD_DIR, TOOLS_DIR, ARM_TOOLCHAIN_PATH, PICO_SDK_PATH,
           PICOTOOL_DIR (CMake package directory), NESASM_BIN, JOBS.
See doom/plan/08-build.md for prerequisites and hardware validation history.
EOF
        exit 0 ;;
    '') ;;
    *) echo "Unknown argument: $1 (use --help)" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { echo 'Too many arguments' >&2; exit 2; }
fail() { echo "$*" >&2; exit 1; }
build=${BUILD_DIR:-$repo/.build/doom-shadow}
tools_dir=${TOOLS_DIR:-$repo/.build/tools}
arm=${ARM_TOOLCHAIN_PATH:-$repo/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi}
export PICO_SDK_PATH=${PICO_SDK_PATH:-$HOME/.local/fcpico/pico-sdk}
picotool_dir=${PICOTOOL_DIR:-$tools_dir/picotool-install/lib/cmake/picotool}
export NESASM_BIN=${NESASM_BIN:-$tools_dir/nesasm/nesasm}
jobs=${JOBS:-4}
for command in cmake ninja python3 git make; do
    command -v "$command" >/dev/null || fail "Missing command: $command"
done
[[ -x $arm/bin/arm-none-eabi-gcc ]] || fail "Missing Arm toolchain: $arm"
[[ $("$arm/bin/arm-none-eabi-gcc" -dumpfullversion) == 13.2.1 ]] || fail 'Requires Arm GCC 13.2.1'
[[ -f $PICO_SDK_PATH/pico_sdk_version.cmake ]] || fail "Missing Pico SDK: $PICO_SDK_PATH"
[[ $(git -C "$PICO_SDK_PATH" describe --tags --exact-match HEAD) == 2.1.1 ]] || fail 'Requires Pico SDK tag 2.1.1'
[[ -f $repo/doom/rp2040-doom/doom1.whx ]] || fail 'Initialize doom/rp2040-doom first'
export PATH="$arm/bin:$PATH"

if [[ ! -f $picotool_dir/picotoolConfig.cmake ]]; then
    [[ $bootstrap == 1 ]] || fail 'Set PICOTOOL_DIR to native picotool 2.1.1, or run --bootstrap-tools'
    mkdir -p "$tools_dir"
    if [[ ! -d $tools_dir/picotool-src ]]; then
        git clone --branch 2.1.1 --depth 1 https://github.com/raspberrypi/picotool.git "$tools_dir/picotool-src"
    fi
    [[ $(git -C "$tools_dir/picotool-src" rev-parse HEAD) == de8ae5ac334e1126993f72a5c67949712fd1e1a4 ]] || fail 'Unexpected picotool source revision'
    cmake -S "$tools_dir/picotool-src" -B "$tools_dir/picotool-build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DPICO_SDK_PATH="$PICO_SDK_PATH" -DCMAKE_INSTALL_PREFIX="$tools_dir/picotool-install"
    cmake --build "$tools_dir/picotool-build" -j "$jobs"
    cmake --install "$tools_dir/picotool-build"
    picotool_dir=$tools_dir/picotool-install/lib/cmake/picotool
fi
if ! command -v "$NESASM_BIN" >/dev/null; then
    [[ $bootstrap == 1 ]] || fail 'Set NESASM_BIN to native NESASM CE, or run --bootstrap-tools'
    mkdir -p "$tools_dir"
    if [[ ! -d $tools_dir/nesasm ]]; then
        git clone https://github.com/ClusterM/nesasm.git "$tools_dir/nesasm"
        git -C "$tools_dir/nesasm" checkout --detach 6fc41cda37b934aa29aa2639d0baa74424268e31
    fi
    [[ $(git -C "$tools_dir/nesasm" rev-parse HEAD) == 6fc41cda37b934aa29aa2639d0baa74424268e31 ]] || fail 'Unexpected NESASM source revision'
    make -C "$tools_dir/nesasm/source" -j "$jobs"
    export NESASM_BIN=$tools_dir/nesasm/nesasm
fi
# Refuse reuse of a cache made with the system GCC or another compiler.
if [[ -f $build/device/CMakeCache.txt ]]; then
    cached=$(sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p;s/^CMAKE_C_COMPILER:STRING=//p' "$build/device/CMakeCache.txt")
    [[ $(realpath "$cached") == $(realpath "$arm/bin/arm-none-eabi-gcc") ]] || fail 'Build cache uses another compiler; choose a fresh BUILD_DIR'
fi
"$repo/doom/bootrom/ci/tutorial_md5_gate.sh"
cmake -S "$repo/doom" -B "$build/device" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_BOARD=fcpico -DPICO_PLATFORM=rp2350-arm-s \
    -DPICO_TOOLCHAIN_PATH="$arm" -DPICO_NO_FLASH=OFF -DPICO_COPY_TO_RAM=OFF \
    -DPICO_SDK_PATH="$PICO_SDK_PATH" -Dpicotool_DIR="$picotool_dir" \
    -DFCPICO_HOST_ONLY=OFF -DFCPICO_BUILD_ENGINE=ON -DFCPICO_DIAGNOSTIC_NO_ENGINE=OFF \
    -DFCPICO_DIAGNOSTIC_ENGINE_DELAY=ON -DPICO_NO_PICOTOOL=OFF -DPICO_NO_UF2=OFF
cmake --build "$build/device" --target doom_tiny_fcpico -j "$jobs"
firmware=$build/device/rp2040-doom/src
python3 "$repo/doom/tools/flash_layout_check.py" "$firmware/fcpico_doom.elf"
output=$build/artifacts
mkdir -p "$output"
cp "$firmware/fcpico_doom.elf" "$firmware/fcpico_doom.uf2" "$output/"
cp "$firmware/fcpico/doom_bootrom/doom.nes" "$output/"
python3 "$repo/doom/tools/whx2uf2.py" "$repo/doom/rp2040-doom/doom1.whx" \
    "$output/fcpico_doom_shadow_detail_delay_whx.uf2" --firmware "$output/fcpico_doom.uf2"
{
    printf 'Profile: shadow detail, MinSizeRel, RP2350 ARM Secure, engine delay ON\n'
    printf 'Repository: '; git -C "$repo" rev-parse HEAD
    printf 'Engine: '; git -C "$repo/doom/rp2040-doom" rev-parse HEAD
    printf 'SDK: '; git -C "$PICO_SDK_PATH" rev-parse HEAD
    "$arm/bin/arm-none-eabi-gcc" --version
    printf 'picotool_DIR=%s\nNESASM_BIN=%s\n' "$picotool_dir" "$NESASM_BIN"
    git -C "$repo" status --short
} > "$output/build-info.txt"
cp "$build/device/CMakeCache.txt" "$output/"
git -C "$repo" diff --binary HEAD > "$output/source.patch"
(cd "$output" && sha256sum fcpico_doom.elf fcpico_doom.uf2 doom.nes \
    fcpico_doom_shadow_detail_delay_whx.uf2 > SHA256SUMS)
printf '\nFlash image: %s/fcpico_doom_shadow_detail_delay_whx.uf2\n' "$output"
cat "$output/SHA256SUMS"
