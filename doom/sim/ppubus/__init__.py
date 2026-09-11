# SPDX-License-Identifier: BSD-3-Clause
"""ppubus -- L3 host model of the console side of the fcbus PPU bus.

Re-exports the public API from ``ppubus.ppubus`` (see that module's
docstring for the model itself and its UNCALIBRATED caveat).
"""

from .ppubus import PpuBus, SimpleCart

__all__ = ["PpuBus", "SimpleCart"]
