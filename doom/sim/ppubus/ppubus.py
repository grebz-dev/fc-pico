# SPDX-License-Identifier: BSD-3-Clause
"""ppubus.py -- host model of the console side of the fcbus PPU bus (L3).

Models qualifying `$2007` read strobes and NMI-time `$2007` write traffic at
the level of *counts and ordering*, without emulating the 6502 or the PPU's
own video timing -- see ``doom/plan/09-testing-ci.md`` ("L3 -- PPU-bus
model") and ``doom/plan/01-constraints.md`` ("The bus contract", "An
unresolved discrepancy", "A four-byte prelude from the PIO itself").

**UNCALIBRATED.** Plan 01 documents, and does not resolve, a real
discrepancy: the firmware's own qualifying-read counter
(``PPU_COUNT_VAL`` = 15,490 for v1) is *smaller* than a naive count of
pattern-table fetches implied by the stream's 34-word/scanline layout
(241 lines x 68 bytes = 16,388), and nobody yet knows which fetches the
counter misses (P0-T9's hardware trace is the plan to find out). This module
is the parameterised model plan 01 calls for, and it keeps the two rates
plan 01 insists must stay independent as two separate parameters:

``reads_per_line`` (default 64 = 32 tiles, the picture's visible width)
    Strobes the PIO counter ``fcppu_rna`` attributes to a line. This is
    what ``PPU_COUNT_VAL`` is compared against, so it alone determines
    :meth:`PpuBus.frame_read_count`.
``bytes_per_line`` (default ``reads_per_line``)
    Bytes the transmit state machine ``fcppu_r`` actually hands out for a
    line, which is what advances the DMA through the buffer. This alone
    determines :meth:`PpuBus.frame_byte_count`, and hence how many times
    :meth:`PpuBus.run_frame` calls ``cart.ppu_read()``.

They are equal by default, which is the self-consistent reading. Set
``bytes_per_line=68`` (the 34-word ``convVram()`` stride) to model the other
reading -- that the counter misses strobes the transmitter still answers --
and ``frame_read_count()`` does not move while ``frame_byte_count()`` grows
by 240 x 4 = 960 bytes. Plan 01 observes the firmware's own numbers want
962 = 241 x 4 - 2 there rather than 960; the two-byte and one-line
difference is absorbed by ``prerender_reads``, which is exactly the kind of
fudge the hardware trace is meant to replace. Do not read meaning into
either default beyond "they reproduce the firmware's totals".

``prerender_reads`` (default 61) is the third free parameter. Together their
**only** job, until P0-T10 calibrates them against a real trace, is to make
:meth:`PpuBus.frame_read_count` add up to the firmware's known totals:

    osr_prelude_bytes + prerender_reads + 240*reads_per_line + 1 + mailbox_len
        == PPU_COUNT_VAL_V1 (15,490)   for mailbox_len = 64
        == PPU_COUNT_VAL_V2 (15,554)   for mailbox_len = 128

(4 + 61 + 15,360 + 1 + 64 = 15,490; swap in 128 for v2's 15,554.) The 1 is
the NMI's mandatory dummy `$2007` read (every read sequence discards one
byte first, plan 01's "bus contract" table); the per-line split of that
total between "picture" and "pre-render" reads, and which stream bytes a
real qualifying read actually corresponds to, are exactly what is
uncalibrated -- do not read significance into the specific default values
beyond "they reproduce the totals". Golden hashes produced with these
defaults are valid regression evidence, not correctness evidence (plan 09).

Buffer layout note: because of this, :meth:`PpuBus.reconstruct` does **not**
assume the same absolute byte offsets as ``fcpico.stream`` (whose
``VRAM_MAILBOX_OFF`` = 15,426 is a real, unambiguous C-pointer offset into
the firmware's buffer, unrelated to how many bus reads the PIO counter
believes it saw). Instead ``reconstruct()`` walks the byte stream using
:meth:`PpuBus.frame_events`'s own structure (prelude, pre-render filler,
``reads_per_line`` bytes per visible line, the dummy, the mailbox) and packs
each line's bytes into pixels with ``fcpico.stream.unpack_run`` -- the same
bit-level convention (MSB-first pixel, low byte plane 0 / high byte plane 1)
``convVram()`` and ``fcpico.stream`` use, just not the same byte *offsets*.
:meth:`PpuBus.reference_buffer` builds a buffer in this module's own layout
(the exact inverse of ``reconstruct()``) for tests and for
:class:`SimpleCart`; it is deliberately not byte-identical to
``fcpico.stream.encode_frame()``'s output.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

TOOLS_DIR = Path(__file__).resolve().parent.parent.parent / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from fcpico import protocol, stream  # noqa: E402

__all__ = ["PpuBus", "SimpleCart"]


class PpuBus:
    """Parameterised model of one frame's qualifying-read/write traffic.

    Parameters mirror plan 09's ``ppubus_frame_reads(params)``:
    ``reads_per_line``, ``prerender_reads``, ``mailbox_len`` (64 for
    protocol v1, 128 for v2 -- selects both the mailbox length and the
    v1-raw-byte-vs-v2-KEY-packet heartbeat format), ``osr_prelude_bytes``
    (plan 01's PIO OSR-zero prelude) and ``cs1_mask`` (the pattern-space
    address decode from plan 09's Mesen mapper sketch; stored for parity
    with that model but not otherwise used here, since this model works at
    the byte-count level and never sees PPU addresses).
    """

    def __init__(
        self,
        reads_per_line: int = 64,
        prerender_reads: int = 61,
        mailbox_len: int = protocol.FC_COM_BUF_SIZE_V1,
        osr_prelude_bytes: int = 4,
        cs1_mask: int = 0xE000,
        bytes_per_line: int | None = None,
    ):
        if mailbox_len not in (protocol.FC_COM_BUF_SIZE_V1, protocol.FC_COM_BUF_SIZE_V2):
            raise ValueError(
                f"mailbox_len must be {protocol.FC_COM_BUF_SIZE_V1} (v1) or "
                f"{protocol.FC_COM_BUF_SIZE_V2} (v2), got {mailbox_len}"
            )
        if reads_per_line <= 0 or reads_per_line % 2:
            raise ValueError(f"reads_per_line must be a positive even number, got {reads_per_line}")
        if bytes_per_line is None:
            bytes_per_line = reads_per_line
        if bytes_per_line <= 0 or bytes_per_line % 2:
            raise ValueError(f"bytes_per_line must be a positive even number, got {bytes_per_line}")
        if bytes_per_line < reads_per_line:
            raise ValueError(
                f"bytes_per_line ({bytes_per_line}) must be at least reads_per_line "
                f"({reads_per_line}): the transmit state machine cannot answer fewer strobes "
                "than the counter sees"
            )
        self.reads_per_line = reads_per_line
        self.bytes_per_line = bytes_per_line
        # The visible picture is whatever the counted reads cover; anything the transmit
        # machine hands out beyond that (the prefetch pair, under the CS1 hypothesis) is
        # served, walked over, and never displayed.
        self.visible_bytes_per_line = reads_per_line
        self.prerender_reads = prerender_reads
        self.mailbox_len = mailbox_len
        self.osr_prelude_bytes = osr_prelude_bytes
        self.cs1_mask = cs1_mask
        self.lines = protocol.VRAM_LINES  # 240; not a constructor parameter, the console geometry is fixed

    def frame_read_count(self) -> int:
        """Qualifying reads the PIO counter attributes to one frame -- the firmware's ``ppu_count``.

        This is the number the sync decision compares against ``PPU_COUNT_VAL``. It is a
        function of ``reads_per_line`` and is **independent of** ``bytes_per_line``; see
        :meth:`frame_byte_count` and the module docstring.
        """
        return (
            self.osr_prelude_bytes
            + self.prerender_reads
            + self.lines * self.reads_per_line
            + 1  # the NMI's mandatory dummy read
            + self.mailbox_len
        )

    def frame_byte_count(self) -> int:
        """Total ``cart.ppu_read()`` calls one :meth:`run_frame` makes (no faults injected).

        Bytes the transmit state machine hands out, which is what actually advances the DMA
        through the firmware's buffer. Equal to :meth:`frame_read_count` at the defaults,
        because ``bytes_per_line`` defaults to ``reads_per_line``; the two diverge exactly to
        the extent that the counter misses strobes the transmitter still answers, which is
        the open question plan 01 leaves to the hardware trace.
        """
        return (
            self.osr_prelude_bytes
            + self.prerender_reads
            + self.lines * self.bytes_per_line
            + 1  # the NMI's mandatory dummy read
            + self.mailbox_len
        )

    def uncounted_bytes_per_frame(self) -> int:
        """Bytes served but not counted: ``frame_byte_count() - frame_read_count()``."""
        return self.frame_byte_count() - self.frame_read_count()

    def frame_events(self):
        """Yield this frame's read events in bus order.

        - ``('prelude', i)`` for ``i`` in ``0..osr_prelude_bytes``: the PIO
          OSR's zero bytes, emitted before the first real DMA byte.
        - ``('read', -1, i)`` for ``i`` in ``0..prerender_reads``: the
          pre-render scanline's reads (no picture data; NES scanline -1).
        - ``('read', line, tile)`` for ``line`` in ``0..239``, ``tile`` in
          ``0..bytes_per_line``: one visible scanline's served bytes. The
          first ``visible_bytes_per_line`` of them carry the displayed
          picture; any remainder is served and never displayed.
        - ``('nmi_read', 0)``: the mandatory dummy read of the NMI's
          `$2007` read sequence (discarded, never picture or mailbox data).
        - ``('nmi_read', i)`` for ``i`` in ``1..1+mailbox_len``: the mailbox
          bytes themselves.
        """
        for i in range(self.osr_prelude_bytes):
            yield ("prelude", i)
        for i in range(self.prerender_reads):
            yield ("read", -1, i)
        for line in range(self.lines):
            for tile in range(self.bytes_per_line):
                yield ("read", line, tile)
        yield ("nmi_read", 0)
        for i in range(1, 1 + self.mailbox_len):
            yield ("nmi_read", i)

    def run_frame(
        self,
        cart,
        heartbeat: int = 0,
        drop: int = 0,
        duplicate: int = 0,
        hold_heartbeat: bool = False,
    ) -> bytes:
        """Pull one frame's bytes from ``cart`` and deliver the heartbeat.

        Calls ``cart.ppu_read() -> int`` once per :meth:`frame_events` entry
        (in order) and returns the bytes collected. ``heartbeat`` is the
        16-bit controller state; unless ``hold_heartbeat`` is set, it is
        then delivered through ``cart.ppu_write(byte)`` -- a single raw byte
        for a v1 (``mailbox_len == 64``) bus, or the 3-byte v2
        ``FP_COM_KEY, pad1, pad2`` packet for v2.

        Fault injection (plan 09, "drop or duplicate N reads in frame K"):
        ``drop`` removes that many events off the end of this frame's read
        sequence before pulling bytes (so the returned buffer is ``drop``
        bytes short -- a simulated missed strobe); ``duplicate`` instead
        repeats the last event that many extra times (a simulated
        double-counted strobe). ``hold_heartbeat=True`` pulls the frame's
        bytes as normal but skips the ``ppu_write()`` call, simulating a
        stalled heartbeat.
        """
        events = list(self.frame_events())
        if drop:
            if drop > len(events):
                raise ValueError(f"drop={drop} exceeds this frame's {len(events)} events")
            events = events[: len(events) - drop]
        if duplicate:
            events = events + [events[-1]] * duplicate

        received = bytearray(cart.ppu_read() & 0xFF for _ in events)

        if not hold_heartbeat:
            self._write_heartbeat(cart, heartbeat)
        return bytes(received)

    def run_frames(self, cart, n_frames: int, heartbeat: int = 0, faults: dict | None = None):
        """Run ``n_frames`` consecutive frames; ``faults[k]`` (if given) are extra kwargs for frame ``k``."""
        faults = faults or {}
        return [self.run_frame(cart, heartbeat=heartbeat, **faults.get(k, {})) for k in range(n_frames)]

    def _write_heartbeat(self, cart, heartbeat: int) -> None:
        if self.mailbox_len == protocol.FC_COM_BUF_SIZE_V2:
            cart.ppu_write(protocol.FP_COM_KEY)
            cart.ppu_write((heartbeat >> 8) & 0xFF)
            cart.ppu_write(heartbeat & 0xFF)
        else:
            cart.ppu_write(heartbeat & 0xFF)

    def reconstruct(self, bytes_read: bytes):
        """Inverse of a fault-free :meth:`run_frame`: bytes -> ``(pix, mailbox)``.

        ``bytes_read`` must be exactly :meth:`frame_byte_count` bytes (what
        a fault-free ``run_frame`` returns). Skips the prelude and the
        pre-render line's reads (no picture data), then for each visible
        line unpacks the first ``visible_bytes_per_line`` of its
        ``bytes_per_line`` served bytes into pixels with
        ``fcpico.stream.unpack_run`` (so the default 64 gives the usual
        256-pixel-wide picture) and steps over the rest, discards the dummy
        read, and returns the trailing ``mailbox_len`` bytes verbatim as the
        mailbox.

        Returns ``(pix, mailbox)``: ``pix`` is a
        ``(240, visible_bytes_per_line * 4)`` ``uint8`` array of 2-bit
        colours; ``mailbox`` is ``bytes``.
        """
        data = bytes(bytes_read)
        expected = self.frame_byte_count()
        if len(data) != expected:
            raise ValueError(f"expected exactly {expected} bytes (a fault-free frame), got {len(data)}")

        pos = self.osr_prelude_bytes + self.prerender_reads  # skip prelude and the pre-render line
        words_per_line = self.visible_bytes_per_line // 2
        width = words_per_line * 8
        pix = np.zeros((self.lines, width), dtype=np.uint8)
        for line in range(self.lines):
            line_bytes = data[pos : pos + self.visible_bytes_per_line]
            pos += self.bytes_per_line
            for t in range(words_per_line):
                word = line_bytes[2 * t] | (line_bytes[2 * t + 1] << 8)
                pix[line, t * 8 : t * 8 + 8] = stream.unpack_run(word)

        pos += 1  # the NMI's dummy read
        mailbox = data[pos : pos + self.mailbox_len]
        return pix, mailbox

    def reference_buffer(self, pix, mailbox: bytes) -> bytes:
        """Build a buffer in *this module's* layout (the exact inverse of :meth:`reconstruct`).

        Not the same layout as ``fcpico.stream.encode_frame()`` (see the
        module docstring) -- this is what :class:`SimpleCart` should serve
        for :meth:`run_frame` + :meth:`reconstruct` to round-trip ``pix``
        and ``mailbox`` exactly. ``pix`` is
        ``(240, visible_bytes_per_line * 4)``, values 0..3; ``mailbox`` must
        be exactly ``mailbox_len`` bytes. Each line is followed by
        ``bytes_per_line - visible_bytes_per_line`` zero bytes, which the
        model serves and :meth:`reconstruct` steps over.
        """
        pix = np.asarray(pix, dtype=np.uint8) & 0x03
        words_per_line = self.visible_bytes_per_line // 2
        width = words_per_line * 8
        if pix.shape != (self.lines, width):
            raise ValueError(f"pix must have shape ({self.lines}, {width}), got {pix.shape}")
        mailbox = bytes(mailbox)
        if len(mailbox) != self.mailbox_len:
            raise ValueError(f"mailbox must be {self.mailbox_len} bytes, got {len(mailbox)}")

        tail = self.bytes_per_line - self.visible_bytes_per_line  # served, never displayed
        out = bytearray(self.prerender_reads)  # pre-render line: no picture data, arbitrary filler
        for line in range(self.lines):
            for t in range(words_per_line):
                word = stream.pack_run(pix[line, t * 8 : t * 8 + 8].tolist())
                out.append(word & 0xFF)
                out.append((word >> 8) & 0xFF)
            out += bytes(tail)
        out.append(0)  # consumed by the dummy read
        out += mailbox
        return bytes(out)


class SimpleCart:
    """The simplest possible cartridge: a fixed buffer served linearly.

    ``ppu_read()`` returns ``osr_prelude_bytes`` zeros (reproducing the PIO
    OSR-zero prelude on every re-arm, plan 01) and then ``buf[0], buf[1],
    ...`` forever, wrapping if more bytes are requested than ``buf`` holds.
    ``ppu_write()`` (any heartbeat write, in this model) restarts the
    sequence from the prelude, matching ``pio_sm_restart()`` being called on
    every DMA re-arm in ``rp_system::ppu_dma()``. It has no protocol logic
    at all -- pair it with :meth:`PpuBus.reference_buffer`, not a raw
    ``fcpico.stream.encode_frame()`` buffer (again, see the module
    docstring for why those are different layouts).
    """

    def __init__(self, buf: bytes, osr_prelude_bytes: int = 4):
        if not buf:
            raise ValueError("buf must not be empty")
        self._buf = bytes(buf)
        self._prelude = osr_prelude_bytes
        self._pos = 0

    def ppu_read(self) -> int:
        if self._pos < self._prelude:
            value = 0
        else:
            value = self._buf[(self._pos - self._prelude) % len(self._buf)]
        self._pos += 1
        return value

    def ppu_write(self, value: int) -> None:
        self._pos = 0
