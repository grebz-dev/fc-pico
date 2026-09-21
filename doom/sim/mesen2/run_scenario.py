#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Run tutorial-ROM S0; diagnostics do not imply calibrated hardware correctness."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(REPO / "doom/tools"))
from fcpico.protocol import PPU_COUNT_VAL_V1  # noqa: E402
from nes.set_mapper import set_mapper  # noqa: E402


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def steady_heartbeat_rows(writes):
    """Return post-startup v1 heartbeat rows from the mixed mapper trace.

    The trace includes ordinary CPU writes as well as heartbeat writes.  A
    steady v1 heartbeat has one mailbox dummy/read sequence (65 CPU reads)
    and a nonzero rendering-read count; startup transitions can report
    partial or cumulative values and are excluded from the histogram.
    """
    return [
        row for row in writes
        if int(row["ppu_frame"]) > 120
        and int(row["cpu_reads"]) == 65
        and int(row["render_reads"]) > 10000
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", choices=["S0"])
    parser.add_argument("--diagnostic", action="store_true", help="check the bridge and determinism, not S0 correctness")
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--cs1-mask", choices=["0xf000", "0xe000"], default="0xf000")
    parser.add_argument("--mesen", type=Path, default=HERE / "Mesen2/bin/linux-x64/Release/linux-x64/publish/Mesen")
    parser.add_argument("--output", type=Path, default=HERE / "results/S0")
    parser.add_argument("--golden", type=Path, help="reviewed final ARGB SHA-256 text file (required for strict S0)")
    args = parser.parse_args()
    if args.frames <= 120:
        parser.error("--frames must exceed the 120-frame startup window")
    binary = args.mesen.resolve()
    if not binary.is_file():
        parser.error(f"build MesenCE first; missing {binary}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Isolate configuration and extracted native libraries from any desktop Mesen install.
    staging = output / "emulator"
    shutil.copytree(binary.parent, staging, dirs_exist_ok=True)
    config = {
        "Nes": {"Region": "Ntsc", "DisableGameDatabase": True, "RamPowerOnState": "AllZeros",
                "RandomizeCpuPpuAlignment": False, "RandomizeMapperPowerOnState": False,
                "RemoveSpriteLimit": False},
        "Preferences": {"AutomaticallyCheckForUpdates": False, "EnableRewind": False,
                        "EnableAutoSaveState": False, "DisableOsd": True},
        "Debug": {"ScriptWindow": {"AllowIoOsAccess": True}},
    }
    (staging / "settings.json").write_text(json.dumps(config))
    source = (REPO / "tutorial_project/BOOTROM/rom.NES").read_bytes()
    rom = output / "tutorial-cosim.nes"
    rom.write_bytes(set_mapper(source))
    runs = []
    for number in range(2):
        run = output / f"run{number + 1}"
        run.mkdir(exist_ok=True)
        # Prevent an interrupted earlier run's completion marker from being reused.
        for filename in ("lua_result.json", "trace.csv", "final.png", "final.argb", "mailbox.csv"):
            (run / filename).unlink(missing_ok=True)
        env = dict(os.environ, FCPICO_RESULTS=str(run), FCPICO_TRACE=str(run / "trace.csv"),
                   FCPICO_FRAMES=str(args.frames), FCPICO_CS1_MASK=args.cs1_mask,
                   FCPICO_DEBUG_PEEKS=str(number), SDL_AUDIODRIVER="dummy")
        with (run / "console.log").open("w") as log:
            process = subprocess.run([str(staging / binary.name), "--testRunner", str(HERE / "lua/S0.lua"),
                                      str(rom), "--timeout=60"], cwd=run, env=env,
                                     stdout=log, stderr=subprocess.STDOUT, timeout=90)
        if process.returncode or not (run / "lua_result.json").exists():
            print(f"Mesen run failed/incomplete: {run / 'console.log'}", file=sys.stderr)
            return 1
        result = json.loads((run / "lua_result.json").read_text())
        with (run / "trace.csv").open() as trace:
            rows = list(csv.DictReader(trace))
        writes = [row for row in rows if row["event"] == "write"]
        if not writes or not any(int(row["heartbeats"]) > 10 and int(row["init_actions"]) > 0 for row in writes):
            print(f"Cartridge bridge did not receive heartbeats; inspect {run}", file=sys.stderr)
            return 1
        # The mapper logs every CPU-side write, including protocol payload bytes and
        # ordinary controller traffic.  A steady v1 heartbeat consumes exactly the
        # mailbox plus its buffered-read dummy (65 CPU reads).  Startup transitions
        # can report partial or cumulative values, so keep those out of the steady
        # state histogram as well.
        stable = steady_heartbeat_rows(writes)
        counts = sorted({int(row["last_count"]) for row in stable})
        result.update(counts_after_120=counts, final_argb_sha256=digest(run / "final.argb"),
                      trace_sha256=digest(run / "trace.csv"), mailbox_sha256=digest(run / "mailbox.csv"),
                      stops_after_120=(int(stable[-1]["dma_stops"]) - int(stable[0]["dma_stops"])) if stable else -1,
                      final_stats=writes[-1])
        runs.append(result)
    deterministic = all(runs[0][key] == runs[1][key] for key in
                        ("trace_sha256", "mailbox_sha256", "final_argb_sha256"))
    strict_errors = []
    if runs[0]["counts_after_120"] != [PPU_COUNT_VAL_V1]:
        strict_errors.append(f"observed counts {runs[0]['counts_after_120']}, expected {PPU_COUNT_VAL_V1}")
    if runs[0]["valid_mailboxes_after_120"] != args.frames - 120:
        strict_errors.append("mailbox magic is not valid on every post-startup frame")
    if runs[0]["stops_after_120"] != 0:
        strict_errors.append("DMA stops during post-startup frames")
    if not args.golden:
        strict_errors.append("no reviewed screenshot golden supplied")
    elif args.golden.read_text().strip() != runs[0]["final_argb_sha256"]:
        strict_errors.append("screenshot does not match the reviewed golden")
    report = dict(scenario="S0", calibrated=False, cs1_mask=args.cs1_mask,
                  debug_peeks_do_not_change_results=deterministic, runs=runs,
                  strict_s0_pass=deterministic and not strict_errors, strict_errors=strict_errors)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if deterministic and (args.diagnostic or not strict_errors) else 1


if __name__ == "__main__":
    raise SystemExit(main())
