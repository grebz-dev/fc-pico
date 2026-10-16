# SPDX-License-Identifier: BSD-3-Clause
"""Decode a picture stream into NES colour indices or an image.

    python3 -m fcpico.ppu_decode STREAM --palette PAL --attr ATTR -o frame.ppm

STREAM is a stream buffer (or at least its picture), PAL the 16- or 32-byte palette
and ATTR the 64-byte attribute table. The output is binary PPM, or PNG when the
name ends in .png and Pillow is installed. --indices writes one byte per pixel (the
NES colour index) instead, for comparing against other captures.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from . import layout, nes_palette, stream


def decode(data: bytes, palette: bytes, attributes: bytes, geometry=None) -> list[list[int]]:
    """NES colour index (0..63) of every pixel of the picture in `data`."""
    return stream.colour_indices(stream.unpack_frame(data, geometry), palette, attributes)


def to_rgb(indices, table=nes_palette.NES_PALETTE_RGB) -> bytes:
    return bytes(channel for row in indices for index in row for channel in table[index])


def write_image(path: Path, indices, table=nes_palette.NES_PALETTE_RGB) -> None:
    height, width = len(indices), len(indices[0])
    rgb = to_rgb(indices, table)
    if path.suffix.lower() == ".png":
        from PIL import Image  # optional dependency
        Image.frombytes("RGB", (width, height), rgb).save(path)
    else:
        path.write_bytes(f"P6 {width} {height} 255\n".encode() + rgb)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("stream", type=Path)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--attr", type=Path, required=True)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--indices", action="store_true", help="write raw colour indices")
    parser.add_argument("--descriptor", type=Path, default=layout.DESCRIPTOR)
    args = parser.parse_args(argv)
    indices = decode(args.stream.read_bytes(), args.palette.read_bytes(), args.attr.read_bytes(),
                     layout.load(args.descriptor))
    if args.indices:
        args.output.write_bytes(bytes(i for row in indices for i in row))
    else:
        write_image(args.output, indices)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
