#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Check the FC PICO mapper in a built MesenCE, headless.

    python3 sim/mesen/check_mapper.py [--mesen PATH] [--output DIR]

Runs the tutorial boot ROM against the cartridge model three times:

1. with the calibrated CS1 mask, no debugger reads;
2. the same, but reading every pattern-table and nametable address through the
   debugger every frame;
3. with the wider diagnostic mask 0xf000.

Runs 1 and 2 must reach the steady read count of 15490 with no DMA stops after
startup and identical cartridge traces: real fetches reach the cartridge exactly
once and debugger reads not at all. Run 3 must not report 15490, showing the mask
decides which fetches reach the cartridge.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from nes.set_mapper import set_mapper  # noqa: E402

DEFAULT_MESEN = ROOT / "build/mesen/fcpico/src/bin/linux-x64/Release/linux-x64/publish/Mesen"
EXPECTED_COUNT = 15490
STARTUP_HEARTBEATS = 60

# Deterministic, isolated emulator settings; hardware sprite limits stay on.
SETTINGS = {
    "Nes": {"Region": "Ntsc", "DisableGameDatabase": True, "RamPowerOnState": "AllZeros",
            "RandomizeCpuPpuAlignment": False, "RandomizeMapperPowerOnState": False,
            "RemoveSpriteLimit": False},
    "Preferences": {"AutomaticallyCheckForUpdates": False, "EnableRewind": False,
                    "EnableAutoSaveState": False, "DisableOsd": True},
    "Debug": {"ScriptWindow": {"AllowIoOsAccess": True}},
}


def stage(mesen: Path, output: Path) -> Path:
    """Copy the emulator next to its own settings so no desktop install is touched."""
    staging = output / "emulator"
    shutil.copytree(mesen.parent, staging, dirs_exist_ok=True)
    (staging / "settings.json").write_text(json.dumps(SETTINGS))
    return staging / mesen.name


def run(binary: Path, rom: Path, directory: Path, frames: int, mask: str, peeks: bool) -> list[dict]:
    directory.mkdir(parents=True, exist_ok=True)
    for name in ("lua_result.json", "trace.csv"):
        (directory / name).unlink(missing_ok=True)
    env = dict(os.environ, FCPICO_RESULTS=str(directory), FCPICO_TRACE=str(directory / "trace.csv"),
               FCPICO_FRAMES=str(frames), FCPICO_CS1_MASK=mask, FCPICO_PROVIDER="pattern",
               FCPICO_DEBUG_PEEKS="1" if peeks else "0", SDL_AUDIODRIVER="dummy")
    with (directory / "console.log").open("w") as log:
        subprocess.run([str(binary), "--testRunner", str(HERE / "lua/frames.lua"), str(rom), "--timeout=120"],
                       cwd=directory, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    if not (directory / "lua_result.json").exists():
        raise RuntimeError(f"Mesen did not finish; see {directory / 'console.log'}")
    with (directory / "trace.csv").open() as stream:
        return list(csv.DictReader(stream))


def heartbeats(rows: list[dict]) -> list[dict]:
    """Trace rows where a heartbeat completed (the heartbeat counter advanced)."""
    beats, last = [], 0
    for row in rows:
        if row["event"] == "write" and int(row["heartbeats"]) > last:
            beats.append(row)
            last = int(row["heartbeats"])
    return beats


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--mesen", type=Path, default=DEFAULT_MESEN)
    parser.add_argument("--output", type=Path, default=ROOT / "build/mesen/check")
    parser.add_argument("--frames", type=int, default=300)
    args = parser.parse_args(argv)
    if not args.mesen.is_file():
        parser.error(f"no MesenCE at {args.mesen}; run sim/mesen/mesen.py build")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    binary = stage(args.mesen.resolve(), output)
    rom = output / "rom-fcpico.nes"
    rom.write_bytes(set_mapper((ROOT / "tutorial_project/BOOTROM/rom.NES").read_bytes()))

    plain = run(binary, rom, output / "plain", args.frames, "0xf800", peeks=False)
    peeked = run(binary, rom, output / "peeks", args.frames, "0xf800", peeks=True)
    wide = run(binary, rom, output / "mask-0xf000", args.frames, "0xf000", peeks=False)

    errors = []
    steady = heartbeats(plain)[STARTUP_HEARTBEATS:]
    if not steady:
        errors.append("fewer heartbeats than the startup window")
    elif {int(r["last_count"]) for r in steady} != {EXPECTED_COUNT}:
        errors.append(f"steady counts {sorted({int(r['last_count']) for r in steady})}, expected {EXPECTED_COUNT}")
    elif int(steady[-1]["dma_stops"]) != int(steady[0]["dma_stops"]):
        errors.append("DMA stops after startup")
    if (output / "plain/trace.csv").read_bytes() != (output / "peeks/trace.csv").read_bytes():
        errors.append("debugger reads changed the cartridge trace")
    wide_counts = {int(r["last_count"]) for r in heartbeats(wide)[STARTUP_HEARTBEATS:]}
    if wide_counts == {EXPECTED_COUNT}:
        errors.append("the 0xf000 mask produced the calibrated count; the mask has no effect")

    report = {"heartbeats": len(heartbeats(plain)), "steady_counts": sorted({int(r["last_count"]) for r in steady}),
              "wide_mask_counts": sorted(wide_counts)[:5], "errors": errors}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
