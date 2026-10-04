#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""End-to-end v5 world sprite runtime in Mesen (plan 14, P3-W3/W4 evidence).

1. The host engine (FCPICO_WORLD_SPRITES) plays a demo and records, per composed
   frame, the v4 native-HUD picture with admitted actors suppressed and their
   native world generation.
2. The Mesen cartridge model replays those frames through the real fcworld
   scheduler and gated publication against the 20DOOM-05 console ROM.
3. Every captured PPU frame must be explained by exactly one recorded frame k:
   the independent decode of stream k plus the console's actual OAM/CHR equals
   the screenshot, and world OAM slots 35..63 present exactly frame k's tiles.
   A picture shown with another frame's native actors fails both tests.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "doom/tools"))
sys.path.insert(0, str(HERE))
from build_world_atlas import mesen_palette  # noqa: E402
import build_world_sprite_rom  # noqa: E402
from fcpico import protocol as p, stream  # noqa: E402
from nes.set_mapper import set_mapper  # noqa: E402
from ppu_decode import decode_with_sprites  # noqa: E402
from run_doom_frame import check_heartbeats  # noqa: E402

FIRST, ENTRIES = p.WORLD_OAM_FIRST, p.WORLD_OAM_ENTRIES


def load_world(path: Path) -> dict:
    data = path.read_bytes()
    assert data[:4] == b"FCWF", path
    used, tiles, entries, peak = data[4:8]
    pal = data[8:14]
    tile_bytes = [data[14 + 16 * t:30 + 16 * t] for t in range(tiles)]
    oam_at = 14 + 16 * tiles
    oam = [data[oam_at + 4 * n:oam_at + 4 * n + 4] for n in range(entries)]
    return {"used": bool(used), "tiles": tile_bytes, "oam": oam, "pal": pal, "peak": peak}


def present(stored: bytes, flags: int) -> bytes:
    rows = []
    for plane in (0, 1):
        for row in range(8):
            value = stored[plane * 8 + (7 - row if flags & 0x80 else row)]
            if flags & 0x40:
                value = int(f"{value:08b}"[::-1], 2)
            rows.append(value)
    return bytes(rows[0:8] + rows[8:16])


def world_matches(world: dict, oam: bytes, chr_data: bytes, sprite_pal: bytes) -> bool:
    for n in range(ENTRIES):
        y, tile, attr, x = oam[(FIRST + n) * 4:(FIRST + n) * 4 + 4]
        if n >= len(world["oam"]):
            if y < 0xEF:
                return False
            continue
        ey, et, ea, ex = world["oam"][n]
        if (y, x, attr & 3) != (ey, ex, ea & 3):
            return False
        flips = (attr ^ ea) & 0xC0
        if present(chr_data[tile * 16:tile * 16 + 16], flips) != world["tiles"][et]:
            return False
    # A commit may load the next generation's palettes before its OAM DMA, but
    # only into slots the displayed sprites do not use: every used slot must
    # show exactly this generation's colours.
    used = {entry[2] & 3 for entry in world["oam"]}
    if 0 in used and bytes(sprite_pal[1:4]) != world["pal"][0:3]:
        return False
    if 2 in used and bytes(sprite_pal[9:12]) != world["pal"][3:6]:
        return False
    return True


def run(command: list, log: Path, **kwargs) -> None:
    with log.open("w") as out:
        subprocess.run([str(c) for c in command], check=True, stdout=out,
                       stderr=subprocess.STDOUT, **kwargs)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True,
                        help="host fcpico_doom_host built with FCPICO_WORLD_WAD")
    parser.add_argument("--whx", type=Path, default=ROOT / "doom/rp2040-doom/doom1.whx")
    parser.add_argument("--demo", type=int, default=1)
    parser.add_argument("--first", type=int, default=120, help="first recorded frame replayed")
    parser.add_argument("--count", type=int, default=150, help="recorded frames replayed")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--nesasm", type=Path, default=ROOT / ".build/tools/nesasm/nesasm")
    parser.add_argument("--mesen", type=Path,
                        default=HERE / "Mesen2/bin/linux-x64/Release/linux-x64/publish/Mesen")
    parser.add_argument("--reuse-recording", action="store_true")
    parser.add_argument("--baseline", action="store_true",
                        help="record with every actor streamed (empty world generations)")
    parser.add_argument("--evaluate-only", action="store_true",
                        help="re-check an existing output directory's captures")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)

    record = out / "record"
    if not args.reuse_recording and not args.evaluate_only:
        shutil.rmtree(record, ignore_errors=True)
        (record / "stream").mkdir(parents=True)
        command = [args.engine, "--whx", args.whx, "--demo", args.demo, "--frames",
                   args.first + args.count, "--lockstep", "--dump-stream", record / "stream"]
        command += ["--world-dump", record / "world"]
        # Baseline: identical recording format, every actor streamed.
        run(command + (["--world-off"] if args.baseline else []), out / "engine.log")
    sequence = out / "sequence"
    if not args.evaluate_only:
        shutil.rmtree(sequence, ignore_errors=True)
        for kind, suffix in (("stream", "bin"), ("world", "world")):
            (sequence / kind).mkdir(parents=True)
            for n in range(args.count):
                os.symlink(record / kind / f"frame{args.first + n:06d}.{suffix}",
                           sequence / kind / f"frame{n:06d}.{suffix}")
    worlds = [load_world(sequence / "world" / f"frame{n:06d}.world") for n in range(args.count)]
    streams = [stream.expand_native_text_stream((sequence / "stream" / f"frame{n:06d}.bin")
                                                .read_bytes()) for n in range(args.count)]

    startup = 160
    frames = startup + args.count * 8
    captures = out / "mesen"
    rom = out / "rom" / "probe.nes"
    if args.evaluate_only:
        lua = json.loads((captures / "lua_result.json").read_text())
        with (out / "trace.csv").open() as trace:
            heartbeats = check_heartbeats(csv.DictReader(trace), p.PPU_COUNT_VAL_V5, startup)
    else:
        rom_dir = out / "rom"
        rom = build_world_sprite_rom.build(rom_dir, args.nesasm)
        symbols = (rom_dir / "probe.raw.nes.0.nl").read_text()
        nmi_rti = re.search(r"^\$([0-9A-Fa-f]+)#NMI_RTI#$", symbols, re.MULTILINE).group(1)
        staging = out / "emulator"
        shutil.copytree(args.mesen.resolve().parent, staging, dirs_exist_ok=True)
        (staging / "settings.json").write_text(json.dumps({
            "Nes": {"Region": "Ntsc", "DisableGameDatabase": True, "RamPowerOnState": "AllZeros",
                    "RandomizeCpuPpuAlignment": False, "RandomizeMapperPowerOnState": False,
                    "RemoveSpriteLimit": False},
            "Preferences": {"AutomaticallyCheckForUpdates": False, "EnableRewind": False,
                            "EnableAutoSaveState": False, "DisableOsd": True},
            "Debug": {"ScriptWindow": {"AllowIoOsAccess": True}}}))
        cosim = out / "console-cosim.nes"
        cosim.write_bytes(set_mapper(rom.read_bytes()))
        captures = out / "mesen"
        shutil.rmtree(captures, ignore_errors=True)
        captures.mkdir()
        env = dict(os.environ, FCPICO_RESULTS=str(captures), FCPICO_FRAMES=str(frames),
                   FCPICO_CAPTURE_FROM=str(startup), FCPICO_NMI_RTI=nmi_rti,
                   FCPICO_WORLD_SEQUENCE=str(sequence), FCPICO_WORLD_FRAMES=str(args.count),
                   FCPICO_WORLD_LOG=str(out / "world_log.csv"), FCPICO_TRACE=str(out / "trace.csv"),
                   FCPICO_CS1_MASK="0xf800", SDL_AUDIODRIVER="dummy")
        if "DOTNET_ROOT" not in env and (Path.home() / ".dotnet/host/fxr").is_dir():
            env["DOTNET_ROOT"] = str(Path.home() / ".dotnet")
        run([staging / args.mesen.name, "--testRunner", HERE / "lua/world_runtime.lua", cosim,
             f"--timeout={60 + frames // 30}"], out / "console.log", cwd=captures, env=env,
            timeout=120 + frames // 30)
        lua = json.loads((captures / "lua_result.json").read_text())
        with (out / "trace.csv").open() as trace:
            heartbeats = check_heartbeats(csv.DictReader(trace), p.PPU_COUNT_VAL_V5, startup)

    rgb = mesen_palette()
    shown, last = [], 0
    failures = []
    # Mesen's endFrame screenshot is the previously completed picture, so
    # screenshot f pairs with the PPU state sampled when frame f-1 started.
    for f in range(startup + 1, frames + 1):
        image = np.asarray(Image.open(captures / f"f{f:05d}.png").convert("RGB"))
        stem = captures / f"f{f - 1:05d}"
        oam = stem.with_suffix(".oam").read_bytes()
        chr_data = stem.with_suffix(".chr").read_bytes()
        pal = stem.with_suffix(".pal").read_bytes()
        attr = stem.with_suffix(".attr").read_bytes()
        match = None
        window = list(range(max(0, last - 1), min(args.count, last + 4)))
        window += [k for k in range(args.count) if k not in window]
        for k in window:
            expected = decode_with_sprites(streams[k], oam, chr_data, pal[16:32], attr,
                                           pal[0:16], rgb)
            # Rows 232..239 are the native BG text row (local font, not the
            # stream); the v4 text gates cover it.
            if np.array_equal(expected[:232], image[:232, :256]):
                match = k
                break
        if match is None:
            failures.append({"ppu_frame": f, "reason": "no recorded picture explains the screenshot"})
            continue
        if not world_matches(worlds[match], oam, chr_data, pal[16:32]):
            failures.append({"ppu_frame": f, "frame": match,
                             "reason": "world OAM/CHR differs from the picture's generation"})
        peak = max(sum(oam[n * 4] + 1 <= y < oam[n * 4] + 9 for n in range(64))
                   for y in range(240))
        if peak > 8:
            failures.append({"ppu_frame": f, "frame": match, "reason": f"{peak} sprites on a line"})
        shown.append(match)
        last = match
    log = list(csv.reader((out / "world_log.csv").open()))
    end = next((row for row in log if row[0] == "end"), None)
    stats_row = next((row for row in log if row[0] == "stats"), None)
    scheduler_stats = dict(item.split("=") for item in stats_row[1:]) if stats_row else {}
    distinct = sorted(set(shown))
    published = [int(row[1]) for row in log if row[0] == "publish"]
    gaps = [b - a for a, b in zip(published, published[1:])]
    used = [w for w in worlds if w["used"]]
    report = {
        "rom_sha256": hashlib.sha256(rom.read_bytes()).hexdigest(),
        "recorded_frames": args.count, "first_recorded": args.first,
        "captured_ppu_frames": len(shown) + len(failures), "failures": failures[:50],
        "failure_count": len(failures), "distinct_frames_shown": len(distinct),
        "last_frame_shown": max(shown) if shown else None,
        "ppu_frames_per_recorded_frame": (len(shown) / len(distinct)) if distinct else None,
        "native_frames": len(used),
        "native_entries_max": max((len(w["oam"]) for w in worlds), default=0),
        "native_entries_mean": float(np.mean([len(w["oam"]) for w in used])) if used else 0,
        "native_tiles_mean": float(np.mean([len(w["tiles"]) for w in used])) if used else 0,
        "stable_heartbeats": heartbeats, "nmi": lua,
        "heartbeats_per_frame_mean": float(np.mean(gaps)) if gaps else None,
        "heartbeats_per_frame_max": max(gaps) if gaps else None,
        "transport_fps_limit": 60.0988 / float(np.mean(gaps)) if gaps else None,
        "scheduler_stats": {k: int(v) for k, v in scheduler_stats.items()},
        "scheduler_end": dict(zip(("published", "generations", "packets", "resends", "errors"),
                                  map(int, end[1:6]))) if end else None,
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "failures"}, indent=2))
    if failures:
        print(f"FAILED: {len(failures)} frames; first: {failures[0]}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
