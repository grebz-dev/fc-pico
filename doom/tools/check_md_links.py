#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""check_md_links.py -- verify that Markdown links under a directory resolve.

Walks every ``*.md`` file under <dir> (skipping any ``rp2040-doom`` and
``Mesen2`` directories, and ``.git``) and extracts:

  * inline links and images:      [text](target) / ![alt](target)
  * reference-style link targets: [label]: target   (the definition line --
    the only place a reference-style link actually spells out a target)

For each extracted target:

  * ``http://``, ``https://`` and ``mailto:`` targets are external and are
    never checked;
  * a target that is only a ``#fragment`` is an intra-document anchor: it is
    checked against the GitHub-style slugs of the headings in the *same*
    file rather than against the filesystem (a bare ``#`` -- a link to the
    top of the page -- always resolves);
  * anything else is a path, optionally followed by ``#fragment`` (the
    fragment is stripped and not itself checked -- only same-file anchors
    are, per the above). The path is resolved against the directory of the
    file that contains the link (or used as-is if it is absolute) and must
    exist on disk.

Links and headings inside fenced code blocks (``` ``` `` / ``~~~``) are
ignored, so example snippets quoted in the docs are never mistaken for real
links or headings.

Usage:   check_md_links.py <dir>
Exit 1, printing one ``path/to/file.md: target`` line per broken link, if
any are found; exit 0 with a one-line summary otherwise.
"""

import argparse
import os
import re
import sys
import urllib.parse
from pathlib import Path

SKIP_DIR_NAMES = {"rp2040-doom", "Mesen2", ".git"}

EXTERNAL_PREFIXES = ("http://", "https://", "mailto:")

FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")

# [text](target) / ![alt](target), optionally "<target>" and/or a "title".
# One level of nested [..] in the link text is tolerated (e.g. "[a [b]](c)").
# The text alternatives exclude newlines so a stray unmatched "[" earlier in
# the file can never make this run on and swallow an unrelated "](" later.
INLINE_LINK_RE = re.compile(
    r"\[(?:[^\[\]\n]|\[[^\[\]\n]*\])*\]"
    r"\(\s*<?([^)\s>]*)>?(?:\s+\"[^\"]*\"|\s+'[^']*')?\s*\)"
)

# [label]: target "optional title" -- a reference-style link definition.
# This is the only place a reference-style link's target is spelled out, so
# it is the only part of one that needs checking.
REFERENCE_DEF_RE = re.compile(r"^ {0,3}\[([^\]]+)\]:\s*<?([^\s>]+)>?", re.MULTILINE)

# ATX headings only ("#" .. "######"), up to 3 leading spaces per CommonMark,
# with an optional closing run of "#"s stripped. Setext headings (underlined
# with "===" / "---") are not handled: none of this repository's docs use
# them, and table separator rows (the other common source of bare "---"
# lines) all carry "|", which this pattern does not match.
ATX_HEADING_RE = re.compile(r"^ {0,3}(#{1,6})\s+(.*?)\s*#*\s*$")

# GitHub's heading-slug punctuation filter: keep word characters (letters,
# digits, underscore -- Unicode-aware), existing hyphens, and whitespace;
# drop everything else. Whitespace is turned into hyphens afterwards.
_SLUG_STRIP_RE = re.compile(r"[^\w\s-]")
_SLUG_WS_RE = re.compile(r"\s")


def strip_code_fences(text: str) -> str:
    """Blank the contents of fenced code blocks, keeping the line count.

    Applied before link/heading extraction so example text quoted inside a
    ``` fence (a template heading, a shell comment starting with "#", an
    example "[link](target)") is never mistaken for the real thing.
    """
    out_lines = []
    fence_char = None
    fence_len = 0
    for line in text.split("\n"):
        if fence_char is None:
            m = FENCE_RE.match(line)
            if m:
                fence_char = m.group(1)[0]
                fence_len = len(m.group(1))
                out_lines.append("")
            else:
                out_lines.append(line)
            continue
        stripped = line.strip()
        if len(stripped) >= fence_len and set(stripped) == {fence_char}:
            fence_char = None
            fence_len = 0
        out_lines.append("")
    return "\n".join(out_lines)


def slugify(heading_text: str) -> str:
    """GitHub-style heading slug: lowercase, punctuation removed, spaces
    (each whitespace character, not collapsed runs) become hyphens."""
    text = heading_text.strip().lower()
    text = _SLUG_STRIP_RE.sub("", text)
    text = _SLUG_WS_RE.sub("-", text)
    return text


def heading_slugs(scan_text: str) -> set:
    """All heading slugs in a (fence-stripped) document. Repeated headings
    get GitHub's duplicate numbering: the 2nd 'Foo' becomes 'foo-1', the 3rd
    'foo-2', and so on."""
    slugs = set()
    seen = {}
    for line in scan_text.split("\n"):
        m = ATX_HEADING_RE.match(line)
        if not m:
            continue
        base = slugify(m.group(2))
        n = seen.get(base, 0)
        seen[base] = n + 1
        slugs.add(base if n == 0 else f"{base}-{n}")
    return slugs


def extract_targets(scan_text: str):
    """Yield every raw link target (unresolved, un-stripped) in the text."""
    for m in INLINE_LINK_RE.finditer(scan_text):
        yield m.group(1)
    for m in REFERENCE_DEF_RE.finditer(scan_text):
        label, target = m.group(1), m.group(2)
        if label.startswith("^"):
            continue  # a footnote definition ([^1]: ...), not a link reference
        yield target


def resolve_path(base_dir: Path, target: str) -> Path:
    target = urllib.parse.unquote(target)
    if target.startswith("/"):
        return Path(target)
    return base_dir / target


def broken_links_in_file(path: Path):
    """Return (links_checked, [broken_target, ...]) for one Markdown file."""
    raw_text = path.read_text(encoding="utf-8", errors="replace")
    scan_text = strip_code_fences(raw_text)
    slugs = heading_slugs(scan_text)

    checked = 0
    broken = []
    for raw_target in extract_targets(scan_text):
        target = raw_target.strip()
        if not target or target.startswith(EXTERNAL_PREFIXES):
            continue
        checked += 1

        if target.startswith("#"):
            fragment = target[1:]
            if fragment and fragment not in slugs:
                broken.append(target)
            continue

        file_part = target.split("#", 1)[0].strip()
        if not file_part:
            continue
        if not resolve_path(path.parent, file_part).exists():
            broken.append(target)

    return checked, broken


def find_markdown_files(root: Path):
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIR_NAMES)
        for name in sorted(filenames):
            if name.endswith(".md"):
                found.append(Path(dirpath) / name)
    return found


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Check that Markdown links under a directory resolve."
    )
    parser.add_argument("dir", help="directory to walk for *.md files")
    args = parser.parse_args(argv)

    root = Path(args.dir)
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 2

    md_files = find_markdown_files(root)
    total_links = 0
    total_broken = 0
    for md_file in md_files:
        checked, broken = broken_links_in_file(md_file)
        total_links += checked
        for target in broken:
            print(f"{md_file}: {target}")
            total_broken += 1

    if total_broken:
        return 1

    print(f"check_md_links: {len(md_files)} file(s), {total_links} link(s) checked, 0 broken")
    return 0


if __name__ == "__main__":
    sys.exit(main())
