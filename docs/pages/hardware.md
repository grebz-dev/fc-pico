@page hardware Hardware and pinout

## The cartridge

A Famicom cartridge carrying a Raspberry Pi Pico 2-class microcontroller (RP2350). The
official manual describes it as "a microcontroller equivalent to Raspberry Pi PICO2".

Nothing in this codebase is chip-specific: the PIO programs, DMA usage and multicore
calls are common to RP2040 and RP2350. The arduino-pico core exposes its inter-core
FIFO as `rp2040.fifo` regardless of the target part, so that name is not evidence of
which chip is fitted.

The cartridge board is an **NROM-256** layout: mapper 0, 32 KB PRG, **no CHR-ROM**. The
absence of CHR-ROM is the point -- the microcontroller occupies that role.

## Pin assignment

Declared in fcppu.pio so the PIO programs and the C++ cannot disagree.

| Signal | GPIO | Direction | Notes |
|---|---|---|---|
| D0..D7 | GP6..GP13 | bidirectional | Contiguous; #PI_D0_BIT is the shift base |
| CS1 | GP17 | input | PPU chip select for pattern space; the `jmp PIN` predicate of every PIO program |
| PPU `/RD` | GP20 | input | Read strobe, active low |
| PPU `/WR` | GP21 | input | Write strobe, active low |
| User key | GP24 | input | On-cartridge button (#USRKEY_PIN) |
| Status LED | GP25 | output | #LED_PIN |
| PWM audio | GP28 | output | MP3 playback path only |

Three signals are wired but unused, and their definitions are commented out in
fcppu.pio: `PA12` (GP18), `PA13` (GP19) and `OD_DIR` (GP16). They are available if a
future design needs partial address decoding.

@note The PPU **address** lines are not connected at all. Position within the frame is
derived by counting read strobes. @see @ref nes_doom

## Bus turnaround

The data bus is shared, so the cartridge may drive it only during a read. Two state
machines cooperate:

- ::fcppu_r raises an internal PIO IRQ (#IRQ_DIR is `7`) as it begins shifting data out.
- ::fcppu_dir sees the flag, sets all eight `pindirs` to output, clears the flag, then
  waits for `/RD` to rise before releasing the bus.

@warning Never leave the data pins configured as outputs outside that window. A
cartridge that drives a shared bus continuously will fight the console.

## Voltage

The Famicom bus is 5 V; the RP2350 is a 3.3 V part. Level handling is done on the
cartridge board, not in software. NES DOOM used an FX2LP for its 5 V-tolerant pins and
DTA used a 74AHCT541 buffer; FC PICO's arrangement is not described in the distributed
sources.

## Console compatibility

The technique depends on the precise bus behaviour of a genuine Famicom PPU. Clone
consoles often differ slightly -- the same reason MMC5 cartridges are unreliable on
them.

The manual's guidance, if the cartridge does not start:

1. Press reset and see whether it starts then.
2. Reseat the cartridge.
3. Try a genuine console or a different clone.
4. If nothing works, contact impact soft (see @ref references).

## Updating the firmware

The RP2350 is reachable over USB-C. Hold **BOOT** while connecting and it enumerates as
a mass-storage device; copying a UF2 onto that drive updates the firmware. No
programmer is required, and the cartridge need not be opened.

The 6502 boot ROM is a separate matter -- see @ref flashing.
