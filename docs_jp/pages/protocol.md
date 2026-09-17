@page protocol FC to PICO protocol

The complete wire format between the console's 6502 and the cartridge's RP2350.

## The wire

There is one physical channel: the PPU data port at `$2007`, with the PPU address
parked in pattern-table space.

```asm
SET_VRAM_ADD2 #$0800    ; park the address in CHR space
lda $2007               ; read  a byte FROM the cartridge
sta $2007               ; write a byte TO   the cartridge
```

@warning `$0800` is **not** video memory here. It is an address that causes the
cartridge's chip select to assert, which is what routes the access to the RP2350 rather
than to VRAM. Any pattern-space address would work; `$0800` is convention. Reading this
code as "writing to CHR RAM" will mislead you completely.

@warning The first read after setting the PPU address returns the stale internal read
buffer. **Every** read sequence must discard one byte first. This is not defensive
coding -- omit it and all subsequent data is off by one.

## Direction 1: cartridge to console

The cartridge never initiates. It appends a 64-byte **mailbox** to the tail of the
frame stream the PPU is already pulling, and the 6502 reads it during vertical blank
(@ref NMI). It lands in zero page at `$20`-`$5F`.

| Offset | Zero page | Contents |
|---|---|---|
| 0 | `$20` | unused -- see below |
| 1 | `$21` | #PF_MAGIC_NO (`$FC`), also spelled `PF_MAGIC_CODE` on the 6502 side -- validity sentinel |
| 2..15 | `$22`-`$2F` | command list, terminated by #PF_COM_NONE |
| 16..63 | `$30`-`$5F` | APU register writes, `(index, value)` pairs, terminated by `$FF` |

@note Byte 0 is unused but **not reserved**. It is the vestigial #PF_COM_SE sound-effect
slot: in FC PICO v1.00 `playSE()` wrote the effect number there, and the byte was left
free when the SE opcode was disabled. It is not spare space to claim -- the FC_PICO_GB
derivative has already taken bytes 0 *and* 1 for its own APU protocol, so anything built
on byte 0 is competing with a shipped project. See @ref references.

The command area is written by rp_system::setPF_COM() and rp_system::setPF_VRAM(); the
APU area by rp_system::setPF_APU(). Both refuse to overflow their region, deferring
whatever does not fit to the next frame.

### Command opcodes

| Opcode | Value | Payload | Effect |
|---|---|---|---|
| #PF_COM_NONE | `0` | — | End of list |
| #PF_COM_DMOD | `1` | — | Blank display, enter bulk data mode |
| #PF_COM_FDIN | `2` | — | Palette fade in |
| #PF_COM_FDOT | `3` | — | Palette fade out |
| #PF_COM_SE | `$80\|n` | — | Play sound effect *n* (disabled in this build) |
| #PF_COM_BGM | `$A0\|n` | — | Play music *n* (disabled in this build) |
| #PF_COM_VRAM | `$C0\|adrH` | `adrL`, `data` | Write one VRAM byte |

Opcodes with bit 7 clear are dispatched through a jump table; the rest by range. See
@ref jobPICO.

## Direction 2: console to cartridge

A single byte written to `$2007`, captured by ::fcppu_w.

| Opcode | Value | Follow-up | Effect |
|---|---|---|---|
| #FP_COM_VER | `$2F` | — | Return the 14-byte build stamp |
| #FP_COM_ROM | `$3F` | page | Return 256 bytes of the boot ROM image |
| #FP_COM_LOG | `$BF` | up to 7 bytes | Print to the cartridge's serial console |
| #FP_COM_DRQ | `$CF` | — | Request the next data-mode block |
| #FP_COM_DLD | `$DF` | page | Request one 256-byte payload page |
| #FP_COM_RST | `$EF` | — | Restart cartridge firmware |
| #FP_COM_INI | `$FF` | stage | Initialise, starting at *stage* |

@note Any value **not** in this table is a controller state byte. That is the hot path,
sent once per frame, and it is what advances the whole system. See @ref architecture.

## Frame synchronisation

The two sides share no clock. Synchronisation is inferred from how many bytes the PPU
fetched:

- ::fcppu_rna counts qualifying read strobes continuously.
- rp_system::ppu_dma() samples the count once per frame and compares it against
  #PPU_COUNT_VAL, which is `15426 + 64`.
- Within ±2, the stream is in phase: the DMA is re-armed, and the residual drift of one
  or two bytes is absorbed by manually stepping the transmit state machine.
- Outside ±2, the DMA is **stopped**. Pushing more data would tear the display; the
  cartridge waits for the console to come back into phase.

@warning This is why rendering must stay enabled during normal operation. With the PPU
idle there is no fetch stream to ride on, which is why bulk transfers are a separate,
explicitly entered mode.

## Validity checking

The only integrity mechanism is the magic byte. @ref NMI compares mailbox byte 1
against `PF_MAGIC_CODE` -- the 6502-side name for the same `$FC` the firmware calls
#PF_MAGIC_NO -- before acting on anything; a mismatch means the frame slipped and the 64
bytes just read are arbitrary pattern data, so the buffer is discarded.

There is no checksum, no sequence number and no retransmission.

## The handshake

@ref PICO_COM_WAIT, in its entirety:

```asm
PICO_COM_WAIT:
    ldx  #0
.wait
    dex
    bne  .wait
    rts
```

256 iterations, roughly 1.3 ms. No ready line, no acknowledgement, no timeout. After a
request the 6502 simply assumes the answer has arrived.

@warning This is the tightest coupling in the system. Anything that lengthens the
cartridge's response path -- a `Serial.print`, a watchdog stall, a long interrupt --
causes the 6502 to read stale bytes with no error indication. Keep
rp_system::jobRcvCom() and everything it calls fast.

## Bulk data mode

For payloads too large for the per-frame mailbox: palettes, attribute tables, whole
nametables, and the boot ROM image during self-reflash.

@dot
digraph datamode {
  rankdir=TB;
  node [shape=box, fontname="Helvetica", fontsize=9];
  edge [fontname="Helvetica", fontsize=8];
  a [label="cartridge queues PF_COM_DMOD\n(startDataMode, retries up to 100x)"];
  b [label="6502: display OFF\nenter xPF_COM_DMOD loop"];
  c [label="6502 -> FP_COM_DRQ"];
  d [label="PICO_COM_WAIT (~1.3 ms)"];
  e [label="6502 reads 6-byte header:\ncom, magic, adrL, adrH, sizeL, sizeH"];
  f [label="magic == $FC?", shape=diamond];
  g [label="PF_DAT_VRAM / PF_DAT_RAM:\nFP_COM_DLD, stage into $400,\nthen blast to $2007"];
  h [label="PF_DAT_STEP:\nset STG_COD, leave mode"];
  i [label="setErrorLog(1),\nleave mode"];
  j [label="display ON, fade in"];
  a -> b -> c -> d -> e -> f;
  f -> g [label="yes, data"];
  f -> h [label="yes, step"];
  f -> i [label="no"];
  g -> c [label="next block"];
  h -> j; i -> j;
}
@enddot

Two details worth knowing:

- Payloads are staged through `PICO_DATA_BUF` at `$400` before being written to VRAM,
  because `$2007` is both the source and the destination and cannot be streamed
  directly from one to the other.
- Transfers targeting the attribute table at `$23C0` need **one extra dummy read**. The
  original author documented this as an unexplained one-byte offset. It is preserved
  verbatim in @ref xPF_COM_DMOD; do not remove it without re-testing an attribute
  upload.

Payload priority when the cartridge answers a #FP_COM_DRQ is fixed: palette (`$3F00`,
32 bytes), then attributes (`$23C0`, 64 bytes), then a pending step change, then
#PF_COM_NONE to end the mode.

## Audio relay

The cartridge does not synthesise the music channels. It runs the NSF driver on an
emulated 6502 (rp_fcemu), traps the driver's writes to `$4000`-`$4017` via
rp_system::setAPU(), and ships them as `(index, value)` pairs in the mailbox. @ref NMI
replays them into the console's real APU, capped at 24 pairs per frame.

See @ref audio_page.

## Keeping the three copies in sync

@warning The opcode values exist in **three** places:

- `tutorial_project/tuto1_hw/sys/rp_system.h` (`PF_COM_*`, `FP_COM_*`)
- `tutorial_project/BOOTROM/SysPico.asm` (`PF_COM_*`, `FP_COM_*`)
- `tutorial_project/BOOTROM_FIX/PG_main.asm` (`FP_COM_*`, redefined locally)

Nothing checks that they agree. There is no protocol version field. Change one, change
all three, and rebuild both sides -- see the version-coupling note in
@ref build_pipeline.

## Protocol versions

There are two firmware generations in the wild and they are **wire-incompatible**:

| | v1.00 | current (`tuto1_hw`) |
|---|---|---|
| #FC_COM_BUF_SIZE | 16 | 64 |
| #PPU_COUNT_VAL | 15442 | 15490 |
| Mailbox zero page | `$40`-`$4F` | `$20`-`$5F` |
| #PF_MAGIC_NO | `$FC` | `$FC` |

@warning The magic byte is identical in both, so it **cannot** tell them apart. A v1.00
console reading a current-firmware frame finds `$FC` at the wrong offset, fails the
check on every frame, and discards all of them -- with no diagnostic, because a failed
magic check is indistinguishable from ordinary frame slip. The screen simply stops
updating.

In practice the mismatch is caught upstream of the protocol, by the `dbdate.h` build
stamp: the boot ROM and the firmware are version-coupled, and a stamp mismatch triggers
a reflash rather than a silent stall. See @ref build_pipeline and @ref boot_reflash.
That coupling is the only version check this system has.

## What is ABI and what is sample code

FC_PICO_GB (@ref references) is an independent implementation on the same cartridge,
forked from v1.00 by a different author. What it inherited unedited, and what it threw
away, is the sharpest available statement of where the boundary lies.

**Fixed surface.** Inherited byte-identical, and safe to build against:

- the PIO bus contract and pin map (`fcppu.pio`);
- the DMA plumbing (`rp_dma.*`), including the `0x6008` phase nudge and the tail memcpy;
- #PPU_COUNT_VAL frame sync, its ±2 window, and the manual state-machine step that
  absorbs the residual;
- the mailbox-on-frame-tail mechanism itself;
- `PF_COM_*`, `PF_DAT_*`, `FP_COM_*`, and the `$FC` magic;
- the `$2007` reply convention, including the mandatory dummy read;
- **joypad-as-default-case** -- any `$2007` byte that does not match an `FP_COM_*` opcode
  is controller state, and that write is the frame heartbeat that re-arms the DMA.

**Sample code.** Replaced or deleted wholesale by the derivative, and safe to discard:
everything in `ap_*`, `ArduinoGL`, `Obj3d`, `rp_fcemu`, `rp_nsfplayer`, `rp_sound`, and
the Canvas sprite/clip/zoom features.

@warning The opcode space is a deny-list, which makes it expensive to extend. Because
every unmatched byte means "controller state", **each new `FP_COM_*` opcode steals a
controller encoding.** FC_PICO_GB needed a console-to-cartridge notify and had to carve
`0x05` out of that space for it -- and had to send it exactly once, at startup, because
writes to `$2007` genuinely reach VRAM. Repeating it every frame would have corrupted a
byte of the picture each time.
