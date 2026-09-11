@page nes_doom The NES DOOM technique

FC PICO's graphics rest on a trick first demonstrated by NES DOOM. This page explains
the trick, its limitations, and which of those limitations FC PICO removes.

The primary source is Norix's write-up *NES DOOM 的な技術の解説*, which also documents
the author's own follow-up cartridge, DESTROY THEM ALL (DTA). See @ref references.

## Why the Famicom is unusual

Most consoles of its generation put only a program ROM in the cartridge. The Famicom
brings out **two** buses:

@dot
digraph buses {
  rankdir=LR;
  node [shape=box, fontname="Helvetica", fontsize=10];
  subgraph cluster_cart { label="Cartridge"; style=dashed; PRG [label="PRG ROM"]; CHR [label="CHR ROM"]; }
  subgraph cluster_con  { label="Console"; style=dashed; CPU; PPU; WRAM; VRAM; }
  CPU -> PRG  [label="CPU bus"];
  CPU -> WRAM [label="CPU bus"];
  PPU -> CHR  [label="PPU bus"];
  PPU -> VRAM [label="PPU bus"];
}
@enddot

Nearly every access the PPU makes while drawing appears on that second bus. A cartridge
sitting on it can therefore supply whatever pattern data it likes, on a
per-fetch basis, and thereby control the screen far more directly than the CPU ever
could.

## The fetch pattern

While rendering, the PPU repeats a fixed cycle for each tile of each scanline:

1. nametable byte
2. attribute byte
3. tile pattern, bitplane 0
4. tile pattern, bitplane 1

That is 170 bytes per scanline over 241 lines, about 40970 bytes per frame. Sprite
pattern fetches follow the same shape but skip steps 1 and 2. The Famicom also
pre-fetches two tiles' worth of the next line near the end of the current one, to make
scrolling seamless.

@note This number does not match the 15426 that #PPU_COUNT_VAL is built from, and should
not: 40970 counts *every* fetch the PPU performs, while the cartridge's counter increments
only on the read strobes that qualify -- the pattern fetches it is actually responsible for
answering. Nametable, attribute and sprite traffic are excluded. Two different
measurements of the same frame.

The insight is that **only the order matters, not the addresses**. Whether the screen
is scrolling or static changes which addresses the PPU emits, but the *sequence* of
fetch types is invariant. So a cartridge that ignores the address lines entirely and
simply presents the right byte at the right position in the sequence produces a
coherent picture. The nametable and attribute bytes exist only to influence which
address the PPU will emit next; if the address lines are unused, those bytes can be
anything at all.

This is exactly what FC PICO does. Its fourth PIO state machine (::fcppu_rna) counts
read strobes, and the count *is* the position in the frame.

## Consequences

**Colour resolution improves.** Ordinarily a palette is chosen per 16x16-pixel
attribute quadrant. Feeding the bitplanes directly means the pairing of bits can vary
per 8x1-pixel run, so the effective colour granularity is far finer than the hardware
normally allows -- comparable to MSX SCREEN 2.

**Compatibility narrows.** The technique depends on the exact bus behaviour of a
genuine Famicom PPU. Clone consoles frequently differ in small ways, which is the same
reason MMC5 cartridges often misbehave on them. The FC PICO manual says as much:
tested on major models, not guaranteed on all.

## What NES DOOM could not do

NES DOOM drove only the PPU bus, and used background tiles only. Two limitations
followed:

- **No sprites**, because sprite rendering requires the PPU's per-scanline sprite
  evaluation to be mirrored on the cartridge side.
- **No OAM DMA**, because the cartridge was not on the CPU bus. Moving 256 bytes of
  sprite attributes by hand costs at least 8 cycles per byte -- over 2048 cycles
  against a vertical-blank budget of roughly 2200. OAM DMA does it in about 514.

DTA solved both by adding a CPLD to bridge the console's 68-family CPU bus onto the
80-family bus its PIC32 expected, which restored OAM DMA and made sprites practical.

## What FC PICO does differently

FC PICO keeps the PPU-bus streaming idea and layers a **bidirectional command
protocol** on top of it, rather than adding a second bus:

| Concern | NES DOOM | FC PICO |
|---|---|---|
| Frame data | PPU bus | PPU bus (::fcppu_r, DMA-fed) |
| Return channel | none | 64-byte mailbox on the frame tail |
| Console -> cartridge | none | byte writes to `$2007` (::fcppu_w) |
| Attributes and palette | unused | pushed as commands, applied by the 6502 |
| Audio | host-generated | NSF driver emulated on the cartridge, register writes replayed to the real APU |
| Frame sync | — | read-strobe count vs. #PPU_COUNT_VAL |

The mailbox is what makes the difference. Because the cartridge can ask the 6502 to do
things -- write VRAM, change the palette, run a fade, program its own flash -- the
console's own hardware stays useful instead of being bypassed. Sprites remain
available through ordinary OAM DMA, since the 6502 is still running normal code.

The cost is synchronisation. Both sides must agree on where the frame boundary is, with
no shared clock and no acknowledgement. @ref protocol covers how that is maintained and
how it fails.
