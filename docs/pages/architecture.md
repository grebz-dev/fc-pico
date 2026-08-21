@page architecture System architecture

Three processors are involved: the console's 6502, and both cores of the cartridge's
RP2350. This page covers how the work is split and how one frame flows through the
system.

## Division of labour

| | Runs | Responsibilities |
|---|---|---|
| **6502** | Console, ~1.79 MHz | Vertical-blank handler, controller reads, palette and scroll registers, sprite DMA, APU register writes, executing cartridge commands. Generates no graphics. |
| **RP2350 core 0** | Cartridge | Bus interrupt, rendering, framebuffer-to-bitplane conversion, DMA scheduling, application logic. |
| **RP2350 core 1** | Cartridge | NSF music driver on an emulated 6502, MP3 decoding. |

The 6502 is deliberately thin. It is a frame servant: drain the mailbox, apply what it
finds, hand back the controller state.

## Who drives the clock

Nothing on the cartridge has a frame timer. The console's vertical blank is the
heartbeat:

- @ref NMI ends by writing the controller byte to `$2007`.
- ::fcppu_w captures it and raises `PIO0_IRQ_0`.
- pio0_itr0() calls rp_system::jobRcvCom(), which falls through to its default case,
  latches the keys, calls rp_system::ppu_dma() and clears rp_system::frame_draw.
- Core 0's `loop()` sees `frame_draw == 0` and renders exactly one frame.

So the RP2350 runs at whatever rate the console asks for, and drops a frame rather than
tearing if it cannot keep up.

## One frame, end to end

@dot
digraph frame {
  rankdir=TB;
  node [shape=box, fontname="Helvetica", fontsize=9];
  edge [fontname="Helvetica", fontsize=8];

  subgraph cluster_fc { label="6502 (console)"; style=dashed; color=gray;
    nmi   [label="NMI: read mailbox\nfrom $2007"];
    keys  [label="write KEY_NEW\nto $2007"];
    apu   [label="replay APU pairs\nto $4000+"];
    job   [label="jobPICO:\nexecute commands"];
  }
  subgraph cluster_c0 { label="RP2350 core 0"; style=dashed; color=gray;
    irq   [label="pio0_itr0 ->\njobRcvCom()"];
    dma   [label="ppu_dma():\ncheck sync, arm DMA,\nappend mailbox"];
    draw  [label="ap.main():\ndraw into Canvas"];
    conv  [label="convVram():\nto bitplanes, swap"];
  }
  subgraph cluster_c1 { label="RP2350 core 1"; style=dashed; color=gray;
    snd   [label="jobSound():\ntick NSF driver"];
    pack  [label="setPF_APU():\npack register writes"];
  }
  ppu [label="PPU fetches ~15426 bytes\nthrough fcppu_r", shape=box, style=filled, fillcolor=gray90];

  keys -> irq   [label="PIO capture"];
  irq  -> dma;
  dma  -> snd   [label="FIFO: C1_SNDJOB"];
  snd  -> pack;
  irq  -> draw  [label="frame_draw = 0"];
  draw -> conv;
  conv -> ppu   [label="next frame"];
  dma  -> ppu   [label="DMA"];
  ppu  -> nmi   [label="mailbox on frame tail"];
  nmi  -> apu;
  nmi  -> job;
  apu  -> keys;
}
@enddot

The cycle is one frame deep: what core 0 renders now is what the PPU fetches next, and
the APU writes core 1 produces are heard one frame later.

## Build model

The firmware is a **unity build**. `tuto1_hw.ino` contains nothing but
`#include "system.h"`, and `system.h` textually includes every `.cpp` in `sys/`:

```cpp
#include "sys/rp_system.h"
#include "ap_data.h"
#include "ap_main.h"
#include "sys/ArduinoGL.cpp"
...
#include "sys/rp_system.cpp"
#include "sys/rp_core0.h"   // defines setup()  / loop()
#include "sys/rp_core1.h"   // defines setup1() / loop1()
```

Include order matters: the two core files come last because they reference the global
instances (`sys`, `ap`, `snd`) defined at the bottom of the `.cpp` files. Adding a new
`.cpp` under `sys/` requires adding it to `system.h` -- the Arduino IDE will not pick
it up on its own.

The arduino-pico core names its inter-core FIFO object `rp2040.fifo` on every supported
chip, RP2350 included. The name is historical and does not indicate the target part.

## Dual-core messaging

Core 0 talks to core 1 through the hardware FIFO, using the `C1_*` codes in
`ap_main.h`. `C1_SND_MP3PLAY` is a tagged command carrying an index in its low byte;
the others are plain values.

Core 1 idles by calling `rp_sound::jobMP3()` and only wakes for real work when a
message arrives. It also feeds the watchdog on core 0's behalf while the bus is idle --
see rp_debug.h for the handover rule.
