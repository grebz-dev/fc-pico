#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Run FC PICO scenarios in MesenCE, headless, or play interactively.

    python3 sim/mesen/scenario.py run S0             # strict test-pattern check
    python3 sim/mesen/scenario.py run S0 --diagnostic --cs1-mask 0xf000
    python3 sim/mesen/scenario.py run S1             # console reflashes from the cartridge
    python3 sim/mesen/scenario.py run demo           # live provider driven by scripted input
    python3 sim/mesen/scenario.py play --provider demo

Every headless scenario runs twice and requires identical traces, mailboxes and
screenshots. Results go to build/mesen/scenarios/<name>/ with report.json, which
records the verdict and the hashes of everything that determined the run.

S0 is strict: after startup the read count must be 15490 on every heartbeat, the
mailbox valid on every frame, with no DMA stops, identical results with and without
debugger reads, and the screenshot must match the reviewed golden in goldens/.
--diagnostic reports the same measurements without failing on them.

S1 boots the console from one boot ROM while the cartridge serves a rebuilt one with
a new stamp (tools/build/build_rom.sh --stamp, which needs NESASM CE). The console must
reflash itself to exactly the served PRG with the permanent bank unchanged, then
stream normally; the reverse direction must recover the original ROM.

demo runs the "demo" provider with scripted input: the square must move right and
the palette poke for the A button must reach the console's palette RAM.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import runner  # noqa: E402
from nes.set_mapper import set_mapper  # noqa: E402
from runner import EXPECTED_COUNT, STARTUP_HEARTBEATS, ROOT, heartbeats  # noqa: E402

SCENARIO_LUA = HERE / "lua" / "scenario.lua"
STARTUP_FRAMES = 150
DEMO_INPUT = "200 239 right\n300 300 a\n"


def emulator_rom(source: Path, destination: Path) -> Path:
    destination.write_bytes(set_mapper(source.read_bytes()))
    return destination


def steady_report(directory: Path, frames: int) -> dict:
    beats = heartbeats(runner.read_trace(directory))
    steady = beats[STARTUP_HEARTBEATS:]
    result = json.loads((directory / "lua_result.json").read_text())
    return {
        "heartbeats": len(beats),
        "steady_counts": sorted({int(r["last_count"]) for r in steady}),
        "stops_after_startup": int(steady[-1]["dma_stops"]) - int(steady[0]["dma_stops"]) if steady else None,
        "valid_mailboxes_after_startup": result["valid_mailboxes_after_startup"],
        "frames_after_startup": frames - STARTUP_FRAMES,
        "screenshot_sha256": runner.sha256(directory / "final.argb"),
        "trace_sha256": runner.sha256(directory / "trace.csv"),
        "mailbox_sha256": runner.sha256(directory / "mailbox.csv"),
    }


def same(a: Path, b: Path, *names: str) -> list[str]:
    differ = []
    for name in names:
        if name == "trace.csv":
            first, second = runner.comparable_traces(a, b)
        else:
            first, second = (a / name).read_bytes(), (b / name).read_bytes()
        if first != second:
            differ.append(f"{name} differs between runs")
    return differ


def strict_errors(report: dict) -> list[str]:
    errors = []
    if report["steady_counts"] != [EXPECTED_COUNT]:
        errors.append(f"steady counts {report['steady_counts']}, expected [{EXPECTED_COUNT}]")
    if report["stops_after_startup"] != 0:
        errors.append(f"{report['stops_after_startup']} DMA stops after startup")
    if report["valid_mailboxes_after_startup"] != report["frames_after_startup"]:
        errors.append("mailbox magic missing on some frames after startup")
    return errors


def scenario_s0(args, binary, output) -> tuple[dict, list[str], list[str]]:
    rom = emulator_rom(runner.TUTORIAL_ROM, output / "console.nes")
    env = {"FCPICO_PROVIDER": "pattern", "FCPICO_PROVIDER_OPTIONS": args.provider_options or "0",
           "FCPICO_CS1_MASK": args.cs1_mask, "FCPICO_FRAMES": str(args.frames),
           "FCPICO_STARTUP_FRAMES": str(STARTUP_FRAMES)}
    plain, peeked = output / "run1", output / "run2-debugger-reads"
    runner.run_script(binary, rom, SCENARIO_LUA, plain, dict(env, FCPICO_DEBUG_PEEKS="0"))
    runner.run_script(binary, rom, SCENARIO_LUA, peeked, dict(env, FCPICO_DEBUG_PEEKS="1"))
    report = steady_report(plain, args.frames)
    determinism = same(plain, peeked, "trace.csv", "mailbox.csv", "final.argb")
    errors = strict_errors(report)
    golden = HERE / "goldens" / "S0.sha256"
    if args.cs1_mask != "0xf800":
        errors.append("only the 0xf800 mask is calibrated")
    elif not golden.exists():
        errors.append("no reviewed golden screenshot (goldens/S0.sha256)")
    elif golden.read_text().split()[0] != report["screenshot_sha256"]:
        errors.append("screenshot differs from the reviewed golden")
    report["provenance"] = runner.provenance(args.mesen, console_rom=rom)
    return report, determinism, errors


def stamped_rom(output: Path) -> Path:
    built = output / "served-rom"
    nesasm = os.environ.get("NESASM_BIN") or str(ROOT / ".cache/nesasm/nesasm")
    if not Path(nesasm).exists():
        raise runner.RunError("S1 needs NESASM CE: run tools/build/fetch_nesasm.sh or set NESASM_BIN")
    subprocess.run([str(ROOT / "tools/build/build_rom.sh"), "--stamp", "--out", str(built)],
                   check=True, env=dict(os.environ, NESASM_BIN=nesasm), stdout=subprocess.DEVNULL)
    return built / "rom.NES"


def reflash_run(binary, console: Path, served: Path, directory: Path, frames: int) -> list[str]:
    env = {"FCPICO_PROVIDER": "pattern", "FCPICO_SERVE_ROM": str(served), "FCPICO_FRAMES": str(frames),
           "FCPICO_STARTUP_FRAMES": str(frames - 300), "FCPICO_DUMP_PRG": "1"}
    runner.run_script(binary, console, SCENARIO_LUA, directory, env, timeout=600)
    errors = []
    prg = (directory / "prg.bin").read_bytes()
    if prg != served.read_bytes()[16:]:
        errors.append(f"{directory.name}: console PRG differs from the served image")
    if prg[-4096:] != runner.FIX_BANK.read_bytes():
        errors.append(f"{directory.name}: permanent fix bank changed")
    rows = runner.read_trace(directory)
    if not any(r["event"] == "flash_erase" for r in rows):
        errors.append(f"{directory.name}: no flash erase observed; the console did not reflash")
    tail = heartbeats(rows)[-60:]
    if {int(r["last_count"]) for r in tail} != {EXPECTED_COUNT}:
        errors.append(f"{directory.name}: link not streaming after the reflash")
    return errors


def scenario_s1(args, binary, output) -> tuple[dict, list[str], list[str]]:
    stamped = stamped_rom(output)
    tutorial_console = emulator_rom(runner.TUTORIAL_ROM, output / "console-tutorial.nes")
    stamped_console = emulator_rom(stamped, output / "console-stamped.nes")
    errors = reflash_run(binary, tutorial_console, stamped, output / "run1", args.frames)
    errors += reflash_run(binary, tutorial_console, stamped, output / "run2", args.frames)
    errors += reflash_run(binary, stamped_console, runner.TUTORIAL_ROM, output / "reverse", args.frames)
    determinism = same(output / "run1", output / "run2", "trace.csv", "prg.bin", "final.argb")
    stamp = stamped.read_bytes()[16 + 0x6FF0:16 + 0x6FFE].decode("ascii", "replace")
    report = {"served_stamp": stamp,
              "provenance": runner.provenance(args.mesen, console_rom=tutorial_console, served_rom=stamped)}
    return report, determinism, errors


def square_left_edge(argb: bytes) -> int | None:
    """Leftmost column of pixels that are neither of the demo's grid colours."""
    pixels = [int.from_bytes(argb[i:i + 4], "little") for i in range(0, len(argb), 4)]
    background, grid = pixels[2 * 256 + 2], pixels[0]
    columns = [i % 256 for i, p in enumerate(pixels) if p not in (background, grid)]
    return min(columns) if columns else None


def scenario_demo(args, binary, output) -> tuple[dict, list[str], list[str]]:
    rom = emulator_rom(runner.TUTORIAL_ROM, output / "console.nes")
    script = output / "input.txt"
    script.write_text(DEMO_INPUT)
    env = {"FCPICO_PROVIDER": "demo", "FCPICO_FRAMES": str(args.frames),
           "FCPICO_STARTUP_FRAMES": str(STARTUP_FRAMES), "FCPICO_INPUT": str(script)}
    runner.run_script(binary, rom, SCENARIO_LUA, output / "run1", env)
    runner.run_script(binary, rom, SCENARIO_LUA, output / "run2", env)
    determinism = same(output / "run1", output / "run2", "trace.csv", "mailbox.csv", "final.argb")
    report = steady_report(output / "run1", args.frames)
    # Pressing A starts a data-mode upload, which blanks the display and makes the
    # link stop and resynchronise; require recovery instead of an unbroken stream.
    beats = heartbeats(runner.read_trace(output / "run1"))
    tail = beats[-60:]
    report.update(final_counts=sorted({int(r["last_count"]) for r in tail}),
                  dma_stops=int(beats[-1]["dma_stops"]), resyncs=int(beats[-1]["resyncs"]))
    errors = []
    if report["final_counts"] != [EXPECTED_COUNT]:
        errors.append(f"link not streaming at the end (counts {report['final_counts']})")
    edge = square_left_edge((output / "run1/final.argb").read_bytes())
    palette = (output / "run1/palette.bin").read_bytes()
    report.update(square_left_edge=edge, palette_entry_3=palette[3])
    if edge is None or edge < 160:
        errors.append(f"square did not move right (left edge {edge}, started at 120)")
    if palette[3] != 0x2A:
        errors.append(f"palette entry 3 is ${palette[3]:02X}, expected $2A after pressing A")
    report["provenance"] = runner.provenance(args.mesen, console_rom=rom, input_script=script)
    return report, determinism, errors


SCENARIOS = {"S0": (scenario_s0, 300), "S1": (scenario_s1, 3000), "demo": (scenario_demo, 400)}


def run(args) -> int:
    function, default_frames = SCENARIOS[args.scenario]
    args.frames = args.frames or default_frames
    args.mesen = args.mesen.resolve()
    output = (args.output or ROOT / "build/mesen/scenarios" / args.scenario).resolve()
    output.mkdir(parents=True, exist_ok=True)
    binary = runner.stage(args.mesen, output, runner.RUNNER_SETTINGS)
    report, determinism, errors = function(args, binary, output)
    strict = not args.diagnostic
    passed = not determinism and (not errors or not strict)
    report = dict(scenario=args.scenario, mode="strict" if strict else "diagnostic", frames=args.frames,
                  passed=passed, determinism_errors=determinism, errors=errors, **report)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "provenance"}, indent=2))
    print(f"report: {output / 'report.json'}")
    return 0 if passed else 1


def play(args) -> int:
    """Interactive launch: real keyboard/pad controls, hardware sprite limits."""
    output = (ROOT / "build/mesen/play").resolve()
    output.mkdir(parents=True, exist_ok=True)
    binary = runner.stage(args.mesen.resolve(), output, runner.SETTINGS)
    rom = emulator_rom(args.rom or runner.TUTORIAL_ROM, output / "console.nes")
    env = dict(os.environ, FCPICO_PROVIDER=args.provider, FCPICO_PROVIDER_OPTIONS=args.provider_options or "")
    print(f"MesenCE with provider '{args.provider}'; configure input in Settings > Input if needed.")
    return subprocess.run([str(binary), str(rom)], cwd=output, env=env).returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--mesen", type=Path, default=runner.DEFAULT_MESEN)
    sub = parser.add_subparsers(dest="command", required=True)
    r = sub.add_parser("run")
    r.add_argument("scenario", choices=sorted(SCENARIOS))
    r.add_argument("--diagnostic", action="store_true", help="report without strict pass/fail")
    r.add_argument("--cs1-mask", choices=["0xf800", "0xf000", "0xe000"], default="0xf800")
    r.add_argument("--frames", type=int)
    r.add_argument("--provider-options")
    r.add_argument("--output", type=Path)
    p = sub.add_parser("play")
    p.add_argument("--provider", default="demo")
    p.add_argument("--provider-options")
    p.add_argument("--rom", type=Path, help="console boot ROM (default: the tutorial's)")
    args = parser.parse_args(argv)
    try:
        return run(args) if args.command == "run" else play(args)
    except (runner.RunError, subprocess.CalledProcessError) as error:
        print(f"scenario.py: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
