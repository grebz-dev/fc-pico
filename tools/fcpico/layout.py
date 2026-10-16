# SPDX-License-Identifier: BSD-3-Clause
"""Load the generated stream/mailbox layout descriptor, or compare it with the native one.

    from fcpico import layout
    geometry = layout.load()            # platform/fcbus/gen/fcbus_layout.json
    geometry.mailbox_offset             # 15426

    python3 tools/fcpico/layout.py --compare-native <fcbus_layout_dump>
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass, fields
from pathlib import Path

DESCRIPTOR = Path(__file__).resolve().parents[2] / "platform" / "fcbus" / "gen" / "fcbus_layout.json"
SUPPORTED_VERSIONS = frozenset({1})


class LayoutError(ValueError):
    """A descriptor this reader cannot use."""


@dataclass(frozen=True)
class Layout:
    version: int
    expected_read_count: int
    picture_reads: int
    count_report_bias: int
    count_window: int
    buffer_bytes: int
    head_words: int
    words_per_line: int
    lines: int
    mailbox_offset: int
    mailbox_bytes: int
    mailbox_magic_index: int
    mailbox_magic: int
    command_offset: int
    command_end: int
    apu_offset: int
    apu_pairs_max: int


def from_document(document: dict) -> Layout:
    version = document.get("layout_version")
    if version not in SUPPORTED_VERSIONS:
        raise LayoutError(f"unsupported layout_version {version!r}; this reader knows "
                          f"{sorted(SUPPORTED_VERSIONS)}")
    values = document.get("layout", {})
    names = [f.name for f in fields(Layout) if f.name != "version"]
    missing = [n for n in names if n not in values]
    if missing:
        raise LayoutError(f"layout descriptor lacks {', '.join(missing)}")
    return Layout(version=version, **{n: int(values[n]) for n in names})


def load(path: Path = DESCRIPTOR) -> Layout:
    return from_document(json.loads(Path(path).read_text(encoding="utf-8")))


def native(dump_program: Path) -> Layout:
    out = subprocess.run([str(dump_program)], check=True, capture_output=True, text=True).stdout
    return from_document(json.loads(out))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare the native layout with the descriptor.")
    parser.add_argument("--compare-native", type=Path, required=True, metavar="DUMP_PROGRAM")
    parser.add_argument("--descriptor", type=Path, default=DESCRIPTOR)
    args = parser.parse_args(argv)
    generated, built = load(args.descriptor), native(args.compare_native)
    if generated != built:
        for f in fields(Layout):
            a, b = getattr(generated, f.name), getattr(built, f.name)
            if a != b:
                print(f"{f.name}: descriptor {a}, native {b}", file=sys.stderr)
        return 1
    print("native layout matches the descriptor")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
