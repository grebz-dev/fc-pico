# SPDX-License-Identifier: BSD-3-Clause
"""Capture what a console would receive from a simulated cartridge.

Drives a native cartridge (``fcpico.native``) the way the tutorial boot ROM does:
FP_COM_INI, then frames of qualifying reads followed by the controller byte, with the
data-mode exchange whenever the mailbox carries PF_COM_DMOD. Returns the last
picture, its mailbox, and the palette and attribute table uploaded so far.
"""
from __future__ import annotations

from dataclasses import dataclass

from . import protocol


@dataclass
class Capture:
    stream: bytes        # bytes read in the last frame, from byte 0 (picture then mailbox)
    palette: bytes       # 32 bytes uploaded in data mode
    attributes: bytes    # 64 bytes uploaded in data mode
    data_uploads: int


class Console:
    def __init__(self, cart, geometry):
        self.cart, self.g = cart, geometry
        self.palette, self.attributes = bytearray(32), bytearray(64)
        self.uploads = 0
        self.stream = b""

    def _word(self) -> int:
        return int.from_bytes(self.cart.read_bytes(4), "little")

    def data_mode(self) -> None:
        while True:
            self.cart.write(protocol.FP_COM_DRQ)
            head, size = self._word(), self._word()
            if (head - protocol.FCBUS_DRQ_MAGIC_BASE) & 0xFF != protocol.PF_DAT_VRAM:
                return
            address = head >> 16
            self.cart.write(protocol.FP_COM_DLD)
            self.cart.write(0)
            if address == 0x23C0:
                self.cart.read()  # the boot ROM's dummy read before attribute uploads
            page = self.cart.read_bytes(256)
            if address == 0x3F00:
                self.palette[:size] = page[:size]
            elif address == 0x23C0:
                self.attributes[:size] = page[:size]
            self.uploads += 1

    def frame(self, pad: int = 0) -> None:
        g = self.g
        self.stream = self.cart.read_bytes(g.mailbox_offset + g.mailbox_bytes)
        self.cart.read()  # the NMI's dummy $2007 read
        self.cart.write(pad)
        box = self.stream[g.mailbox_offset:]
        if box[g.mailbox_magic_index] == g.mailbox_magic and \
                protocol.PF_COM_DMOD in box[g.command_offset:g.command_end].split(b"\0", 1)[0]:
            self.data_mode()


def capture(cart, geometry, frames: int = 12, pads=()) -> Capture:
    """Boots `cart` and runs `frames` frames; `pads` gives the controller byte per frame."""
    console = Console(cart, geometry)
    cart.write(protocol.FP_COM_INI)
    cart.write(0)
    pads = list(pads)
    for n in range(frames):
        console.frame(pads[n] if n < len(pads) else 0)
    return Capture(console.stream, bytes(console.palette), bytes(console.attributes), console.uploads)
