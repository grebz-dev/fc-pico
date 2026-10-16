#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build MesenCE with the FC PICO cartridge model from a pinned pristine source.

    python3 sim/mesen/mesen.py build            # build/mesen/fcpico/.../Mesen
    python3 sim/mesen/mesen.py build --stock    # the same source without FC PICO
    python3 sim/mesen/mesen.py check-patches    # patches apply exactly to the pinned source
    python3 sim/mesen/mesen.py regenerate DIR   # rewrite patches/ from a git checkout

The pristine source is the archive named in manifest.json, downloaded into
.cache/mesen/ (or given with --archive for offline builds) and verified against
its SHA-256. The FC PICO changes are the patch series in patches/, applied with no
fuzz and no offsets. Nothing else changes the emulator.

Building MesenCE needs a C++17 compiler, SDL2 development files and the .NET SDK
the pinned MesenCE requires (10). Before building, the SDL2 that `sdl2-config` names
is resolved the way the linker will resolve -lSDL2; a missing, static-only or broken
library stops the build with a message instead of a link error. Extra make arguments
can be passed with --make-arg; --make-arg SDL2LIB=... chooses the library yourself
and skips that check.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MANIFEST = json.loads((HERE / "manifest.json").read_text())
PATCHES = [HERE / "patches" / name for name in MANIFEST["patches"]]
CACHE = ROOT / ".cache" / "mesen"
BUILD = ROOT / "build" / "mesen"
BINARY = Path("bin/linux-x64/Release/linux-x64/publish/Mesen")


class BuildError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def pristine_archive(offline: Path | None) -> Path:
    archive = offline or CACHE / f"{MANIFEST['commit']}.tar.gz"
    if not archive.exists():
        if offline is not None:
            raise BuildError(f"archive not found: {archive}")
        CACHE.mkdir(parents=True, exist_ok=True)
        print(f"downloading {MANIFEST['archive_url']}", file=sys.stderr)
        partial = archive.with_suffix(".part")
        with urllib.request.urlopen(MANIFEST["archive_url"]) as response, partial.open("wb") as out:
            shutil.copyfileobj(response, out)
        partial.rename(archive)
    actual = sha256(archive)
    if actual != MANIFEST["archive_sha256"]:
        raise BuildError(f"{archive}: SHA-256 {actual}, manifest expects {MANIFEST['archive_sha256']}")
    return archive


def extract(archive: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split("/")[0]
        for member in tar.getmembers():
            if member.name == top:
                continue
            if not member.name.startswith(top + "/"):
                raise BuildError(f"unexpected archive member {member.name}")
            member.name = member.name[len(top) + 1:]
            tar.extract(member, destination, filter="data")


def apply_patches(source: Path, dry_run: bool) -> None:
    for patch in PATCHES:
        args = ["patch", "-p1", "--fuzz=0", "--forward", "--no-backup-if-mismatch", "-i", str(patch)]
        if dry_run:
            args.append("--dry-run")
        result = subprocess.run(args, cwd=source, capture_output=True, text=True)
        output = result.stdout + result.stderr
        if result.returncode != 0 or "offset" in output or "fuzz" in output:
            raise BuildError(f"{patch.name} does not apply exactly:\n{output}")


def source_tree(variant: str, offline: Path | None, patched: bool) -> Path:
    """The extracted source for `variant`, re-extracted when its inputs change."""
    archive = pristine_archive(offline)
    source = BUILD / variant / "src"
    inputs = MANIFEST["archive_sha256"] + "".join(sha256(p) for p in PATCHES if patched)
    stamp = BUILD / variant / "inputs.sha256"
    if not source.exists() or not stamp.exists() or stamp.read_text() != inputs:
        extract(archive, source)
        if patched:
            apply_patches(source, dry_run=True)
            apply_patches(source, dry_run=False)
        stamp.write_text(inputs)
    return source


def build_cartmodel() -> tuple[list[Path], list[Path]]:
    build = BUILD / "platform"
    subprocess.run(["cmake", "-S", str(ROOT / "platform"), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["cmake", "--build", str(build), "--target", "fcpico_cartmodel"], check=True)
    includes = [ROOT / "sim" / "cartmodel", ROOT / "platform" / "fcbus"]
    libraries = [build / "cartmodel" / "libfcpico_cartmodel.a", build / "fcbus" / "libfcbus_host.a",
                 build / "fcbus" / "libfcbus_core.a"]
    return includes, libraries


def linker_dirs() -> list[Path]:
    """The C compiler's default library search directories."""
    out = subprocess.run(["cc", "-print-search-dirs"], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if line.startswith("libraries: ="):
            return [Path(d) for d in line.split("=", 1)[1].split(":") if d]
    return [Path("/usr/lib"), Path("/lib")]


def check_sdl2(make_args: list[str]) -> None:
    """Fail early, with the reason, if -lSDL2 would not link a shared SDL2."""
    if any(arg.startswith("SDL2LIB=") for arg in make_args):
        return
    advice = ("install the SDL2 development package (libsdl2-dev on Debian and Ubuntu), "
              "or choose a library with --make-arg SDL2LIB=...")
    config = shutil.which("sdl2-config")
    if config is None:
        raise BuildError(f"SDL2 development files not found (no sdl2-config on PATH); {advice}")
    flags = subprocess.run([config, "--libs"], capture_output=True, text=True, check=True).stdout.split()
    dirs = [Path(f[2:]) for f in flags if f.startswith("-L")] + linker_dirs()
    for directory in dirs:
        shared, static = directory / "libSDL2.so", directory / "libSDL2.a"
        if shared.exists():
            return
        if shared.is_symlink():
            raise BuildError(f"{shared} (from {config}) is a broken link to {os.readlink(shared)}; {advice}")
        if static.exists():
            raise BuildError(f"only the static {static} is available (from {config}), which cannot be "
                             f"linked into Mesen's shared core; {advice}")
    raise BuildError(f"no libSDL2.so in {', '.join(map(str, dirs))} (from {config}); {advice}")


def build(args: argparse.Namespace) -> Path:
    for tool in ("make", "patch", "cmake", "dotnet"):
        if shutil.which(tool) is None:
            raise BuildError(f"{tool} is required to build MesenCE")
    check_sdl2(args.make_arg)
    variant = "stock" if args.stock else "fcpico"
    source = source_tree(variant, args.archive, patched=not args.stock)
    make = ["make", "-C", str(source), f"-j{args.jobs}", "USE_GCC=true", "LTO=false", *args.make_arg]
    if not args.stock:
        includes, libraries = build_cartmodel()
        make += [f"FCPICO_INCLUDE={' '.join(map(str, includes))}",
                 f"FCPICO_LIBS={' '.join(map(str, libraries))}"]
    env = dict(os.environ, DOTNET_CLI_TELEMETRY_OPTOUT="1",
               DOTNET_CLI_HOME=str(BUILD / "dotnet-home"), NUGET_PACKAGES=str(CACHE / "nuget"))
    subprocess.run(make, check=True, env=env)
    binary = source / BINARY
    if not binary.is_file():
        raise BuildError(f"build finished without {binary}")
    print(binary)
    return binary


def check_patches(args: argparse.Namespace) -> None:
    archive = pristine_archive(args.archive)
    with tempfile.TemporaryDirectory() as scratch:
        source = Path(scratch) / "src"
        extract(archive, source)
        apply_patches(source, dry_run=True)
    print(f"{len(PATCHES)} patches apply exactly to MesenCE {MANIFEST['commit'][:12]}")


def regenerate(args: argparse.Namespace) -> None:
    """Export the FC PICO commits of a checkout based on the pinned commit."""
    checkout = args.checkout.resolve()
    base = args.base or MANIFEST["commit"]
    out = HERE / "patches"
    for old in out.glob("*.patch"):
        old.unlink()
    subprocess.run(["git", "-C", str(checkout), "format-patch", "--zero-commit", "--no-signature",
                    "--no-numbered", "-o", str(out), f"{base}..HEAD"], check=True)
    names = sorted(p.name for p in out.glob("*.patch"))
    manifest = dict(MANIFEST, patches=names)
    (HERE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("\n".join(names))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build")
    b.add_argument("--stock", action="store_true", help="build without the FC PICO patches")
    b.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    b.add_argument("--make-arg", action="append", default=[])
    b.add_argument("--archive", type=Path, help="use this pristine archive (offline)")
    c = sub.add_parser("check-patches")
    c.add_argument("--archive", type=Path)
    r = sub.add_parser("regenerate")
    r.add_argument("checkout", type=Path)
    r.add_argument("--base", help="pristine commit in the checkout (default: the manifest commit)")
    args = parser.parse_args(argv)
    try:
        {"build": build, "check-patches": check_patches, "regenerate": regenerate}[args.command](args)
    except (BuildError, subprocess.CalledProcessError) as error:
        print(f"mesen.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
