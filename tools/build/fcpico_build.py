#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Reproducible FC PICO platform builds on Linux: host, boot ROM, resources, firmware, emulator.

    python3 tools/build/fcpico_build.py setup [--device]   # venv, NESASM CE, Arduino toolchain
    python3 tools/build/fcpico_build.py host rom resources device
    python3 tools/build/fcpico_build.py demo               # host + emulator demo scenario
    python3 tools/build/fcpico_build.py all --verify-twice

Stages:
  setup      project-local .venv with tools/ installed, pinned NESASM CE; with --device
             (or when the device stage runs) the pinned arduino-cli, core and libraries
  host       platform libraries, native library and every C and Python test
  rom        the boot ROM with native NESASM CE (tools/build/build_rom.sh)
  resources  res.bin, res_id.h and resdata.c for tutorial_project/tuto1_hw, from the
             rebuilt ROM; identical to the committed files when the ROM is unchanged
  device     the tutorial firmware UF2 with arduino-cli; the Arduino IDE route still works
  emulator   MesenCE with the cartridge model, the mapper check and S0/S1/demo scenarios
  demo       host, then the emulator demo scenario
  all        setup, host, rom, resources, device, emulator

Outputs go to build/ (or --out) with manifest.json: source commit and dirty state, tool
versions, pinned inputs and the SHA-256 of every artifact. --verify-twice builds the
selected stages in two independent copies of the source tree and compares artifacts.

Nothing is installed system-wide. Missing system tools (a C compiler, CMake, make,
patch, git; SDL2 and the .NET SDK for the emulator) stop the build with a message.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
STAGES = ["setup", "host", "rom", "resources", "device", "emulator", "demo", "all"]
ALL = ["setup", "host", "rom", "resources", "device", "emulator"]
UF2_FAMILY_RP2350_ARM_S = 0xE48BFF59
UF2_FAMILY_ABSOLUTE = 0xE48BFF57


class BuildError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def run(args: list, **kwargs) -> subprocess.CompletedProcess:
    print("+", " ".join(str(a) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def output(args: list) -> str:
    try:
        return subprocess.run([str(a) for a in args], capture_output=True, text=True).stdout.strip()
    except OSError:
        return ""


def need(*tools: str, why: str) -> None:
    missing = [t for t in tools if shutil.which(t) is None]
    if missing:
        raise BuildError(f"{why} needs {', '.join(missing)}; install it with your package manager")


class Build:
    def __init__(self, root: Path, out: Path, jobs: int, mesen_make_args: list[str] | None = None):
        self.root, self.out, self.jobs = root, out, jobs
        self.mesen_make_args = mesen_make_args or []
        self.cache = root / ".cache"
        self.venv = root / ".venv"
        self.artifacts: dict[str, Path] = {}
        self.tools: dict[str, str] = {"python": platform.python_version()}
        self.notes: list[str] = []
        self.arduino = json.loads((HERE / "arduino.json").read_text())

    # -- helpers -------------------------------------------------------------------

    @property
    def python(self) -> Path:
        return self.venv / "bin" / "python"

    @property
    def nesasm(self) -> Path:
        return Path(os.environ.get("NESASM_BIN") or self.cache / "nesasm" / "nesasm")

    def artifact(self, name: str, path: Path) -> None:
        if not path.is_file():
            raise BuildError(f"expected artifact missing: {path}")
        self.artifacts[name] = path

    # -- setup ---------------------------------------------------------------------

    def setup(self, device: bool) -> None:
        need("git", "make", "cc", "cmake", why="setup")
        if not self.python.exists():
            run([sys.executable, "-m", "venv", self.venv])
        run([self.python, "-m", "pip", "install", "--quiet", "-e", f"{self.root / 'tools'}[test]"])
        if not self.nesasm.exists():
            run([HERE / "fetch_nesasm.sh"], env=dict(os.environ, NESASM_CACHE=str(self.cache / "nesasm")))
        if device:
            self.setup_arduino()

    def arduino_cli(self) -> Path:
        return self.cache / "arduino" / f"cli-{self.arduino['cli_version']}" / "arduino-cli"

    def arduino_config(self) -> Path:
        base = self.cache / "arduino"
        config = base / "arduino-cli.yaml"
        config.write_text(
            "board_manager:\n"
            f"  additional_urls: [{self.arduino['board_index']}]\n"
            "directories:\n"
            f"  data: {base / 'data'}\n  downloads: {base / 'downloads'}\n  user: {base / 'user'}\n"
            "updater:\n  enable_notification: false\n")
        return config

    def setup_arduino(self) -> None:
        cli = self.arduino_cli()
        if not cli.exists():
            cli.parent.mkdir(parents=True, exist_ok=True)
            archive = cli.parent / "arduino-cli.tar.gz"
            print(f"downloading {self.arduino['cli_url']}", flush=True)
            with urllib.request.urlopen(self.arduino["cli_url"]) as response, archive.open("wb") as out:
                shutil.copyfileobj(response, out)
            if sha256(archive) != self.arduino["cli_sha256"]:
                archive.unlink()
                raise BuildError("arduino-cli download does not match the pinned SHA-256")
            with tarfile.open(archive) as tar:
                tar.extract("arduino-cli", cli.parent, filter="data")
        config = self.arduino_config()
        run([cli, "--config-file", config, "core", "update-index"])
        run([cli, "--config-file", config, "core", "install", self.arduino["core"]])
        for library in self.arduino["libraries"]:
            run([cli, "--config-file", config, "lib", "install", library])

    # -- host ----------------------------------------------------------------------

    def host(self) -> None:
        need("cmake", "cc", why="the host stage")
        build = self.out / "platform"
        run(["cmake", "-S", self.root / "platform", "-B", build, "-DCMAKE_BUILD_TYPE=Release"])
        run(["cmake", "--build", build, "-j", self.jobs])
        run(["ctest", "--test-dir", build, "--output-on-failure"])
        python = self.python if self.python.exists() else Path(sys.executable)
        env = dict(os.environ, FCPICO_NATIVE_LIB=str(build / "libfcpico_native.so"),
                   NESASM_BIN=str(self.nesasm))
        run([python, "-m", "pytest", "-q", "-p", "no:cacheprovider", self.root / "tests",
             self.root / "platform" / "tests"], cwd=self.root, env=env)
        run([python, self.root / "tools/gen_protocol.py", "--check"])
        self.artifact("platform/libfcpico_native.so", build / "libfcpico_native.so")
        self.tools["cc"] = output(["cc", "--version"]).splitlines()[0]
        self.tools["cmake"] = output(["cmake", "--version"]).splitlines()[0]

    # -- rom and resources --------------------------------------------------------

    def rom(self) -> None:
        if not self.nesasm.exists():
            raise BuildError(f"NESASM CE not found at {self.nesasm}; run the setup stage")
        run([HERE / "build_rom.sh", "--out", self.out / "rom"], env=dict(os.environ, NESASM_BIN=str(self.nesasm)))
        self.artifact("rom/rom.NES", self.out / "rom/rom.NES")
        self.artifact("rom/bootrom_fixr.bin", self.out / "rom/bootrom_fixr.bin")
        self.tools["nesasm"] = output([self.nesasm, "--version"]).splitlines()[0] if self.nesasm.exists() else ""

    def resources(self) -> None:
        rom = self.out / "rom/rom.NES"
        if not rom.exists():
            raise BuildError("run the rom stage first")
        tree = self.out / "resources/tutorial_project"
        if tree.exists():
            shutil.rmtree(tree)
        res = tree / "tuto1_hw/res"
        res.mkdir(parents=True)
        (tree / "BOOTROM").mkdir()
        (tree / "mml").mkdir()
        source = self.root / "tutorial_project"
        shutil.copy(rom, tree / "BOOTROM/rom.NES")
        shutil.copy(source / "mml/sound.nsf", tree / "mml/sound.nsf")
        for name in ("binlink.lst", "OBJ.chr", "font.chr"):
            shutil.copy(source / "tuto1_hw/res" / name, res / name)
        sys.path.insert(0, str(self.root / "tools"))
        from nes import bin2c, respack  # noqa: E402
        if respack.main([str(res / "binlink.lst"), str(res / "res_id.h"), str(res / "res.bin"), "0"]) or \
                bin2c.main([str(res / "res.bin"), str(res / "resdata.c"), "_resdata"]):
            raise BuildError("resource packing failed")
        committed = source / "tuto1_hw/res"
        if rom.read_bytes() == (source / "BOOTROM/rom.NES").read_bytes():
            for name in ("res.bin", "res_id.h", "resdata.c"):
                if (res / name).read_bytes() != (committed / name).read_bytes():
                    raise BuildError(f"rebuilt {name} differs from the committed one for the same ROM")
            self.notes.append("resources match the committed tuto1_hw/res files")
        for name in ("res.bin", "res_id.h", "resdata.c"):
            self.artifact(f"resources/{name}", res / name)

    # -- device --------------------------------------------------------------------

    def device(self) -> None:
        cli = self.arduino_cli()
        if not cli.exists():
            self.setup_arduino()
        resources = self.out / "resources/tutorial_project/tuto1_hw/res"
        if not (resources / "resdata.c").exists():
            raise BuildError("run the resources stage first")
        work = self.out / "device"
        sketch = work / "src/tuto1_hw"
        if sketch.parent.exists():
            shutil.rmtree(sketch.parent)
        shutil.copytree(self.root / "tutorial_project/tuto1_hw", sketch)
        for name in ("res.bin", "res_id.h", "resdata.c"):
            shutil.copy(resources / name, sketch / "res" / name)
        arduino = self.cache / "arduino"
        prefix = (f"-ffile-prefix-map={sketch}=tuto1_hw -ffile-prefix-map={arduino / 'data'}=arduino "
                  f"-ffile-prefix-map={arduino / 'user'}=arduino-user")
        run([cli, "--config-file", self.arduino_config(), "compile", "--fqbn", self.arduino["fqbn"],
             "--build-path", work / "build", "--output-dir", work / "out",
             "--build-property", f"compiler.c.extra_flags={prefix}",
             "--build-property", f"compiler.cpp.extra_flags={prefix}", sketch])
        uf2 = work / "out/tuto1_hw.ino.uf2"
        self.check_uf2(uf2, self.out / "rom/rom.NES")
        self.artifact("device/tuto1_hw.ino.uf2", uf2)
        self.artifact("device/tuto1_hw.ino.bin", work / "out/tuto1_hw.ino.bin")
        self.tools["arduino"] = f"arduino-cli {self.arduino['cli_version']}, {self.arduino['core']}, " \
                                f"{', '.join(self.arduino['libraries'])}"

    def check_uf2(self, uf2: Path, rom: Path) -> None:
        data = uf2.read_bytes()
        if len(data) % 512:
            raise BuildError(f"{uf2} is not a whole number of UF2 blocks")
        image = bytearray()
        for offset in range(0, len(data), 512):
            block = data[offset:offset + 512]
            magic0, magic1, flags, _addr, size, _n, _total, family = \
                [int.from_bytes(block[i:i + 4], "little") for i in range(0, 32, 4)]
            if (magic0, magic1) != (0x0A324655, 0x9E5D5157):
                raise BuildError(f"{uf2}: bad UF2 block at {offset}")
            # picotool adds a block with the "absolute" family (RP2350-E10 workaround).
            if flags & 0x2000 and family == UF2_FAMILY_ABSOLUTE:
                continue
            if not flags & 0x2000 or family != UF2_FAMILY_RP2350_ARM_S:
                raise BuildError(f"{uf2}: family {family:#x}, expected RP2350 ARM Secure")
            image += block[32:32 + size]
        device_rom = rom.read_bytes()
        if device_rom not in image:
            raise BuildError("the firmware does not contain the rebuilt boot ROM")
        emulator_header = bytes(device_rom[:6]) + bytes([(device_rom[6] & 0x0F) | 0xD0])
        if image.find(emulator_header) >= 0:
            raise BuildError("the firmware contains an emulator-only mapper header")
        self.notes.append("firmware UF2 is RP2350 ARM Secure and embeds the device boot ROM")

    # -- emulator ------------------------------------------------------------------

    def emulator(self, scenarios: list[str]) -> None:
        need("make", "patch", "cmake", "dotnet", "sdl2-config", why="the emulator stage")
        mesen = self.root / "sim/mesen"
        env = dict(os.environ, NESASM_BIN=str(self.nesasm))
        make_args = [a for arg in self.mesen_make_args for a in ("--make-arg", arg)]
        run([sys.executable, mesen / "mesen.py", "build", "--jobs", self.jobs, *make_args], env=env)
        binary = self.root / "build/mesen/fcpico/src/bin/linux-x64/Release/linux-x64/publish/Mesen"
        if scenarios == ["S0", "S1", "demo"]:
            run([sys.executable, mesen / "check_mapper.py", "--mesen", binary], env=env)
        for name in scenarios:
            out = self.out / "scenarios" / name
            run([sys.executable, mesen / "scenario.py", "--mesen", binary, "run", name, "--output", out], env=env)
            self.artifact(f"scenarios/{name}/final.argb", out / "run1/final.argb")
        self.tools["dotnet"] = output(["dotnet", "--version"])
        manifest = json.loads((mesen / "manifest.json").read_text())
        self.tools["mesen"] = f"{manifest['commit']} + {len(manifest['patches'])} patches"

    # -- manifest ------------------------------------------------------------------

    def manifest(self, stages: list[str]) -> dict:
        git = lambda *a: output(["git", "-C", self.root, *a])  # noqa: E731
        document = {
            "stages": stages,
            "source": {"commit": git("rev-parse", "HEAD"), "dirty": bool(git("status", "--porcelain"))},
            "tools": self.tools,
            "pinned": {"arduino": self.arduino,
                       "mesen": json.loads((self.root / "sim/mesen/manifest.json").read_text())},
            "artifacts": {name: sha256(path) for name, path in sorted(self.artifacts.items())},
            "notes": self.notes,
        }
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / "manifest.json").write_text(json.dumps(document, indent=2) + "\n")
        return document


def expand(stages: list[str]) -> list[str]:
    result = []
    for stage in stages:
        for item in ALL if stage == "all" else ["host", "emulator:demo"] if stage == "demo" else [stage]:
            if item not in result:
                result.append(item)
    return result


def build(args, stages: list[str]) -> dict:
    b = Build(args.root.resolve(), args.out.resolve(), args.jobs, args.mesen_make_arg)
    device = args.device or "device" in stages
    for stage in stages:
        if stage == "setup":
            b.setup(device)
        elif stage == "emulator":
            b.emulator(["S0", "S1", "demo"])
        elif stage == "emulator:demo":
            b.emulator(["demo"])
        else:
            getattr(b, stage)()
    return b.manifest(stages)


def first_difference(a: Path, b: Path) -> int:
    x, y = a.read_bytes(), b.read_bytes()
    return next((i for i, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))


def verify_twice(args, stages: list[str]) -> int:
    base = args.out.resolve() / "verify"
    if "setup" in stages:
        # The environment (venv and tool caches) is shared by both copies; set it up once.
        Build(args.root.resolve(), args.out.resolve(), args.jobs).setup(args.device or "device" in stages)
        stages = [s for s in stages if s != "setup"]
    tracked = output(["git", "-C", args.root, "ls-files", "-co", "--exclude-standard"]).splitlines()
    manifests = []
    for copy in ("a", "b"):
        source = base / copy / "src"
        if source.exists():
            shutil.rmtree(source)
        for name in tracked:
            path = args.root / name
            if path.is_file():
                (source / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, source / name)
        (args.root / ".cache").mkdir(exist_ok=True)
        (source / ".cache").symlink_to(args.root.resolve() / ".cache")
        venv = args.root.resolve() / ".venv"
        if venv.exists():
            (source / ".venv").symlink_to(venv)
        run([sys.executable, source / "tools/build/fcpico_build.py", *stages, "--out", base / copy / "build",
             "--jobs", str(args.jobs)] + (["--device"] if args.device else [])
            + [a for arg in args.mesen_make_arg for a in ("--mesen-make-arg", arg)])
        manifests.append(json.loads((base / copy / "build/manifest.json").read_text()))
    a, b = manifests[0]["artifacts"], manifests[1]["artifacts"]
    differences = []
    for name in sorted(set(a) | set(b)):
        if a.get(name) != b.get(name):
            where = ""
            pa, pb = base / "a/build" / name, base / "b/build" / name
            if pa.exists() and pb.exists():
                where = f" (first difference at byte {first_difference(pa, pb)})"
            differences.append(f"{name}{where}")
    report = {"identical": sorted(n for n in a if a.get(n) == b.get(n)), "different": differences}
    (base / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 1 if differences else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog=__doc__.split("\n\n", 1)[1])
    parser.add_argument("stages", nargs="+", choices=STAGES + ["emulator:demo"])
    parser.add_argument("--out", type=Path, default=ROOT / "build")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    parser.add_argument("--device", action="store_true", help="also set up the Arduino toolchain")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    parser.add_argument("--mesen-make-arg", action="append", default=[],
                        help="extra make argument for the MesenCE build (repeatable)")
    parser.add_argument("--verify-twice", action="store_true",
                        help="build in two independent source copies and compare artifacts")
    args = parser.parse_args(argv)
    stages = expand(args.stages)
    try:
        if args.verify_twice:
            return verify_twice(args, stages)
        document = build(args, stages)
    except (BuildError, subprocess.CalledProcessError) as error:
        print(f"fcpico_build.py: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"artifacts": document["artifacts"], "notes": document["notes"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
