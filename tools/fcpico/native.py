# SPDX-License-Identifier: BSD-3-Clause
"""Thin ctypes bindings to the native bus core and cartridge model.

The firmware's stream encoding and the cartridge behaviour come from the same C
code the cartridge model runs, through ``libfcpico_native`` (built with the
platform CMake project). Python never re-implements them.

    from fcpico import native
    lib = native.load()                      # FCPICO_NATIVE_LIB or build/platform
    words = lib.encode_frame(pixels)         # 240 rows x 256 pixel values 0..3
    cart = lib.cart("pattern", "0", prg)     # a simulated cartridge
"""
from __future__ import annotations

import ctypes
import os
from dataclasses import fields
from pathlib import Path

from . import layout as layout_module

ROOT = Path(__file__).resolve().parents[2]
CART_ABI_VERSION = 1
DEFAULT_LIBRARY = ROOT / "build" / "platform" / "libfcpico_native.so"


class NativeError(RuntimeError):
    """The native library is missing or does not match these bindings."""


class _Layout(ctypes.Structure):
    _fields_ = [("version", ctypes.c_uint32)] + [
        (f.name, ctypes.c_uint32) for f in fields(layout_module.Layout) if f.name != "version"]


class Stats(ctypes.Structure):
    """fcbus_stats_t."""
    _fields_ = [("frames", ctypes.c_uint32), ("dma_stops", ctypes.c_uint32),
                ("resyncs", ctypes.c_uint32), ("last_count", ctypes.c_uint32),
                ("count_hist", ctypes.c_uint16 * 9), ("init_events", ctypes.c_uint32),
                ("last_init_stage", ctypes.c_uint8)]


class _Provider(ctypes.Structure):
    pass


class _CartConfig(ctypes.Structure):
    _fields_ = [("abi_version", ctypes.c_uint32), ("provider", ctypes.POINTER(_Provider)),
                ("provider_options", ctypes.c_char_p), ("served_prg", ctypes.c_char_p),
                ("served_prg_len", ctypes.c_size_t)]


class Cart:
    """One simulated cartridge (fcpico_cart_t); close() or use as a context manager."""

    def __init__(self, lib: "Native", handle: int):
        self._lib, self._handle = lib, handle

    def read(self) -> int:
        """One CS1-qualified PPU read (0xFF while the cartridge drives nothing)."""
        return self._lib._c.fcpico_cart_ppu_read(self._handle)

    def read_bytes(self, count: int) -> bytes:
        read, handle = self._lib._c.fcpico_cart_ppu_read, self._handle
        return bytes(read(handle) for _ in range(count))

    def write(self, value: int) -> None:
        """One console write to $2007."""
        self._lib._c.fcpico_cart_ppu_write(self._handle, value)

    @property
    def stats(self) -> Stats:
        return self._lib._c.fcpico_cart_stats(self._handle).contents

    def close(self) -> None:
        if self._handle:
            self._lib._c.fcpico_cart_destroy(self._handle)
            self._handle = None

    def __enter__(self) -> "Cart":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class Native:
    def __init__(self, path: Path):
        try:
            c = ctypes.CDLL(str(path))
        except OSError as error:
            raise NativeError(f"cannot load {path}: {error}; build it with "
                              "cmake -S platform -B build/platform && cmake --build build/platform") from error
        c.fcpico_cart_abi_version.restype = ctypes.c_uint32
        c.fcbus_layout.restype = ctypes.POINTER(_Layout)
        c.fcbus_stream_put_line.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
        c.fcbus_stream_get_line.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
        c.fcpico_cart_find_provider.restype = ctypes.POINTER(_Provider)
        c.fcpico_cart_find_provider.argtypes = [ctypes.c_char_p]
        c.fcpico_cart_create.restype = ctypes.c_void_p
        c.fcpico_cart_create.argtypes = [ctypes.POINTER(_CartConfig), ctypes.c_char_p, ctypes.c_size_t]
        c.fcpico_cart_destroy.argtypes = [ctypes.c_void_p]
        c.fcpico_cart_ppu_read.restype = ctypes.c_uint8
        c.fcpico_cart_ppu_read.argtypes = [ctypes.c_void_p]
        c.fcpico_cart_ppu_write.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
        c.fcpico_cart_stats.restype = ctypes.POINTER(Stats)
        c.fcpico_cart_stats.argtypes = [ctypes.c_void_p]
        if c.fcpico_cart_abi_version() != CART_ABI_VERSION:
            raise NativeError(f"{path}: cartridge ABI {c.fcpico_cart_abi_version()}, "
                              f"these bindings need {CART_ABI_VERSION}")
        self._c = c
        native = c.fcbus_layout().contents
        document = {"layout_version": native.version,
                    "layout": {name: getattr(native, name) for name, _ in _Layout._fields_[1:]}}
        self.layout = layout_module.from_document(document)  # rejects unknown versions

    def encode_frame(self, rows) -> bytes:
        """Encodes 240 rows of 256 pixels (0..3) into a full stream buffer."""
        buffer = ctypes.create_string_buffer(self.layout.buffer_bytes)
        for line, row in enumerate(rows):
            self._c.fcbus_stream_put_line(buffer, line, bytes(row))
        return buffer.raw

    def decode_frame(self, stream: bytes) -> list[bytes]:
        """The 240 rows of 256 pixels (0..3) held in a stream buffer."""
        if len(stream) < self.layout.mailbox_offset:
            raise ValueError("stream is shorter than the picture")
        buffer = ctypes.create_string_buffer(bytes(stream).ljust(self.layout.buffer_bytes, b"\0"))
        row = ctypes.create_string_buffer(256)
        lines = []
        for line in range(self.layout.lines):
            self._c.fcbus_stream_get_line(buffer, line, row)
            lines.append(row.raw)
        return lines

    def cart(self, provider: str, options: str | None, prg: bytes) -> Cart:
        handle = self._c.fcpico_cart_find_provider(provider.encode())
        if not handle:
            raise NativeError(f"unknown provider {provider!r}")
        config = _CartConfig(CART_ABI_VERSION, handle, options.encode() if options else None,
                             bytes(prg), len(prg))
        error = ctypes.create_string_buffer(256)
        cart = self._c.fcpico_cart_create(ctypes.byref(config), error, len(error))
        if not cart:
            raise NativeError(error.value.decode())
        return Cart(self, cart)


def load(path: Path | None = None) -> Native:
    """Loads the library from `path`, $FCPICO_NATIVE_LIB or build/platform."""
    return Native(Path(path or os.environ.get("FCPICO_NATIVE_LIB") or DEFAULT_LIBRARY))
