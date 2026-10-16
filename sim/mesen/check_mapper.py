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
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import runner  # noqa: E402
from nes.set_mapper import set_mapper  # noqa: E402
from runner import EXPECTED_COUNT, STARTUP_HEARTBEATS, heartbeats  # noqa: E402


def run(binary: Path, rom: Path, directory: Path, frames: int, mask: str, peeks: bool) -> list[dict]:
    env = {"FCPICO_FRAMES": str(frames), "FCPICO_CS1_MASK": mask, "FCPICO_PROVIDER": "pattern",
           "FCPICO_DEBUG_PEEKS": "1" if peeks else "0"}
    runner.run_script(binary, rom, HERE / "lua/frames.lua", directory, env, timeout=120)
    return runner.read_trace(directory)


def comparable_traces(first: Path, second: Path) -> tuple[str, str]:
    """Both traces up to the last end-of-frame event they share. emu.stop() in an
    end-of-frame callback lets the emulator run on for up to a frame, so the rows after
    that point vary between otherwise identical runs."""
    def frames(path: Path):
        lines = path.read_text().splitlines(keepends=True)
        ends = {int(line.split(",")[1]): i for i, line in enumerate(lines) if line.startswith("frame,")}
        return lines, ends
    (a, a_ends), (b, b_ends) = frames(first), frames(second)
    common = max(set(a_ends) & set(b_ends), default=None)
    if common is None:
        return "".join(a), "".join(b)
    return "".join(a[:a_ends[common] + 1]), "".join(b[:b_ends[common] + 1])


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--mesen", type=Path, default=runner.DEFAULT_MESEN)
    parser.add_argument("--output", type=Path, default=runner.ROOT / "build/mesen/check")
    parser.add_argument("--frames", type=int, default=300)
    args = parser.parse_args(argv)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        binary = runner.stage(args.mesen.resolve(), output, runner.RUNNER_SETTINGS)
    except runner.RunError as error:
        parser.error(str(error))
    rom = output / "rom-fcpico.nes"
    rom.write_bytes(set_mapper(runner.TUTORIAL_ROM.read_bytes()))

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
    first, second = comparable_traces(output / "plain/trace.csv", output / "peeks/trace.csv")
    if first != second:
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
