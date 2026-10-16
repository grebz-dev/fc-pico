# SPDX-License-Identifier: BSD-3-Clause
"""platform/fcbus/fcbus_protocol.h must match the tutorial firmware and boot ROM.

Run with ``python3 -m pytest platform/tests``.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "platform/fcbus/fcbus_protocol.h"
RP_SYSTEM = ROOT / "tutorial_project/tuto1_hw/sys/rp_system.h"
SYS_PICO = ROOT / "tutorial_project/BOOTROM/SysPico.asm"

DEFINE = re.compile(r"^#define\s+([A-Z_0-9]+)\s+([^/\n]+?)\s*(?://.*)?$", re.M)


def _evaluate(expr: str, known: dict[str, int]) -> int:
    if not re.fullmatch(r"[\sA-Za-z_0-9x()+\-*/]+", expr):
        raise ValueError(f"unsupported expression: {expr!r}")
    expr = re.sub(r"sizeof\s*\(\s*uint32_t\s*\)", "4", expr)
    value = eval(expr.replace("/", "//"), {"__builtins__": {}}, dict(known))
    return int(value)


def header_constants() -> dict[str, int]:
    known: dict[str, int] = {}
    for name, expr in DEFINE.findall(HEADER.read_text()):
        if name == "FCBUS_PROTOCOL_H":
            continue
        known[name] = _evaluate(expr, known)
    return known


def rp_system_constants() -> dict[str, int]:
    text = re.sub(r"//[^\n]*", "", RP_SYSTEM.read_text(encoding="utf-8"))
    known: dict[str, int] = {}
    for name, expr in re.findall(r"^\s*#define\s+([A-Z_0-9]+)\s+([^\n]+)$", text, re.M):
        try:
            known[name] = _evaluate(expr.strip(), known)
        except (ValueError, NameError, SyntaxError):
            pass
    for name, value in re.findall(r"^\s*(FP_COM_[A-Z]+)\s*=\s*(0x[0-9A-Fa-f]+)", text, re.M):
        known[name] = int(value, 16)
    return known


def sys_pico_constants() -> dict[str, int]:
    known = {}
    for name, value in re.findall(r"^([A-Z_]+)\s*=\s*(\$[0-9A-Fa-f]+|\d+)", SYS_PICO.read_text(encoding="utf-8"), re.M):
        known[name] = int(value[1:], 16) if value.startswith("$") else int(value)
    return known


def test_firmware_constants_match_rp_system():
    ours, theirs = header_constants(), rp_system_constants()
    names = [n for n in ours if n.startswith(("PF_", "KEY_")) and n != "PF_MAGIC_NO"]
    names += ["PF_MAGIC_NO", "FC_COM_BUF_SIZE16", "FC_COM_BUF_SIZE", "PICO_SNDREG",
              "PICO_APU_BUF_SIZE", "PPU_COUNT_VAL",
              "FP_COM_VER", "FP_COM_ROM", "FP_COM_LOG", "FP_COM_DRQ", "FP_COM_DLD",
              "FP_COM_RST", "FP_COM_INI"]
    for name in names:
        assert ours[name] == theirs[name], name
    assert ours["VRAM_BUF_BYTES"] == theirs["VRAM_BUF_SIZE"] * 4


def test_console_constants_match_sys_pico():
    ours, asm = header_constants(), sys_pico_constants()
    aliases = {"PF_MAGIC_NO": "PF_MAGIC_CODE"}
    names = [n for n in ours if n.startswith(("PF_", "FP_COM_"))]
    for name in names:
        assert ours[name] == asm[aliases.get(name, name)], name


def test_derived_geometry():
    c = header_constants()
    assert c["PPU_COUNT_VAL"] == 15490
    assert c["VRAM_MAILBOX_OFF"] == 15426
    assert c["APU_PAIRS_MAX"] == (c["FC_COM_BUF_SIZE"] - 2 - c["PICO_SNDREG"]) // 2
    assert c["MBX_APU"] == c["PICO_SNDREG"]
