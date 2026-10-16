# SPDX-License-Identifier: BSD-3-Clause
"""The fcpico package needs only the standard library (Pillow is optional)."""
from __future__ import annotations

import ast
import sys
from pathlib import Path

PACKAGE = Path(__file__).resolve().parents[2] / "tools" / "fcpico"
OPTIONAL = {"PIL"}


def test_imports_are_standard_library_or_package_local():
    allowed = set(sys.stdlib_module_names) | OPTIONAL
    for source in PACKAGE.glob("*.py"):
        for node in ast.walk(ast.parse(source.read_text())):
            if isinstance(node, ast.Import):
                names = [alias.name for alias in node.names]
            elif isinstance(node, ast.ImportFrom):
                if node.level:  # relative: inside the package
                    continue
                names = [node.module or ""]
            else:
                continue
            for name in names:
                assert name.split(".")[0] in allowed, f"{source.name} imports {name}"
