# SPDX-License-Identifier: BSD-3-Clause
"""Shared pieces for running MesenCE against the FC PICO cartridge model."""
from __future__ import annotations

import csv
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DEFAULT_MESEN = ROOT / "build/mesen/fcpico/src/bin/linux-x64/Release/linux-x64/publish/Mesen"
TUTORIAL_ROM = ROOT / "tutorial_project/BOOTROM/rom.NES"
FIX_BANK = ROOT / "tutorial_project/BOOTROM/bootrom_fixr.bin"
EXPECTED_COUNT = 15490
STARTUP_HEARTBEATS = 60

#: Deterministic, isolated emulator settings. Hardware sprite limits stay on, and
#: nothing a desktop install has configured is read or written. ConfigUpgrade 1
#: ("first run") makes the GUI apply MesenCE's default key bindings (arrow keys and
#: an Xbox pad) for interactive play; Port1 is set explicitly because the headless
#: test runner does not apply those defaults and scripted input needs a controller.
SETTINGS = {
    "ConfigUpgrade": 1,
    "Nes": {"Region": "Ntsc", "DisableGameDatabase": True, "RamPowerOnState": "AllZeros",
            "RandomizeCpuPpuAlignment": False, "RandomizeMapperPowerOnState": False,
            "RemoveSpriteLimit": False, "Port1": {"Type": "NesController"}},
    "Preferences": {"AutomaticallyCheckForUpdates": False, "EnableRewind": False,
                    "EnableAutoSaveState": False, "DisableOsd": True},
}
#: Lua file I/O is allowed only for the headless test runner.
RUNNER_SETTINGS = dict(SETTINGS, Debug={"ScriptWindow": {"AllowIoOsAccess": True}})


class RunError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stage(mesen: Path, directory: Path, settings: dict) -> Path:
    """Copy the emulator beside its own settings.json; returns the staged binary."""
    if not mesen.is_file():
        raise RunError(f"no MesenCE at {mesen}; run sim/mesen/mesen.py build")
    staging = directory / "emulator"
    shutil.copytree(mesen.parent, staging, dirs_exist_ok=True)
    (staging / "settings.json").write_text(json.dumps(settings))
    return staging / mesen.name


def run_script(binary: Path, rom: Path, lua: Path, directory: Path, env: dict[str, str],
               timeout: int = 300) -> dict:
    """Runs `lua` headless on `rom`; returns the script's lua_result.json."""
    directory.mkdir(parents=True, exist_ok=True)
    for stale in directory.glob("*"):
        if stale.is_file():
            stale.unlink()
    full_env = dict(os.environ, SDL_AUDIODRIVER="dummy", FCPICO_RESULTS=str(directory),
                    FCPICO_TRACE=str(directory / "trace.csv"), **env)
    with (directory / "console.log").open("w") as log:
        subprocess.run([str(binary), "--testRunner", str(lua), str(rom), f"--timeout={timeout}"],
                       cwd=directory, env=full_env, stdout=log, stderr=subprocess.STDOUT,
                       timeout=timeout + 60)
    result = directory / "lua_result.json"
    if not result.exists():
        raise RunError(f"Mesen did not finish; see {directory / 'console.log'}")
    return json.loads(result.read_text())


def read_trace(directory: Path) -> list[dict]:
    with (directory / "trace.csv").open() as stream:
        return list(csv.DictReader(stream))


def comparable_traces(first: Path, second: Path) -> tuple[str, str]:
    """Both runs' traces up to the last end-of-frame event they share. emu.stop() in
    an end-of-frame callback lets the emulator run on for up to a frame, so the rows
    after that point vary between otherwise identical runs."""
    def frames(directory: Path):
        lines = (directory / "trace.csv").read_text().splitlines(keepends=True)
        ends = {int(line.split(",")[1]): i for i, line in enumerate(lines) if line.startswith("frame,")}
        return lines, ends
    (a, a_ends), (b, b_ends) = frames(first), frames(second)
    common = max(set(a_ends) & set(b_ends), default=None)
    if common is None:
        return "".join(a), "".join(b)
    return "".join(a[:a_ends[common] + 1]), "".join(b[:b_ends[common] + 1])


def heartbeats(rows: list[dict]) -> list[dict]:
    """Trace rows where a heartbeat completed (the heartbeat counter advanced)."""
    beats, last = [], 0
    for row in rows:
        if row["event"] == "write" and int(row["heartbeats"]) > last:
            beats.append(row)
            last = int(row["heartbeats"])
    return beats


def git_state() -> dict:
    def git(*args: str) -> str:
        return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True,
                              text=True).stdout.strip()
    return {"commit": git("rev-parse", "HEAD"), "dirty": bool(git("status", "--porcelain"))}


def provenance(mesen: Path, **inputs: Path) -> dict:
    """Hashes of everything that determines a run."""
    manifest = HERE / "manifest.json"
    patches = sorted((HERE / "patches").glob("*.patch"))
    return {
        "source": git_state(),
        "emulator_sha256": sha256(mesen),
        "mesen_manifest_sha256": sha256(manifest),
        "patch_sha256": {p.name: sha256(p) for p in patches},
        "settings": RUNNER_SETTINGS,
        **{f"{name}_sha256": sha256(path) for name, path in inputs.items()},
    }
