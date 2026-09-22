# SPDX-License-Identifier: BSD-3-Clause
"""Differential host test: C frame conversion against the Python video oracle."""

from __future__ import annotations

import ctypes
from pathlib import Path
import subprocess

import numpy as np
import pytest

from fcpico import protocol, stream
import fcvideo_ref


ROOT = Path(__file__).resolve().parents[3]
PRESET = {
    "backdrop": 0x0F,
    "subpalettes": ((0x00, 0x10, 0x30), (0x06, 0x16, 0x26),
                    (0x09, 0x19, 0x29), (0x02, 0x12, 0x22)),
}
U8 = ctypes.c_uint8
U8_PTR = ctypes.POINTER(U8)


class Tables(ctypes.Structure):
    _fields_ = [("err", U8_PTR), ("lut", U8_PTR), ("palette", U8 * protocol.MBX_PAL_LEN)]


@pytest.fixture(scope="module")
def library(tmp_path_factory):
    output = tmp_path_factory.mktemp("fcvideo") / "libfcvideo.so"
    subprocess.run(
        ["cc", "-shared", "-fPIC", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         "-I", str(ROOT / "doom/port/video"), "-I", str(ROOT / "doom/fcbus"),
         str(ROOT / "doom/port/video/fcvideo.c"), "-o", str(output)],
        check=True,
    )
    lib = ctypes.CDLL(str(output))
    lib.fcvideo_sizeof.restype = ctypes.c_size_t
    lib.fcvideo_init.argtypes = [ctypes.c_void_p, ctypes.POINTER(Tables)]
    lib.fcvideo_convert.argtypes = [ctypes.c_void_p, U8_PTR, U8_PTR, U8_PTR, ctypes.c_bool]
    lib.fcvideo_set_palette.argtypes = [ctypes.c_void_p, U8_PTR]
    return lib


def _reference(frame, err, lut, palette, previous=None):
    reduced = fcvideo_ref.decimate_320_to_256(frame)
    image = fcvideo_ref.place_letterbox(reduced, backdrop=0)
    attr = fcvideo_ref.choose_block_palettes(image, err, prev_attr=previous)
    pixels = fcvideo_ref.quantize(image, lut, attr)
    mailbox = bytearray(protocol.FC_COM_BUF_SIZE_V2)
    mailbox[protocol.MBX_FLAGS] = (
        protocol.MBX_FLAG_V2 | protocol.MBX_FLAG_ATTR_VALID | protocol.MBX_FLAG_PAL_VALID
    )
    mailbox[protocol.MBX_MAGIC] = protocol.PF_MAGIC_NO
    mailbox[protocol.MBX_PAL:protocol.MBX_PAL + protocol.MBX_PAL_LEN] = palette
    mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR + protocol.MBX_ATTR_LEN] = attr
    return bytes(stream.encode_frame(pixels, mailbox)), bytes(attr)


def _convert(lib, video, frame, reset):
    encoded = np.zeros(protocol.VRAM_BUF_BYTES_V2, dtype=np.uint8)
    attr = np.zeros(protocol.MBX_ATTR_LEN, dtype=np.uint8)
    source = np.ascontiguousarray(frame, dtype=np.uint8)
    lib.fcvideo_convert(video, source.ctypes.data_as(U8_PTR),
                        encoded.ctypes.data_as(U8_PTR), attr.ctypes.data_as(U8_PTR), reset)
    return encoded.tobytes(), attr.tobytes()


def test_c_stream_matches_python_reference_for_distinct_frames(library):
    grey = np.arange(256, dtype=np.uint8)
    playpal = np.column_stack((grey, grey, grey))
    err, lut = fcvideo_ref.build_err_and_lut(
        playpal, PRESET["subpalettes"], PRESET["backdrop"]
    )
    err = np.ascontiguousarray(err)
    lut = np.ascontiguousarray(lut)
    palette = bytes([PRESET["backdrop"], *PRESET["subpalettes"][0],
                     PRESET["backdrop"], *PRESET["subpalettes"][1],
                     PRESET["backdrop"], *PRESET["subpalettes"][2],
                     PRESET["backdrop"], *PRESET["subpalettes"][3]])
    tables = Tables(err.ctypes.data_as(U8_PTR), lut.ctypes.data_as(U8_PTR),
                    (U8 * protocol.MBX_PAL_LEN).from_buffer_copy(palette))
    storage = (ctypes.c_uint64 * ((library.fcvideo_sizeof() + 7) // 8))()
    video = ctypes.cast(storage, ctypes.c_void_p)
    library.fcvideo_init(video, ctypes.byref(tables))

    x = np.arange(320, dtype=np.uint16)
    gradient = np.tile((x * 255 // 319).astype(np.uint8), (200, 1))
    checker = np.indices((200, 320)).sum(axis=0).astype(np.uint8) * 37
    random = np.random.default_rng(7).integers(0, 256, size=(200, 320), dtype=np.uint8)
    for frame in (gradient, checker, random):
        expected_stream, expected_attr = _reference(frame, err, lut, palette)
        actual_stream, actual_attr = _convert(library, video, frame, True)
        assert actual_attr == expected_attr
        assert actual_stream == expected_stream

    expected_stream, expected_attr = _reference(random, err, lut, palette, expected_attr)
    actual_stream, actual_attr = _convert(library, video, random, False)
    assert actual_attr == expected_attr
    assert actual_stream == expected_stream

    flashed = bytes([0x0f, 0x30, 0x30, 0x30] * 4)
    palette_c = (U8 * protocol.MBX_PAL_LEN).from_buffer_copy(flashed)
    library.fcvideo_set_palette(video, palette_c)
    expected_stream, expected_attr = _reference(random, err, lut, flashed, expected_attr)
    actual_stream, actual_attr = _convert(library, video, random, False)
    assert actual_attr == expected_attr
    assert actual_stream == expected_stream
