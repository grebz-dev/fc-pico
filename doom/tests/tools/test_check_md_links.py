# SPDX-License-Identifier: BSD-3-Clause
"""Tests for tools/check_md_links.py."""

import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
SCRIPT_PATH = TOOLS_DIR / "check_md_links.py"


def _load_module():
    spec = importlib.util.spec_from_file_location("check_md_links", SCRIPT_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


mdlinks = _load_module()


def run_tool(directory: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(SCRIPT_PATH), str(directory)],
        capture_output=True,
        text=True,
    )


def parse_broken_lines(stdout: str):
    """Return {(file, target), ...} from ``file: target`` output lines."""
    pairs = set()
    for line in stdout.splitlines():
        if not line.strip():
            continue
        file_part, _, target_part = line.partition(": ")
        pairs.add((file_part, target_part))
    return pairs


# ---------------------------------------------------------------------------
# Unit tests: slugify / heading_slugs / strip_code_fences
# ---------------------------------------------------------------------------


def test_slugify_lowercases_and_hyphenates_spaces():
    assert mdlinks.slugify("Toolchain pins") == "toolchain-pins"


def test_slugify_strips_punctuation():
    # Colons, commas, backticks and parentheses are all punctuation that
    # GitHub's slugifier removes outright (it does not turn them into "-").
    assert (
        mdlinks.slugify("Decision D5: a register sequencer, not a 6502 emulator")
        == "decision-d5-a-register-sequencer-not-a-6502-emulator"
    )
    assert mdlinks.slugify("Music stream format (`.apus`)") == "music-stream-format-apus"


def test_slugify_keeps_hyphens_and_underscores():
    assert mdlinks.slugify("check_md_links.py") == "check_md_linkspy"
    assert mdlinks.slugify("Stage B -- decimation") == "stage-b----decimation"


def test_heading_slugs_deduplicates_like_github():
    text = "# Notes\n\n## Steps\n\nSome text.\n\n## Steps\n\nMore text.\n"
    assert mdlinks.heading_slugs(text) == {"notes", "steps", "steps-1"}


def test_strip_code_fences_blanks_fenced_content_only():
    text = "# Real heading\n\n```\n### Not a heading\n[fake](nowhere.md)\n```\n\nafter\n"
    scanned = mdlinks.strip_code_fences(text)
    assert "Not a heading" not in scanned
    assert "[fake]" not in scanned
    assert "Real heading" in scanned
    assert "after" in scanned
    # Line count is preserved so this is safe to run heading/link regexes on.
    assert scanned.count("\n") == text.count("\n")


# ---------------------------------------------------------------------------
# End-to-end: a temp tree with a mix of good and bad links
# ---------------------------------------------------------------------------


@pytest.fixture
def mixed_tree(tmp_path):
    docs = tmp_path / "docs"
    guide = docs / "guide"
    guide.mkdir(parents=True)
    assets = tmp_path / "assets"
    assets.mkdir()
    image_path = assets / "image.png"
    image_path.write_bytes(b"\x89PNG\r\n")

    (guide / "setup.md").write_text("# Setup\n\nNothing to see here.\n", encoding="utf-8")

    index_md = f"""\
# Index

Good relative link: [setup](guide/setup.md).
Good link to a directory: [guide dir](guide).
Good absolute-path link: [image]({image_path}).
Bad relative link: [missing](guide/missing.md).
External link (never checked): [ext](https://example.invalid/x/y).
Mailto link (never checked): [mail](mailto:nobody@example.com).
Anchor to a real heading: [see below](#a-real-heading).
Anchor to a missing heading: [nope](#does-not-exist).
Bare hash always resolves: [top](#).
Duplicate-heading anchors: [first](#dup-heading) and [second](#dup-heading-1).
Reference-style good link: [ref good][goodref].
Reference-style bad link: [ref bad][badref].
Collapsed reference link: [guide][].

## A Real Heading

## Dup heading

## Dup heading

Example inside a fence must be ignored entirely (no false heading, no
false-good and no false-broken link):

```
### Not A Real Heading
[fake](nonexistent/should/not/matter.md)
```

[goodref]: guide/setup.md
[badref]: guide/also-missing.md
[guide]: guide/setup.md
"""
    (docs / "index.md").write_text(index_md, encoding="utf-8")

    # These live under skipped directories and each contain an obviously
    # broken link; none of it should ever be reported.
    for skip_dir, filename in (
        ("rp2040-doom", "README.md"),
        ("Mesen2", "NOTES.md"),
        (".git", "HEAD.md"),
    ):
        d = tmp_path / skip_dir
        d.mkdir()
        (d / filename).write_text(
            "# Skipped\n\n[bad](does/not/exist.md)\n", encoding="utf-8"
        )

    return tmp_path


def test_mixed_tree_exits_1_and_reports_only_real_broken_links(mixed_tree):
    result = run_tool(mixed_tree)
    assert result.returncode == 1, result.stdout + result.stderr

    reported = parse_broken_lines(result.stdout)
    reported = {(str(Path(f)), t) for f, t in reported}
    index_md = str(mixed_tree / "docs" / "index.md")
    expected = {
        (index_md, "guide/missing.md"),
        (index_md, "#does-not-exist"),
        (index_md, "guide/also-missing.md"),
    }
    assert reported == expected

    # Nothing from the skipped directories ever gets walked or printed.
    assert "rp2040-doom" not in result.stdout
    assert "Mesen2" not in result.stdout
    assert ".git" not in result.stdout


def test_skipped_directories_are_never_reported_even_alone(tmp_path):
    for skip_dir, filename in (("rp2040-doom", "a.md"), ("Mesen2", "b.md"), (".git", "c.md")):
        d = tmp_path / skip_dir
        d.mkdir()
        (d / filename).write_text("[bad](nowhere.md)\n", encoding="utf-8")

    result = run_tool(tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "0 link(s) checked" in result.stdout


# ---------------------------------------------------------------------------
# End-to-end: an all-good tree exits 0 with a summary
# ---------------------------------------------------------------------------


def test_all_good_tree_exits_0_with_summary(tmp_path):
    (tmp_path / "a.md").write_text(
        "# A\n\nSee [b](b.md) and [the intro](#introduction).\n\n## Introduction\n",
        encoding="utf-8",
    )
    (tmp_path / "b.md").write_text("# B\n\nBack to [a](a.md).\n", encoding="utf-8")

    result = run_tool(tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "0 broken" in result.stdout
    assert "2 file(s)" in result.stdout
    assert "3 link(s) checked" in result.stdout


def test_nonexistent_directory_is_a_usage_error(tmp_path):
    result = run_tool(tmp_path / "does-not-exist")
    assert result.returncode == 2
    assert "not a directory" in result.stderr
