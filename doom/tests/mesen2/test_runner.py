# SPDX-License-Identifier: BSD-3-Clause
"""Regression tests for the Mesen2 scenario report's trace filtering."""

from pathlib import Path
import sys

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[2] / "sim" / "mesen2"))

from run_scenario import steady_heartbeat_rows


def _row(frame, cpu_reads, render_reads, last_count):
    return {
        "ppu_frame": str(frame),
        "cpu_reads": str(cpu_reads),
        "render_reads": str(render_reads),
        "last_count": str(last_count),
    }


def test_histogram_uses_only_steady_heartbeat_rows():
    rows = [
        _row(119, 65, 16388, 16453),  # startup boundary
        _row(123, 0, 1901828, 0),  # ordinary protocol write
        _row(125, 90, 1934604, 1934694),  # transition/cumulative count
        _row(126, 65, 16388, 16453),  # real heartbeat
    ]

    assert steady_heartbeat_rows(rows) == [rows[-1]]
