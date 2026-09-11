# SPDX-License-Identifier: BSD-3-Clause
"""Tests for the L3 PPU-bus model (doom/sim/ppubus/ppubus.py).

Issue I-01. The model is the only thing standing between the plan's bus arithmetic
and a hardware session, so these tests exist to pin its behaviour down: a later
calibration against a real strobe trace should change numbers in one place and have
these tests say precisely what moved.

Two things these tests deliberately do NOT do:

- They never retype a wire constant. Everything comes from `fcpico.protocol`, which is
  generated from `doom/fcbus/fcbus_protocol.h` by `doom/tools/gen_protocol.py`. A test
  that hardcodes 15490 would keep passing after someone changed the header, which is the
  opposite of useful.
- They do not assert that the model is *right*. It is explicitly UNCALIBRATED, and plan
  01 documents an unresolved contradiction in the underlying arithmetic. What is asserted
  is self-consistency (encode/decode round trips, counts that add up, faults that change
  exactly one thing) and that the contradiction stays visible as two independent
  parameters rather than being quietly averaged away. See `test_the_open_contradiction`.

conftest.py puts doom/tools on sys.path; doom/sim is added here because the model lives
outside both the tools tree and the tests tree, and `tests/conftest.py` is shared with
other suites and owned by nobody in particular.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

_SIM_DIR = Path(__file__).resolve().parents[2] / "sim"
if str(_SIM_DIR) not in sys.path:
    sys.path.insert(0, str(_SIM_DIR))

from fcpico import protocol, stream  # noqa: E402
from ppubus import PpuBus, SimpleCart  # noqa: E402


V1 = protocol.FC_COM_BUF_SIZE_V1  # 64
V2 = protocol.FC_COM_BUF_SIZE_V2  # 128


# ---------------------------------------------------------------------------
# The totals: what the whole module is calibrated to reproduce
# ---------------------------------------------------------------------------


def test_default_frame_read_count_is_the_firmware_v1_total():
    assert PpuBus().frame_read_count() == protocol.PPU_COUNT_VAL_V1


def test_v2_mailbox_gives_the_firmware_v2_total():
    assert PpuBus(mailbox_len=V2).frame_read_count() == protocol.PPU_COUNT_VAL_V2


def test_the_two_totals_differ_by_exactly_the_mailbox_growth():
    """The only thing v2 changes about the frame is a longer mailbox."""
    assert protocol.PPU_COUNT_VAL_V2 - protocol.PPU_COUNT_VAL_V1 == V2 - V1
    assert PpuBus(mailbox_len=V2).frame_read_count() - PpuBus().frame_read_count() == V2 - V1


def test_frame_read_count_is_the_sum_of_its_documented_parts():
    """Restates the module docstring's arithmetic so a changed default is caught here."""
    bus = PpuBus()
    assert bus.frame_read_count() == (
        bus.osr_prelude_bytes
        + bus.prerender_reads
        + bus.lines * bus.reads_per_line
        + 1  # the NMI's mandatory dummy read
        + bus.mailbox_len
    )
    assert bus.lines == protocol.VRAM_LINES


def test_mailbox_len_must_be_a_real_protocol_length():
    for bad in (0, 16, 63, 65, 127, 256):
        with pytest.raises(ValueError, match="mailbox_len"):
            PpuBus(mailbox_len=bad)


@pytest.mark.parametrize("bad", [0, -2, 1, 63, 65])
def test_reads_per_line_must_be_positive_and_even(bad):
    """Odd is refused because two bytes make one 16-bit stream word."""
    with pytest.raises(ValueError, match="reads_per_line"):
        PpuBus(reads_per_line=bad)


# ---------------------------------------------------------------------------
# The open contradiction (issue I-01 step 3, plan 01 "An unresolved discrepancy")
# ---------------------------------------------------------------------------


def test_the_open_contradiction_stays_visible_as_two_parameters():
    """Plan 01: the model must keep "bytes consumed per line" and "reads counted per
    line" as independent parameters rather than deriving one from the other.

    The contradiction in one line: the firmware's counted picture reads factor as
    241 x 64 + 2, while `convVram()` lays the buffer out with a 34-word (68-byte)
    stride, and a stream laid out at 68 bytes per line cannot be consumed at 64
    without shearing the picture. The tutorial demonstrably does not shear. So the
    model gets to express either reading, and neither default may silently decide it.
    """
    bus = PpuBus()
    assert bus.reads_per_line * (protocol.VRAM_LINES + 1) + 2 == protocol.PPU_PICTURE_COUNT
    # The other half of the contradiction, from the buffer layout rather than the counter.
    assert protocol.VRAM_LINE_WORDS * 2 == 68
    naive = (protocol.VRAM_LINES + 1) * protocol.VRAM_LINE_WORDS * 2
    assert naive == 16388
    assert naive - protocol.PPU_PICTURE_COUNT == 962 == (protocol.VRAM_LINES + 1) * 4 - 2

    # Independence: changing the consumption rate must not move the counted total.
    counted = bus.frame_read_count()
    stride = PpuBus(bytes_per_line=protocol.VRAM_LINE_WORDS * 2)
    assert stride.frame_read_count() == counted == protocol.PPU_COUNT_VAL_V1
    assert stride.frame_byte_count() > stride.frame_read_count()
    assert stride.uncounted_bytes_per_frame() == protocol.VRAM_LINES * 4 == 960


def test_the_two_rates_are_equal_at_the_defaults():
    """The self-consistent reading is the default: every byte served is a byte counted."""
    bus = PpuBus()
    assert bus.bytes_per_line == bus.reads_per_line
    assert bus.frame_byte_count() == bus.frame_read_count()
    assert bus.uncounted_bytes_per_frame() == 0


def test_bytes_per_line_may_not_be_smaller_than_reads_per_line():
    """The transmit machine cannot answer fewer strobes than the counter saw."""
    with pytest.raises(ValueError, match="bytes_per_line"):
        PpuBus(reads_per_line=64, bytes_per_line=62)


@pytest.mark.parametrize("bad", [0, -4, 67])
def test_bytes_per_line_must_be_positive_and_even(bad):
    with pytest.raises(ValueError, match="bytes_per_line"):
        PpuBus(bytes_per_line=bad)


# ---------------------------------------------------------------------------
# frame_events: order and quantities
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("mailbox_len", [V1, V2])
def test_frame_events_order_and_quantities(mailbox_len):
    bus = PpuBus(mailbox_len=mailbox_len)
    events = list(bus.frame_events())
    assert len(events) == bus.frame_byte_count()

    prelude = [e for e in events if e[0] == "prelude"]
    reads = [e for e in events if e[0] == "read"]
    nmi = [e for e in events if e[0] == "nmi_read"]
    assert len(prelude) + len(reads) + len(nmi) == len(events)

    # Order: the whole prelude, then every picture read, then the whole NMI sequence.
    kinds = [e[0] for e in events]
    assert kinds == ["prelude"] * len(prelude) + ["read"] * len(reads) + ["nmi_read"] * len(nmi)

    assert [e[1] for e in prelude] == list(range(bus.osr_prelude_bytes))
    assert len(nmi) == 1 + mailbox_len
    assert [e[1] for e in nmi] == list(range(1 + mailbox_len))

    prerender = [e for e in reads if e[1] == -1]
    assert len(prerender) == bus.prerender_reads
    visible = [e for e in reads if e[1] != -1]
    assert len(visible) == protocol.VRAM_LINES * bus.bytes_per_line
    # The pre-render line comes before every visible line, and lines ascend.
    assert [e[1] for e in reads] == [-1] * bus.prerender_reads + [
        line for line in range(protocol.VRAM_LINES) for _ in range(bus.bytes_per_line)
    ]
    assert [e[2] for e in visible[: bus.bytes_per_line]] == list(range(bus.bytes_per_line))


def test_frame_events_lengthens_with_bytes_per_line_but_the_count_does_not():
    tight = PpuBus()
    loose = PpuBus(bytes_per_line=68)
    assert len(list(loose.frame_events())) - len(list(tight.frame_events())) == 960
    assert loose.frame_read_count() == tight.frame_read_count()


# ---------------------------------------------------------------------------
# The prelude is zeros, and it is counted
# ---------------------------------------------------------------------------


def test_the_prelude_is_four_zero_bytes_and_they_count():
    """Plan 01, "A four-byte prelude from the PIO itself": `fcppu_r` starts with
    `mov osr, null` and `pio_sm_restart()` refills the OSR with zeros, so the first
    four `out pins, 8` after every re-arm emit 0x00 before the first autopull. Those
    bytes go out on the bus, so `fcppu_rna` counts them like any other read."""
    bus = PpuBus()
    assert bus.osr_prelude_bytes == 4
    cart = SimpleCart(bytes(range(1, 256)) * 80)
    got = bus.run_frame(cart)
    assert got[: bus.osr_prelude_bytes] == b"\x00" * bus.osr_prelude_bytes
    assert got[bus.osr_prelude_bytes] != 0  # the first real buffer byte follows immediately
    # Counted: the total includes them, and dropping them would break the total.
    assert bus.frame_read_count() == protocol.PPU_COUNT_VAL_V1
    assert PpuBus(osr_prelude_bytes=0).frame_read_count() == protocol.PPU_COUNT_VAL_V1 - 4


def test_a_write_restarts_the_prelude():
    """`pio_sm_restart()` runs on every DMA re-arm, so every frame begins with zeros."""
    bus = PpuBus()
    cart = SimpleCart(bytes(range(1, 256)) * 80)
    first = bus.run_frame(cart)
    second = bus.run_frame(cart)
    assert second[:4] == b"\x00\x00\x00\x00"
    assert second == first  # the same buffer, re-served from the top


# ---------------------------------------------------------------------------
# Round trip: reference_buffer -> SimpleCart -> run_frame -> reconstruct
# ---------------------------------------------------------------------------


def _demo_pix(bus, seed=1234):
    rng = np.random.default_rng(seed)
    width = bus.visible_bytes_per_line * 4
    return rng.integers(0, 4, size=(protocol.VRAM_LINES, width), dtype=np.uint8)


def _demo_mailbox(bus):
    return bytes((i * 7 + 3) & 0xFF for i in range(bus.mailbox_len))


@pytest.mark.parametrize("mailbox_len", [V1, V2])
def test_round_trip_recovers_pixels_and_mailbox(mailbox_len):
    bus = PpuBus(mailbox_len=mailbox_len)
    pix = _demo_pix(bus)
    mailbox = _demo_mailbox(bus)
    cart = SimpleCart(bus.reference_buffer(pix, mailbox))
    got = bus.run_frame(cart)
    assert len(got) == bus.frame_byte_count()
    out_pix, out_mailbox = bus.reconstruct(got)
    assert np.array_equal(out_pix, pix)
    assert out_mailbox == mailbox


def test_round_trip_survives_a_wider_consumption_rate():
    """With bytes_per_line > reads_per_line the model steps over the served-but-unseen
    bytes, so the picture must come back unsheared -- which is the whole point of
    keeping the two rates separate."""
    bus = PpuBus(bytes_per_line=68)
    pix = _demo_pix(bus)
    mailbox = _demo_mailbox(bus)
    cart = SimpleCart(bus.reference_buffer(pix, mailbox))
    out_pix, out_mailbox = bus.reconstruct(bus.run_frame(cart))
    assert out_pix.shape == (protocol.VRAM_LINES, 256)
    assert np.array_equal(out_pix, pix)
    assert out_mailbox == mailbox


def test_reconstruct_uses_the_same_bit_convention_as_the_firmware():
    """Pixel 0 is bit 7; the low byte is bitplane 0 and the high byte bitplane 1.
    Asserted against `fcpico.stream`, which is tested separately against `convVram()`."""
    bus = PpuBus()
    pix = np.zeros((protocol.VRAM_LINES, 256), dtype=np.uint8)
    pix[0, 0] = 1  # plane 0 only
    pix[0, 1] = 2  # plane 1 only
    pix[0, 2] = 3  # both
    cart = SimpleCart(bus.reference_buffer(pix, _demo_mailbox(bus)))
    got = bus.run_frame(cart)
    first = bus.osr_prelude_bytes + bus.prerender_reads
    lo, hi = got[first], got[first + 1]
    assert lo == 0b1010_0000  # pixels 0 and 2 set in plane 0
    assert hi == 0b0110_0000  # pixels 1 and 2 set in plane 1
    assert stream.unpack_run(lo | (hi << 8))[:3] == [1, 2, 3]


def test_reconstruct_refuses_a_frame_of_the_wrong_length():
    bus = PpuBus()
    full = bus.reference_buffer(_demo_pix(bus), _demo_mailbox(bus))
    served = bus.run_frame(SimpleCart(full))
    with pytest.raises(ValueError, match="fault-free frame"):
        bus.reconstruct(served[:-1])
    with pytest.raises(ValueError, match="fault-free frame"):
        bus.reconstruct(served + b"\x00")


def test_reference_buffer_validates_its_arguments():
    bus = PpuBus()
    good_pix, good_mbx = _demo_pix(bus), _demo_mailbox(bus)
    with pytest.raises(ValueError, match="pix must have shape"):
        bus.reference_buffer(good_pix[:, :128], good_mbx)
    with pytest.raises(ValueError, match="mailbox must be"):
        bus.reference_buffer(good_pix, good_mbx[:-1])


def test_reference_buffer_is_not_the_firmware_buffer_layout():
    """The module docstring is explicit that this is a different layout from
    `fcpico.stream.encode_frame()`, whose offsets are real C pointer offsets. If they
    ever do coincide, that is a change worth noticing rather than assuming."""
    bus = PpuBus()
    pix = _demo_pix(bus)
    mailbox = _demo_mailbox(bus)
    mine = bus.reference_buffer(pix, mailbox)
    theirs = bytes(stream.encode_frame(pix, mailbox))
    assert mine != theirs
    # The firmware's mailbox offset is unambiguous and unrelated to this model's counts.
    assert theirs[protocol.VRAM_MAILBOX_OFF_V1 : protocol.VRAM_MAILBOX_OFF_V1 + len(mailbox)] == mailbox


# ---------------------------------------------------------------------------
# The heartbeat
# ---------------------------------------------------------------------------


def test_v1_heartbeat_is_one_raw_byte():
    bus = PpuBus()
    writes = []
    cart = _RecordingCart(bus, writes)
    bus.run_frame(cart, heartbeat=0x1234)
    assert writes == [0x34]


def test_v2_heartbeat_is_a_three_byte_key_packet():
    bus = PpuBus(mailbox_len=V2)
    writes = []
    cart = _RecordingCart(bus, writes)
    bus.run_frame(cart, heartbeat=0x1234)
    assert writes == [protocol.FP_COM_KEY, 0x12, 0x34]


def test_hold_heartbeat_skips_the_write_but_not_the_reads():
    bus = PpuBus()
    writes = []
    cart = _RecordingCart(bus, writes)
    got = bus.run_frame(cart, heartbeat=0xFF, hold_heartbeat=True)
    assert writes == []
    assert len(got) == bus.frame_byte_count()


class _RecordingCart:
    """A cart that serves zeros and records every heartbeat byte written to it."""

    def __init__(self, bus, writes):
        self._writes = writes

    def ppu_read(self) -> int:
        return 0

    def ppu_write(self, value: int) -> None:
        self._writes.append(value)


# ---------------------------------------------------------------------------
# Fault injection: a slip must change the count by exactly the slip
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("n", [1, 2, 3, 64])
def test_a_dropped_read_shortens_the_frame_by_exactly_that_much(n):
    bus = PpuBus()
    cart = SimpleCart(bus.reference_buffer(_demo_pix(bus), _demo_mailbox(bus)))
    got = bus.run_frame(cart, drop=n)
    assert len(got) == bus.frame_byte_count() - n


@pytest.mark.parametrize("n", [1, 2, 3, 64])
def test_a_duplicated_read_lengthens_the_frame_by_exactly_that_much(n):
    bus = PpuBus()
    cart = SimpleCart(bus.reference_buffer(_demo_pix(bus), _demo_mailbox(bus)))
    got = bus.run_frame(cart, duplicate=n)
    assert len(got) == bus.frame_byte_count() + n


def test_a_slip_changes_the_count_and_nothing_else():
    """A dropped strobe must leave every byte before the slip untouched: the model
    removes events off the end, so the picture is intact and only the mailbox tail is
    short. That is what lets a test tell "the link slipped" from "the picture is wrong"."""
    bus = PpuBus()
    buf = bus.reference_buffer(_demo_pix(bus), _demo_mailbox(bus))
    clean = bus.run_frame(SimpleCart(buf))
    short = bus.run_frame(SimpleCart(buf), drop=3)
    assert short == clean[:-3]
    long = bus.run_frame(SimpleCart(buf), duplicate=3)
    assert long[: len(clean)] == clean


def test_dropping_more_than_a_frame_is_refused():
    bus = PpuBus()
    cart = SimpleCart(b"\x00" * 16)
    with pytest.raises(ValueError, match="exceeds this frame"):
        bus.run_frame(cart, drop=bus.frame_byte_count() + 1)


def test_a_slipped_frame_is_followed_by_a_clean_one():
    """Recovery: the heartbeat write restarts the cart, so the frame after a slip is
    byte-identical to a frame with no slip at all. In the firmware this is `ppu_dma()`
    calling `pio_sm_restart()`; here it is `SimpleCart.ppu_write()` rewinding."""
    bus = PpuBus()
    cart = SimpleCart(bus.reference_buffer(_demo_pix(bus), _demo_mailbox(bus)))
    frames = bus.run_frames(cart, 3, faults={1: {"drop": 5}})
    assert len(frames[0]) == bus.frame_byte_count()
    assert len(frames[1]) == bus.frame_byte_count() - 5
    assert len(frames[2]) == bus.frame_byte_count()
    assert frames[2] == frames[0]
    bus.reconstruct(frames[2])  # and it decodes, which frames[1] would not


def test_run_frames_applies_faults_only_to_the_named_frame():
    bus = PpuBus()
    cart = SimpleCart(b"\x5A" * 1024)
    frames = bus.run_frames(cart, 4, faults={0: {"duplicate": 2}, 3: {"drop": 1}})
    sizes = [len(f) for f in frames]
    n = bus.frame_byte_count()
    assert sizes == [n + 2, n, n, n - 1]


# ---------------------------------------------------------------------------
# SimpleCart
# ---------------------------------------------------------------------------


def test_simple_cart_wraps_when_asked_for_more_than_it_holds():
    cart = SimpleCart(b"\x01\x02\x03", osr_prelude_bytes=2)
    assert [cart.ppu_read() for _ in range(8)] == [0, 0, 1, 2, 3, 1, 2, 3]


def test_simple_cart_refuses_an_empty_buffer():
    with pytest.raises(ValueError, match="must not be empty"):
        SimpleCart(b"")
