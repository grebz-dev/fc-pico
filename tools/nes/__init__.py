# SPDX-License-Identifier: BSD-3-Clause
"""Portable Python replacements for the Windows-only NES ROM tools.

* ``bincut`` -- the cut mode of ``bin_catcut.exe``.
* ``normalize_ines`` -- converts NESASM CE's NES 2.0 header to the vendor's iNES header.
* ``check_fixbank`` -- verifies that a ROM's last 4 KB equal ``bootrom_fixr.bin``.
* ``respack`` -- ``binlink.exe``: packs resources into an indexed archive and ``res_id.h``.
* ``bin2c`` -- ``Bin2C.exe``: the archive as an aligned C array.
* ``set_mapper`` -- writes the emulator-only FC PICO mapper header (never flash the copy).
"""
