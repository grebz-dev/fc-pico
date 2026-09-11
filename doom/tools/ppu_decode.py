#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""ppu_decode.py -- reconstruct the picture an fcbus stream buffer produces.

Pure-Python equivalent of what the console's PPU actually draws: unpack the
stream's 2-bit-per-pixel picture (``fcpico.stream.decode_frame``), apply an
attribute table (sub-palette per 16x16 block) and a 16-byte BG palette, and
map through the NES's NTSC colour table (``fcpico.nes_palette``) to produce
a 256x240 RGB image. Used by the L2/L3 golden-frame PSNR gate (plan 09) and
as a human-readable "what does this stream look like" tool.

Usage:
    ppu_decode.py <stream.bin> [--attr attr.bin] [--pal pal.bin]
                  [--mailbox-v2] -o out.png

``stream.bin`` is a raw 17,344-byte (v1) or 17,408-byte (v2) buffer as
produced by ``fcpico.stream.encode_frame`` / ``fcvideo_ref.convert``.
``--attr``/``--pal`` load a standalone 64-byte attribute table / 16-byte BG
palette (``$23C0-$23FF`` / ``$3F00-$3F0F``); without them the attribute
table defaults to all zero (sub-palette 0 everywhere) and the palette to the
Phase 1 boot default, the grey ramp ``0x0F 0x00 0x10 0x30`` repeated in
every sub-palette (plan 04, "Phase 1 (M1) shortcut"). ``--mailbox-v2`` reads
the attribute table and palette out of the stream's own v2 mailbox instead
(bytes 64..127 and 48..63 of the 128-byte mailbox at offset 15426), each
only if its mailbox flag bit (``MBX_FLAG_ATTR_VALID`` / ``MBX_FLAG_PAL_VALID``
in mailbox byte 0) is set; an explicit ``--attr``/``--pal`` file still wins
over the mailbox for that half.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

# So `from fcpico import ...` resolves whether this script is run directly,
# imported as a module, or loaded by path (importlib.util.spec_from_file_location).
TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from fcpico import nes_palette, protocol, stream as stream_mod  # noqa: E402

DEFAULT_PAL16 = bytes([0x0F, 0x00, 0x10, 0x30] * 4)  # Phase 1 boot default: a grey ramp everywhere


class PpuDecodeError(Exception):
    """Raised for a malformed stream buffer, attribute table or palette."""


def decode_to_rgb(stream: bytes, attr: bytes | None = None, pal: bytes | None = None) -> np.ndarray:
    """Decode a stream buffer to a ``(240, 256, 3)`` ``uint8`` RGB array.

    ``attr`` (64 bytes, ``$23C0-$23FF``) defaults to all zero. ``pal`` (16
    bytes, ``$3F00-$3F0F``) defaults to :data:`DEFAULT_PAL16`. Pixel value 0
    always shows the shared backdrop ``pal[0]``, regardless of the block's
    sub-palette (matches the PPU: BG colour 0 of every sub-palette is the
    same backdrop cell).
    """
    pix = stream_mod.decode_frame(stream).astype(np.int64)  # (240, 256), values 0..3

    if attr is None:
        attr = bytes(protocol.MBX_ATTR_LEN)
    if len(attr) != protocol.MBX_ATTR_LEN:
        raise PpuDecodeError(f"attr must be {protocol.MBX_ATTR_LEN} bytes, got {len(attr)}")
    if pal is None:
        pal = DEFAULT_PAL16
    if len(pal) != protocol.MBX_PAL_LEN:
        raise PpuDecodeError(f"pal must be {protocol.MBX_PAL_LEN} bytes, got {len(pal)}")

    attr_arr = np.frombuffer(bytes(attr), dtype=np.uint8)
    pal_arr = np.frombuffer(bytes(pal), dtype=np.uint8).astype(np.int64)

    ys, xs = np.indices(pix.shape)
    tx, ty = xs >> 3, ys >> 3
    # attr_index()/attr_shift() are plain bit ops, so they work unchanged on
    # numpy arrays -- reuse them instead of re-deriving the formula here.
    byte_idx = stream_mod.attr_index(tx, ty)
    shift = stream_mod.attr_shift(tx, ty)
    sub_pal = (attr_arr[byte_idx] >> shift) & 0x3  # (240, 256), 0..3

    pal_index = np.where(pix == 0, 0, sub_pal.astype(np.int64) * 4 + pix)
    nes_index = pal_arr[pal_index]  # (240, 256), 0..63
    return nes_palette.NES_PALETTE_RGB_ARRAY[nes_index]  # (240, 256, 3)


def _mailbox_from_stream(buf: bytes):
    """(attr_or_None, pal_or_None) from a v2 stream's own mailbox, per its flags."""
    if len(buf) != protocol.VRAM_BUF_BYTES_V2:
        raise PpuDecodeError(
            f"--mailbox-v2 needs a {protocol.VRAM_BUF_BYTES_V2}-byte (v2) stream, got {len(buf)}"
        )
    off = protocol.VRAM_MAILBOX_OFF_V2
    mailbox = buf[off : off + protocol.FC_COM_BUF_SIZE_V2]
    flags = mailbox[protocol.MBX_FLAGS]
    attr = None
    pal = None
    if flags & protocol.MBX_FLAG_ATTR_VALID:
        attr = mailbox[protocol.MBX_ATTR : protocol.MBX_ATTR + protocol.MBX_ATTR_LEN]
    if flags & protocol.MBX_FLAG_PAL_VALID:
        pal = mailbox[protocol.MBX_PAL : protocol.MBX_PAL + protocol.MBX_PAL_LEN]
    return attr, pal


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode an fcbus stream buffer to a PNG image."
    )
    parser.add_argument("stream", help="path to the raw stream buffer (17344 or 17408 bytes)")
    parser.add_argument("--attr", metavar="FILE", help="64-byte attribute table ($23C0-$23FF)")
    parser.add_argument("--pal", metavar="FILE", help="16-byte BG palette ($3F00-$3F0F)")
    parser.add_argument(
        "--mailbox-v2",
        action="store_true",
        help="take attributes/palette from the stream's own v2 mailbox where its flags say they are valid",
    )
    parser.add_argument("-o", "--output", required=True, metavar="FILE", help="output PNG path")
    args = parser.parse_args(argv)

    try:
        buf = Path(args.stream).read_bytes()

        attr = pal = None
        if args.mailbox_v2:
            attr, pal = _mailbox_from_stream(buf)
        if args.attr is not None:
            attr = Path(args.attr).read_bytes()
        if args.pal is not None:
            pal = Path(args.pal).read_bytes()

        rgb = decode_to_rgb(buf, attr, pal)
    except (OSError, PpuDecodeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    from PIL import Image

    Image.fromarray(rgb, mode="RGB").save(args.output)
    print(f"wrote {args.output} ({rgb.shape[1]}x{rgb.shape[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
