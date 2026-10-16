# SPDX-License-Identifier: BSD-3-Clause
"""Tests for the SDL2 check in sim/mesen/mesen.py: a missing, static-only or broken
SDL2 stops the build with a reason instead of a link error deep inside make.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "sim" / "mesen"))
import mesen  # noqa: E402


@pytest.fixture
def sdl(tmp_path, monkeypatch):
    """A fake sdl2-config on PATH naming tmp_path/lib, with no default linker dirs."""
    lib, bin_dir = tmp_path / "lib", tmp_path / "bin"
    lib.mkdir()
    bin_dir.mkdir()
    config = bin_dir / "sdl2-config"
    config.write_text(f"#!/bin/sh\necho -L{lib} -lSDL2\n")
    config.chmod(0o755)
    monkeypatch.setenv("PATH", str(bin_dir))
    monkeypatch.setattr(mesen, "linker_dirs", lambda: [])
    return lib


def test_shared_library_passes(sdl):
    (sdl / "libSDL2.so").write_bytes(b"")
    mesen.check_sdl2([])


def test_broken_link_is_reported(sdl):
    (sdl / "libSDL2.so").symlink_to(sdl / "missing.so")
    with pytest.raises(mesen.BuildError, match="broken link"):
        mesen.check_sdl2([])


def test_static_only_is_reported(sdl):
    (sdl / "libSDL2.a").write_bytes(b"")
    with pytest.raises(mesen.BuildError, match="only the static"):
        mesen.check_sdl2([])


def test_no_library_is_reported(sdl):
    with pytest.raises(mesen.BuildError, match="no libSDL2.so"):
        mesen.check_sdl2([])


def test_missing_sdl2_config_is_reported(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", str(tmp_path))
    with pytest.raises(mesen.BuildError, match="no sdl2-config"):
        mesen.check_sdl2([])


def test_explicit_sdl2lib_skips_the_check(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", str(tmp_path))
    mesen.check_sdl2(["SDL2LIB=-lSDL2"])

