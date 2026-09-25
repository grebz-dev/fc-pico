#!/usr/bin/env python3
"""D0/D1/S1: compare one real Doom stream's visible picture with Mesen's PPU output.

With --serve-rom the console starts from --rom while the cartridge serves a
different boot ROM, so the fix bank must erase and reprogram its flash (S1)."""

from __future__ import annotations

import argparse
import csv
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
from nes.set_mapper import set_mapper  # noqa: E402
from fcpico import protocol  # noqa: E402
from ppu_decode import decode_to_rgb  # noqa: E402


def check_heartbeats(rows, expected_count: int, startup_frames: int) -> int:
    """Check every completed heartbeat, including those with no rendering reads."""
    previous_frames = 0
    previous_stops = 0
    samples = 0
    for row in rows:
        frames = int(row["heartbeats"])
        stops = int(row["dma_stops"])
        steady = int(row["ppu_frame"]) > startup_frames
        if steady and stops > previous_stops:
            raise AssertionError(
                f"unstable frame transfer: count={row['last_count']}, "
                f"expected={expected_count}, new DMA stops={stops - previous_stops}"
            )
        if frames > previous_frames and steady:
            count = int(row["last_count"])
            if count != expected_count:
                raise AssertionError(
                    f"unstable frame transfer: count={count}, expected={expected_count}, "
                    f"new DMA stops={stops - previous_stops}"
                )
            samples += 1
        previous_frames, previous_stops = frames, stops
    if samples < 20:
        raise AssertionError(f"only {samples} post-startup heartbeats; expected at least 20")
    return samples


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stream", type=Path, help="17,408-byte converted Doom frame")
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "tutorial_project/BOOTROM/rom.NES",
                        help="console ROM (tutorial v1 by default; use Doom ROM for v2)")
    parser.add_argument("--mesen", type=Path,
                        default=HERE / "Mesen2/bin/linux-x64/Release/linux-x64/publish/Mesen")
    parser.add_argument("--serve-rom", type=Path,
                        help="boot ROM the cartridge serves (default: the console ROM)")
    parser.add_argument("--frames", type=int, default=180,
                        help="frames to run; checks sample the last 30")
    parser.add_argument("--output", type=Path, default=HERE / "results/D0")
    args = parser.parse_args()
    source = args.stream.resolve()
    encoded = source.read_bytes()
    if len(encoded) != protocol.VRAM_BUF_BYTES_V2:
        parser.error(f"stream must be {protocol.VRAM_BUF_BYTES_V2} bytes")
    mailbox = encoded[protocol.VRAM_MAILBOX_OFF_V2:
                      protocol.VRAM_MAILBOX_OFF_V2 + protocol.FC_COM_BUF_SIZE_V2]
    attr = mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR + protocol.MBX_ATTR_LEN]
    pal = mailbox[protocol.MBX_PAL:protocol.MBX_PAL + protocol.MBX_PAL_LEN]

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    staging = output / "emulator"
    binary = args.mesen.resolve()
    shutil.copytree(binary.parent, staging, dirs_exist_ok=True)
    config = {
        "Nes": {"Region": "Ntsc", "DisableGameDatabase": True,
                "RamPowerOnState": "AllZeros", "RandomizeCpuPpuAlignment": False,
                "RandomizeMapperPowerOnState": False, "RemoveSpriteLimit": False},
        "Preferences": {"AutomaticallyCheckForUpdates": False, "EnableRewind": False,
                        "EnableAutoSaveState": False, "DisableOsd": True},
        "Debug": {"ScriptWindow": {"AllowIoOsAccess": True}},
    }
    (staging / "settings.json").write_text(json.dumps(config))
    rom_source = args.rom.resolve()
    rom_data = rom_source.read_bytes()
    served_source = (args.serve_rom or args.rom).resolve()
    served_data = served_source.read_bytes()
    doom_v2 = served_data[16 + protocol.FCBUS_ROM_STAMP_OFF:
                       16 + protocol.FCBUS_ROM_STAMP_OFF + 10] == b"20DOOM-02-"
    nmi_rti = None
    if doom_v2:
        symbols = served_source.with_name("doom.raw.nes.0.nl").read_text()
        match = re.search(r"^\$([0-9A-Fa-f]+)#NMI_RTI#$", symbols, re.MULTILINE)
        if match is None:
            raise AssertionError("Doom ROM has no NMI_RTI assembler symbol")
        nmi_rti = match.group(1)
    rom = output / "console-cosim.nes"
    rom.write_bytes(set_mapper(rom_data))
    run = output / "run"
    run.mkdir(exist_ok=True)
    for stale in ("lua_result.json", "final.png", "prg.bin", "palette.bin", "attributes.bin", "mailbox.bin"):
        (run / stale).unlink(missing_ok=True)
    env = dict(os.environ, FCPICO_RESULTS=str(run), FCPICO_FRAMES=str(args.frames),
               FCPICO_STARTUP_FRAMES=str(args.frames - 30), FCPICO_DEBUG_PEEKS="0",
               FCPICO_STREAM_FILE=str(source), SDL_AUDIODRIVER="dummy",
               FCPICO_CS1_MASK="0xf800", FCPICO_TRACE=str(run / "trace.csv"))
    if nmi_rti:
        env["FCPICO_NMI_RTI"] = nmi_rti
    if args.serve_rom:
        env["FCPICO_SERVE_ROM"] = str(served_source)
    with (run / "console.log").open("w") as log:
        completed = subprocess.run(
            [str(staging / binary.name), "--testRunner", str(HERE / "lua/D0.lua"),
             str(rom), f"--timeout={60 + args.frames // 30}"], cwd=run, env=env, stdout=log,
            stderr=subprocess.STDOUT, timeout=90 + args.frames // 30,
        )
    if completed.returncode or not (run / "lua_result.json").is_file():
        raise AssertionError(f"Mesen failed; inspect {run / 'console.log'}")
    console_prg = (run / "prg.bin").read_bytes()
    if console_prg != served_data[16:16 + protocol.FCBUS_ROM_PRG_BYTES]:
        differing = sum(a != b for a, b in zip(console_prg, served_data[16:]))
        raise AssertionError(f"console PRG differs from the served ROM in {differing} bytes")
    if args.serve_rom:
        print(f"S1 reflash: console PRG now equals {served_source.name}")
    expected_count = protocol.PPU_COUNT_VAL_V2 if doom_v2 else protocol.PPU_COUNT_VAL_V1
    with (run / "trace.csv").open() as trace:
        samples = check_heartbeats(csv.DictReader(trace), expected_count, args.frames - 30)
    print(f"frame transfer: {samples} stable heartbeats, count={expected_count}, no DMA stops")
    actual_palette = (run / "palette.bin").read_bytes()
    actual_attr = (run / "attributes.bin").read_bytes()
    print(f"palette={actual_palette.hex()} expected={pal.hex()}")
    print(f"attribute differing bytes={sum(a != b for a, b in zip(actual_attr, attr))}/64")
    actual = np.asarray(Image.open(run / "final.png").convert("L"), dtype=np.float64)
    # Use the console's observed tables to isolate stream/PPU picture correctness;
    # v1 table-transfer equality is the next acceptance slice.
    expected = decode_to_rgb(encoded, actual_attr, actual_palette)
    reference = np.asarray(Image.fromarray(expected).convert("L"), dtype=np.float64)
    # Compare visible structure, not emulator-specific NTSC colour calibration.
    coefficient = float(np.corrcoef(actual[16:216].ravel(),
                                    reference[16:216].ravel())[0, 1])
    print(f"{'D1' if doom_v2 else 'D0'} visible-frame correlation={coefficient:.4f}; "
          f"image={run / 'final.png'}")
    if not np.isfinite(coefficient) or coefficient < 0.90:
        raise AssertionError("Mesen visible frame does not match the Doom PPU model")
    if actual_palette != pal or actual_attr != attr:
        raise AssertionError("Mesen palette/attribute RAM differs from the stream mailbox")
    if doom_v2:
        actual_mailbox = (run / "mailbox.bin").read_bytes()
        expected_mailbox = bytearray(mailbox)
        if not (expected_mailbox[protocol.MBX_FLAGS] & protocol.MBX_FLAG_APU_VALID):
            # fcbus stamps the explicit no-APU terminator when it publishes;
            # host frame dumps leave this unused slot zero-filled.
            expected_mailbox[protocol.MBX_APU] = 0xFF
        if actual_mailbox != expected_mailbox:
            raise AssertionError("Doom v2 console mailbox differs from the stream")
        timing = json.loads((run / "lua_result.json").read_text())
        print(f"v2 NMI exits: count={timing['nmi_count']}, "
              f"scanlines={timing['nmi_min_scanline']}..{timing['nmi_max_scanline']}")
        if (timing["nmi_count"] < 20 or timing["nmi_max_scanline"] >= 261 or
                timing["nmi_min_scanline"] < 241):
            raise AssertionError("Doom v2 NMI did not complete within vblank")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
