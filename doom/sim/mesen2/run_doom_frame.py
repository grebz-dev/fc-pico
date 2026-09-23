#!/usr/bin/env python3
"""D0: compare one real Doom stream's visible picture with Mesen's PPU output."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stream", type=Path, help="17,408-byte converted Doom frame")
    parser.add_argument("--mesen", type=Path,
                        default=HERE / "Mesen2/bin/linux-x64/Release/linux-x64/publish/Mesen")
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
    rom = output / "tutorial-cosim.nes"
    rom.write_bytes(set_mapper((ROOT / "tutorial_project/BOOTROM/rom.NES").read_bytes()))
    run = output / "run"
    run.mkdir(exist_ok=True)
    for stale in ("lua_result.json", "final.png", "palette.bin", "attributes.bin"):
        (run / stale).unlink(missing_ok=True)
    env = dict(os.environ, FCPICO_RESULTS=str(run), FCPICO_FRAMES="180",
               FCPICO_STARTUP_FRAMES="150", FCPICO_DEBUG_PEEKS="0",
               FCPICO_STREAM_FILE=str(source), SDL_AUDIODRIVER="dummy")
    with (run / "console.log").open("w") as log:
        completed = subprocess.run(
            [str(staging / binary.name), "--testRunner", str(HERE / "lua/D0.lua"),
             str(rom), "--timeout=60"], cwd=run, env=env, stdout=log,
            stderr=subprocess.STDOUT, timeout=90,
        )
    if completed.returncode or not (run / "lua_result.json").is_file():
        raise AssertionError(f"Mesen failed; inspect {run / 'console.log'}")
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
    print(f"D0 visible-frame correlation={coefficient:.4f}; image={run / 'final.png'}")
    if not np.isfinite(coefficient) or coefficient < 0.90:
        raise AssertionError("Mesen visible frame does not match the Doom PPU model")
    if actual_palette != pal or actual_attr != attr:
        raise AssertionError("Mesen palette/attribute RAM differs from the stream mailbox")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
