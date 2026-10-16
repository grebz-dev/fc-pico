# SPDX-License-Identifier: BSD-3-Clause
"""Generated protocol files: freshness, regeneration, rejection and consumers."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

import gen_protocol
from fcpico import layout, protocol

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "tools" / "gen_protocol.py"


def run_gen(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(GEN), *args], capture_output=True, text=True,
                          cwd=Path("/"))


def test_committed_outputs_are_fresh():
    result = run_gen("--check")
    assert result.returncode == 0, result.stderr


def outputs(tmp: Path) -> list[str]:
    return ["--asm", str(tmp / "p.inc"), "--py", str(tmp / "p.py"), "--json", str(tmp / "p.json")]


def test_a_header_change_reaches_every_output(tmp_path):
    header = tmp_path / "fcbus_protocol.h"
    text = gen_protocol.HEADER_PATH.read_text()
    header.write_text(text.replace("#define PPU_COUNT_WINDOW     2", "#define PPU_COUNT_WINDOW     3"))
    assert run_gen("--header", str(header), *outputs(tmp_path)).returncode == 0
    assert "PPU_COUNT_WINDOW\tEQU\t3" in (tmp_path / "p.inc").read_text()
    assert "PPU_COUNT_WINDOW = 3" in (tmp_path / "p.py").read_text()
    assert json.loads((tmp_path / "p.json").read_text())["layout"]["count_window"] == 3
    stale = run_gen("--check", "--header", str(header))
    assert stale.returncode == 1 and "stale:" in stale.stderr


@pytest.mark.parametrize("line", [
    "#define BAD_CALL sizeof(int)",
    "#define BAD_NAME (UNKNOWN + 1)",
    "#define BAD_TERNARY (1 ? 2 : 3)",
])
def test_unsupported_values_fail_instead_of_disappearing(tmp_path, line):
    header = tmp_path / "fcbus_protocol.h"
    header.write_text(gen_protocol.HEADER_PATH.read_text().replace(
        "#endif /* FCBUS_PROTOCOL_H */", line + "\n#endif /* FCBUS_PROTOCOL_H */"))
    result = run_gen("--header", str(header), *outputs(tmp_path))
    assert result.returncode == 2
    assert not (tmp_path / "p.py").exists()


def test_python_constants_match_the_header():
    defs, _ = gen_protocol.parse_header(gen_protocol.HEADER_PATH.read_text())
    assert {d.name: d.value for d in defs} == protocol.ALL
    assert protocol.PF_MAGIC_NO == 0xFC


def test_layout_reader_accepts_the_descriptor_and_rejects_unknown_versions():
    geometry = layout.load()
    assert geometry.mailbox_offset == protocol.VRAM_MAILBOX_OFF
    assert geometry.words_per_line == 32
    document = json.loads(layout.DESCRIPTOR.read_text())
    document["layout_version"] = 99
    with pytest.raises(layout.LayoutError, match="unsupported layout_version"):
        layout.from_document(document)
    del document["layout"]["head_words"]
    document["layout_version"] = 1
    with pytest.raises(layout.LayoutError, match="head_words"):
        layout.from_document(document)


def _nesasm() -> str | None:
    pinned = ROOT / ".cache" / "nesasm" / "nesasm"
    if pinned.exists():
        return str(pinned)
    return os.environ.get("NESASM_BIN") or shutil.which("nesasm")


def test_assembler_include_is_valid_nesasm(tmp_path):
    nesasm = _nesasm()
    if nesasm is None:
        pytest.skip("NESASM CE not available (tools/build/fetch_nesasm.sh)")
    shutil.copy(ROOT / "platform/fcbus/gen/fcbus_protocol.inc", tmp_path)
    shutil.copy(Path(__file__).parent / "asm/protocol_smoke.asm", tmp_path)
    result = subprocess.run([nesasm, "-o", "smoke.nes", "protocol_smoke.asm"],
                            cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert (tmp_path / "smoke.nes").read_bytes()[16:19] == bytes([0xC0, 0x2F, 0x80])
