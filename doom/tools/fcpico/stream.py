# SPDX-License-Identifier: BSD-3-Clause
"""stream.py -- pure-Python model of the fcbus frame-stream buffer.

Reproduces, byte for byte, the layout ``rp_system::convVram()`` and
``rp_system::ppu_dma()`` build in ``tutorial_project/tuto1_hw/sys/rp_system.cpp``
and that ``fcbus_stream_word(line, tile)`` exposes on the cartridge: a flat
buffer of 16-bit little-endian words, 34 words per scanline (two bitplanes of
an 8-pixel tile run each), with a command/attribute/palette mailbox spliced
into its tail. See ``doom/plan/04-video.md`` ("Output format") and
``doom/plan/01-constraints.md`` ("The bus contract") for the numbers this
module is built from; ``doom/fcbus/fcbus_protocol.h`` is the single source of
truth for the constants themselves (imported below, never re-typed as
literals).

Word addressing
----------------
``word_index(y, x)`` for scanline ``y`` (0..239) and tile column ``x``
(0..33) is ``VRAM_HEAD_WORDS + VRAM_LINE_WORDS * y + x`` -- the same
arithmetic as ``convVram()``'s ``vidx`` counter, which starts at
``VRAM_HEAD_WORDS`` (31) and increments once per tile. Tiles 0..31 are the
32 visible 8-pixel columns (256 px); tiles 32 and 33 are the two-tile
prefetch the PPU fetches during the next scanline's HBlank, holding that
next line's first 16 pixels (zero for line 239's prefetch, since there is no
line 240 to read from).

A note on a real quirk this module deliberately does *not* reproduce: the
firmware's own ``convVram()`` uses a single index into the 256x240 canvas
that runs continuously across the whole frame (never reset per scanline),
so each encoded line actually consumes 34*8 = 272 source pixels against a
256-wide canvas -- a 16-pixel drift that accumulates over 240 lines. This
module instead implements the clean per-line model described above and in
plan 04 (each line's own 256 px, prefetch = the *next* line's first 16 px),
which matches the firmware exactly for the word/bit-level layout below (word
indices, bit packing, mailbox offset) and for line 0, and is what
``ppu_decode.py`` and ``fcvideo_ref.py`` are built against.

Bit packing
-----------
Each word packs one 8-pixel run of 2-bit palette indices exactly as
``convVram()``'s inner loop does: starting from ``dt = 0``, for each pixel
(left to right) ``dt = (dt << 1) | conv_tbl[c]`` with
``conv_tbl = (0x0000, 0x0001, 0x0100, 0x0101)``. After 8 iterations the low
byte holds bitplane 0 and the high byte holds bitplane 1, MSB (bit 7) first
-- i.e. the leftmost pixel of the run lands in bit 7 of both bytes.

Mailbox
-------
``rp_system::ppu_dma()`` copies the outgoing mailbox to byte offset
``PPU_COUNT_VAL - FC_COM_BUF_SIZE`` = 15426 (``VRAM_MAILBOX_OFF_V1`` /
``VRAM_MAILBOX_OFF_V2``, both 15426 -- the mailbox tail sits at the same
byte position regardless of protocol version, only its length changes) of
the *same* buffer ``convVram()`` just filled, so those bytes are always the
mailbox's, never picture data, no matter what ``encode_frame()`` computed
for the words that happen to fall there. Because word 31 + 34*225 + 32 is
byte 15426 exactly, a 64-byte (v1) mailbox overwrites line 225's prefetch
and most of line 226's visible tiles; a 128-byte (v2) mailbox reaches into
line 227. In the real video pipeline (plan 04, stage B) those lines fall in
the bottom letterbox band (console lines 216..239), which is always
backdrop colour, so the overlap is invisible -- ``decode_frame()`` below
does not special-case it, so decoding a buffer with picture data actually
placed in that band will show mailbox bytes misread as pixels there.

Attribute table
----------------
The 64-byte attribute table packs a 2-bit sub-palette choice per 16x16-pixel
block, four blocks (a 2x2 quadrant) per byte, exactly as
``rp_system::setAtr()``: byte index ``(tx >> 2) + ((ty >> 2) << 3)`` for tile
coordinates ``tx``, ``ty`` (0..31, 0..29), quadrant
``((tx >> 1) & 1) + (((ty >> 1) & 1) << 1)`` selecting a 2-bit field at shift
0/2/4/6.
"""

from __future__ import annotations

import numpy as np

from fcpico import protocol

__all__ = [
    "word_index",
    "pack_run",
    "unpack_run",
    "encode_frame",
    "decode_frame",
    "attr_index",
    "attr_shift",
    "attr_get",
    "attr_set",
]

# Re-exported for readability below; fcbus/fcbus_protocol.h remains the
# single source of truth (see tools/gen_protocol.py).
VRAM_HEAD_WORDS = protocol.VRAM_HEAD_WORDS  # 31
VRAM_LINE_WORDS = protocol.VRAM_LINE_WORDS  # 34
VRAM_LINES = protocol.VRAM_LINES  # 240
VRAM_TILE_COLS = protocol.VRAM_TILE_COLS  # 32 visible tiles/line
VRAM_WIDTH = VRAM_TILE_COLS * 8  # 256 visible pixels/line
VRAM_MAILBOX_OFF = protocol.VRAM_MAILBOX_OFF_V1  # 15426; == VRAM_MAILBOX_OFF_V2

_CONV_TBL = (0x0000, 0x0001, 0x0100, 0x0101)  # convVram()'s conv_tbl, indexed by 2-bit colour
_PREFETCH_PIXELS = (VRAM_LINE_WORDS - VRAM_TILE_COLS) * 8  # 16: next line's prefetch, in pixels

_ATTR_MASK_TBL = (0b11111100, 0b11110011, 0b11001111, 0b00111111)
_ATTR_SET_TBL = (0b00000001, 0b00000100, 0b00010000, 0b01000000)


def word_index(y: int, x: int) -> int:
    """Word index of scanline ``y`` (0..239), tile column ``x`` (0..33).

    ``31 + 34*y + x`` -- ``convVram()``'s ``vidx`` counter, also
    ``fcbus_stream_word(line, tile)`` on the cartridge.
    """
    if not (0 <= y < VRAM_LINES):
        raise ValueError(f"y must be 0..{VRAM_LINES - 1}, got {y}")
    if not (0 <= x < VRAM_LINE_WORDS):
        raise ValueError(f"x must be 0..{VRAM_LINE_WORDS - 1}, got {x}")
    return VRAM_HEAD_WORDS + VRAM_LINE_WORDS * y + x


def pack_run(pixels8) -> int:
    """Pack 8 left-to-right 2-bit colours into one 16-bit stream word.

    Bit-for-bit what ``convVram()``'s inner loop computes: the leftmost
    pixel ends up in bit 7 of both the low byte (bitplane 0) and the high
    byte (bitplane 1).
    """
    pixels8 = list(pixels8)
    if len(pixels8) != 8:
        raise ValueError(f"pack_run needs exactly 8 pixels, got {len(pixels8)}")
    dt = 0
    for c in pixels8:
        dt = ((dt << 1) | _CONV_TBL[c & 3]) & 0xFFFF
    return dt


def unpack_run(word: int):
    """Inverse of :func:`pack_run`: 16-bit word -> list of 8 2-bit colours, left to right."""
    lo = word & 0xFF
    hi = (word >> 8) & 0xFF
    return [((lo >> (7 - i)) & 1) | (((hi >> (7 - i)) & 1) << 1) for i in range(8)]


def _buf_layout(mailbox_len: int):
    """(total buffer bytes, mailbox byte offset) for a v1 (64) or v2 (128) mailbox."""
    if mailbox_len == protocol.FC_COM_BUF_SIZE_V1:
        return protocol.VRAM_BUF_BYTES_V1, protocol.VRAM_MAILBOX_OFF_V1
    if mailbox_len == protocol.FC_COM_BUF_SIZE_V2:
        return protocol.VRAM_BUF_BYTES_V2, protocol.VRAM_MAILBOX_OFF_V2
    raise ValueError(
        f"mailbox must be {protocol.FC_COM_BUF_SIZE_V1} (v1) or "
        f"{protocol.FC_COM_BUF_SIZE_V2} (v2) bytes, got {mailbox_len}"
    )


def encode_frame(pix256x240, mailbox: bytes) -> bytearray:
    """Encode a 256x240 2-bit-per-pixel frame into an fcbus stream buffer.

    ``pix256x240`` is indexed ``[y][x]`` (row-major, matching ``numpy``'s
    default), values 0..3; anything ``numpy.asarray`` accepts works. The
    buffer length (17,344 or 17,408 bytes) is picked from ``len(mailbox)``
    (64 or 128); the mailbox is spliced in at byte
    :data:`VRAM_MAILBOX_OFF` (15426), verbatim, after the picture words are
    written -- see the module docstring for why that clobbers a little
    picture data near the bottom of the frame, matching real firmware.
    """
    pix = np.asarray(pix256x240, dtype=np.uint8) & 0x03
    if pix.shape != (VRAM_LINES, VRAM_WIDTH):
        raise ValueError(f"pix256x240 must have shape ({VRAM_LINES}, {VRAM_WIDTH}), got {pix.shape}")
    mailbox = bytes(mailbox)
    buf_bytes, mailbox_off = _buf_layout(len(mailbox))

    # One flat canvas, row-major, padded so that the last line's prefetch
    # (which would read line 240) reads zeros instead of running off the end.
    flat = pix.reshape(-1)
    padded = np.concatenate([flat, np.zeros(_PREFETCH_PIXELS, dtype=np.uint8)])

    starts = (
        np.arange(VRAM_LINES, dtype=np.int64)[:, None] * VRAM_WIDTH
        + np.arange(VRAM_LINE_WORDS, dtype=np.int64)[None, :] * 8
    )  # (240, 34): flat-canvas index of each tile run's first pixel
    runs = padded[starts[:, :, None] + np.arange(8)]  # (240, 34, 8), values 0..3

    shifts = np.arange(7, -1, -1, dtype=np.uint32)  # bit 7 (leftmost pixel) .. bit 0
    plane0 = ((runs.astype(np.uint32) & 1) << shifts).sum(axis=-1)
    plane1 = (((runs.astype(np.uint32) >> 1) & 1) << shifts).sum(axis=-1)
    words = (plane0 | (plane1 << 8)).astype("<u2")  # (240, 34)

    n_words = buf_bytes // 2
    word_arr = np.zeros(n_words, dtype="<u2")
    first = word_index(0, 0)
    word_arr[first : first + words.size] = words.reshape(-1)

    buf = bytearray(word_arr.tobytes())
    buf[mailbox_off : mailbox_off + len(mailbox)] = mailbox
    return buf


def decode_frame(buf) -> np.ndarray:
    """Decode an fcbus stream buffer back to a 256x240 array of 2-bit colours.

    Only the 32 visible tile columns of each line are read (the prefetch
    columns are not part of the picture); works for either buffer length as
    long as it is at least long enough to hold all picture words.
    """
    buf = bytes(buf)
    n_words_needed = word_index(VRAM_LINES - 1, VRAM_LINE_WORDS - 1) + 1
    min_bytes = n_words_needed * 2
    if len(buf) < min_bytes:
        raise ValueError(f"buffer too short: need at least {min_bytes} bytes, got {len(buf)}")

    word_arr = np.frombuffer(buf, dtype="<u2", count=n_words_needed)
    first = word_index(0, 0)
    lines = word_arr[first:].reshape(VRAM_LINES, VRAM_LINE_WORDS)
    visible = lines[:, :VRAM_TILE_COLS].astype(np.uint32)  # (240, 32)

    lo = visible & 0xFF
    hi = (visible >> 8) & 0xFF
    shifts = np.arange(7, -1, -1, dtype=np.uint32)
    p0 = (lo[:, :, None] >> shifts) & 1
    p1 = (hi[:, :, None] >> shifts) & 1
    pixels = (p0 | (p1 << 1)).astype(np.uint8)  # (240, 32, 8): colour per pixel
    return pixels.reshape(VRAM_LINES, VRAM_WIDTH)


def attr_index(tx: int, ty: int) -> int:
    """Attribute-table byte index for tile coordinates ``tx`` (0..31), ``ty`` (0..29)."""
    return ((tx >> 2) + ((ty >> 2) << 3)) & 0x3F


def attr_shift(tx: int, ty: int) -> int:
    """Bit shift (0, 2, 4 or 6) of the 2-bit field for tile coordinates ``tx``, ``ty``."""
    quadrant = ((tx >> 1) & 1) + (((ty >> 1) & 1) << 1)
    return quadrant * 2


def attr_get(attr64, px: int, py: int) -> int:
    """Sub-palette (0..3) selected for pixel ``(px, py)`` by a 64-byte attribute table."""
    tx, ty = px >> 3, py >> 3
    idx = attr_index(tx, ty)
    shift = attr_shift(tx, ty)
    return (attr64[idx] >> shift) & 0x3


def attr_set(attr64, block_x: int, block_y: int, p: int) -> None:
    """Set the sub-palette of one 16x16-pixel block (in place).

    ``block_x`` 0..15, ``block_y`` 0..14 (16x15 blocks cover 256x240
    pixels); ``p`` 0..3. Same packing as ``rp_system::setAtr()``: a block is
    two tile columns/rows wide, so this is :func:`attr_index`/
    :func:`attr_shift` evaluated at tile coordinates ``2*block_x``,
    ``2*block_y``.
    """
    idx = ((block_x >> 1) + ((block_y >> 1) << 3)) & 0x3F
    sel = (block_x & 1) + ((block_y & 1) << 1)
    attr64[idx] = (attr64[idx] & _ATTR_MASK_TBL[sel]) | (_ATTR_SET_TBL[sel] * (p & 3))
