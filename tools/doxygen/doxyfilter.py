#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Doxygen ``INPUT_FILTER`` dispatcher for the FC-PICO source tree.

Doxygen has no parser for 6502 assembly, RP2350 PIO assembly, NSD.Lib MML or
Windows batch files, yet all four carry load-bearing parts of this project.  This
filter bridges the gap: it rewrites those dialects into a minimal C-like shadow
that Doxygen *can* parse, so that labels, equates and macros become real,
cross-referenceable Doxygen entities.

Design rules
------------
1. **One output line per input line.**  Declarations therefore land on the same
   line numbers as the constructs they shadow, so ``\\ref`` links and the source
   browser agree with each other.
2. **Comments are opt-in.**  Only comments written with the explicit ``;///``
   (or ``::/`` for batch) marker are promoted to Doxygen comments.  Ordinary
   ``;`` comments -- which in this tree are the original Japanese authoring
   notes -- are dropped, so they never leak into the English reference.
3. **C-family files pass through untouched.**  The real C++ parser handles them.

Doxygen invokes this as ``doxyfilter.py <file>`` and reads the result from
stdout.  Set ``FILTER_SOURCE_FILES = NO`` so the source browser keeps showing
the genuine, unfiltered files.

@see docs/pages/conventions.md for the authoring conventions this implements.
"""

import io
import os
import re
import sys

# --- Passthrough -----------------------------------------------------------
## Extensions handed straight to Doxygen's own C/C++ parser.  Squirrel (.nut,
## .af) is close enough to C that the native parser copes with it.
PASSTHROUGH = {".c", ".cpp", ".cc", ".cxx", ".hpp", ".ino", ".nut", ".af", ".md", ".dox"}

# --- Shared token patterns -------------------------------------------------
## ``;/// text`` -> ``/// text``; ``;///< text`` -> ``///< text``.
RE_DOXY_SEMI = re.compile(r"^(\s*);///(<?)(.*)$")
## ``::/ text`` -> ``/// text`` (batch files, where ``::`` starts a comment).
RE_DOXY_BAT = re.compile(r"^(\s*)::/(<?)(.*)$")
## A trailing ``;///< text`` doc marker on an otherwise ordinary code line.
RE_TRAIL_DOC = re.compile(r";///<\s?(.*)$")


def _num(tok):
    """Translate a nesasm numeric literal into its C equivalent.

    nesasm writes hex as ``$1234`` and binary as ``%1010_0110`` (underscores are
    permitted as digit separators).  Both forms are illegal in C, so rewrite
    them to ``0x1234`` / ``0b10100110``.  Anything unrecognised is returned
    unchanged -- it is almost always already a decimal literal or a reference to
    another equate.

    @param tok the raw literal as it appears in the assembly source.
    @return a literal C's preprocessor will accept.
    """
    tok = tok.strip()
    if tok.startswith("$"):
        return "0x" + tok[1:]
    if tok.startswith("%"):
        return "0b" + tok[1:].replace("_", "")
    return tok


def _trailing_doc(line):
    """Extract a trailing ``;///<`` member comment, if the line carries one.

    @param line a full source line.
    @return the reassembled ``///< text`` suffix, or an empty string.
    """
    m = RE_TRAIL_DOC.search(line)
    return " ///< " + m.group(1).rstrip() if m else ""


# ---------------------------------------------------------------------------
# nesasm (6502) -- BOOTROM/ and BOOTROM_FIX/, both .asm and .h includes
# ---------------------------------------------------------------------------

## ``NAME EQU $1234`` / ``NAME = $1234`` -- a named constant.
RE_EQU = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+(?:EQU|equ|=)\s+(\S+)")
## ``NAME MACRO`` -- an assembler macro definition.
RE_MACRO = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+(?:MACRO|macro)\b")
## ``LABEL:`` at column zero -- a routine entry point.  Local labels start with
## ``.`` and are deliberately excluded; they are branch targets, not API.
RE_LABEL = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):")
## ``.include "file"`` -- reproduced so Doxygen can draw the include graph.
RE_INCLUDE = re.compile(r'^\s*\.include\s+"([^"]+)"')


def filter_nesasm(lines):
    """Shadow 6502 assembly as C declarations.

    ============================  ====================================
    nesasm construct              emitted shadow
    ============================  ====================================
    ``;/// text``                 ``/// text``
    ``NAME EQU $12``              ``#define NAME 0x12``
    ``NAME MACRO``                ``#define NAME(...)``
    ``LABEL:``                    ``void LABEL(void);``
    ``.include "x.asm"``          ``#include "x.asm"``
    anything else                 blank line
    ============================  ====================================

    @param lines the source lines, newline-stripped.
    @return the shadow lines, one per input line.
    """
    out = []
    for line in lines:
        m = RE_DOXY_SEMI.match(line)
        if m:
            out.append("%s///%s%s" % (m.group(1), m.group(2), m.group(3)))
            continue

        m = RE_INCLUDE.match(line)
        if m:
            out.append('#include "%s"' % m.group(1))
            continue

        m = RE_EQU.match(line)
        if m:
            out.append("#define %s %s%s" % (m.group(1), _num(m.group(2)), _trailing_doc(line)))
            continue

        m = RE_MACRO.match(line)
        if m:
            out.append("#define %s(...)%s" % (m.group(1), _trailing_doc(line)))
            continue

        m = RE_LABEL.match(line)
        if m:
            out.append("void %s(void);%s" % (m.group(1), _trailing_doc(line)))
            continue

        out.append("")
    return out


# ---------------------------------------------------------------------------
# RP2350 PIO assembly -- sys/pio/*.pio
# ---------------------------------------------------------------------------

## ``.define public NAME 21`` -- exported to C as a real macro by pioasm.
RE_PIO_DEFINE = re.compile(r"^\s*\.define\s+(?:public\s+)?([A-Za-z_][A-Za-z0-9_]*)\s+(\S+)")
## ``.program fcppu_w`` -- one PIO state-machine program.
RE_PIO_PROGRAM = re.compile(r"^\s*\.program\s+([A-Za-z_][A-Za-z0-9_]*)")


def filter_pio(lines):
    """Shadow a PIO program as C declarations.

    ``.define`` becomes a macro (mirroring what ``pioasm`` itself emits for
    ``public`` symbols) and ``.program`` becomes a function, so each state
    machine gets its own documentable entity.

    @param lines the source lines, newline-stripped.
    @return the shadow lines, one per input line.
    """
    out = []
    for line in lines:
        m = RE_DOXY_SEMI.match(line)
        if m:
            out.append("%s///%s%s" % (m.group(1), m.group(2), m.group(3)))
            continue

        m = RE_PIO_PROGRAM.match(line)
        if m:
            out.append("void %s(void);%s" % (m.group(1), _trailing_doc(line)))
            continue

        m = RE_PIO_DEFINE.match(line)
        if m:
            out.append("#define %s %s%s" % (m.group(1), _num(m.group(2)), _trailing_doc(line)))
            continue

        out.append("")
    return out


# ---------------------------------------------------------------------------
# NSD.Lib MML -- mml/*.mml, mml/*.mmh
# ---------------------------------------------------------------------------

## ``BGM(1)`` / ``SE(3)`` -- a song or sound-effect definition.
RE_MML_TRACK = re.compile(r"^\s*(BGM|SE)\s*\(\s*(\d+)\s*\)")
## ``E(2000)`` / ``Envelop(4000)`` -- an envelope table.
RE_MML_ENV = re.compile(r"^\s*(?:E|Envelop)\s*\(\s*(\d+)\s*\)")
## ``$drum{ ... }`` -- a reusable MML phrase macro.
RE_MML_MACRO = re.compile(r"^\s*\$([A-Za-z_][A-Za-z0-9_]*)\s*\{")
## ``#include "env_set0.mmh"`` -- MML already uses C-style include syntax.
RE_MML_INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"')


def filter_mml(lines):
    """Shadow MML as C declarations.

    MML already uses ``//`` and ``/* */`` comments, so ``///`` blocks are passed
    through verbatim; only the musical constructs need shadowing.

    @param lines the source lines, newline-stripped.
    @return the shadow lines, one per input line.
    """
    out = []
    in_block = False
    for line in lines:
        stripped = line.strip()

        # Preserve /* ... */ comment blocks exactly as written.
        if in_block:
            out.append(line)
            if "*/" in line:
                in_block = False
            continue
        if stripped.startswith("/*"):
            out.append(line)
            if "*/" not in line:
                in_block = True
            continue
        if stripped.startswith("///") or stripped.startswith("//!"):
            out.append(line)
            continue

        m = RE_MML_INCLUDE.match(line)
        if m:
            out.append('#include "%s"' % m.group(1))
            continue

        m = RE_MML_TRACK.match(line)
        if m:
            out.append("void %s_%s(void);%s" % (m.group(1), m.group(2), _trailing_doc(line)))
            continue

        m = RE_MML_ENV.match(line)
        if m:
            out.append("#define E_%s%s" % (m.group(1), _trailing_doc(line)))
            continue

        m = RE_MML_MACRO.match(line)
        if m:
            out.append("void %s(void);%s" % (m.group(1), _trailing_doc(line)))
            continue

        out.append("")
    return out


# ---------------------------------------------------------------------------
# Windows batch -- the build and flashing scripts
# ---------------------------------------------------------------------------


def filter_batch(lines):
    """Shadow a batch script as documentation only.

    Batch scripts have no symbols worth cross-referencing, so nothing is
    synthesised: the file exists in the reference purely to carry its ``@file``
    block explaining what the script does and in what order it must run.

    @param lines the source lines, newline-stripped.
    @return the shadow lines, one per input line.
    """
    out = []
    for line in lines:
        m = RE_DOXY_BAT.match(line)
        out.append("%s///%s%s" % (m.group(1), m.group(2), m.group(3)) if m else "")
    return out


# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------


def select_filter(path):
    """Choose the filter appropriate to *path*.

    Dispatch is by path as well as extension: ``.h`` files under ``BOOTROM/``,
    ``BOOTROM_FIX/`` and ``sample_game/GAMEROM/`` are nesasm includes, not C
    headers, and must be kept away from the C++ parser.  Routing on the path
    here is what lets a single ``INPUT_FILTER`` replace a fragile set of
    ``FILTER_PATTERNS`` globs.

    Getting this wrong is quiet rather than loud.  A missed ``.h`` is passed
    through to the C++ parser, which reads ``NAME EQU $12`` as a variable
    declaration and reports it as an undocumented member -- so the file appears
    in the reference, wrongly, instead of failing outright.

    @param path path to the file Doxygen is about to parse.
    @return a callable taking a list of lines, or ``None`` to pass through.
    """
    norm = path.replace("\\", "/").lower()
    ext = os.path.splitext(norm)[1]

    if ext in PASSTHROUGH:
        return None
    if ext == ".asm":
        return filter_nesasm
    if ext == ".pio":
        return filter_pio
    if ext in (".mml", ".mmh"):
        return filter_mml
    if ext == ".bat":
        return filter_batch
    if ext == ".h":
        # nesasm include, or genuine C header?
        if "/bootrom/" in norm or "/bootrom_fix/" in norm or "/gamerom/" in norm:
            return filter_nesasm
        return None
    return None


def main(argv):
    """Filter one file named on the command line onto stdout.

    @param argv process argument vector; ``argv[1]`` is the file to filter.
    @return 0 on success, 2 if no filename was supplied.
    """
    if len(argv) < 2:
        sys.stderr.write("usage: doxyfilter.py <file>\n")
        return 2
    path = argv[1]

    # Doxygen captures stdout as raw bytes; force UTF-8 so the Japanese
    # authoring comments that survive in passthrough files are not mangled by
    # the console's legacy code page.
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", newline="\n")

    with io.open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()

    fn = select_filter(path)
    if fn is None:
        out.write(text)
    else:
        lines = text.split("\n")
        out.write("\n".join(fn([ln.rstrip("\r") for ln in lines])))
    out.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
