# 01 -- Constraints and budgets

Numbers the design has to respect. Each row says where it comes from. Rows marked
**(inferred)** or **(empirical)** must be confirmed by the Phase 0 hardware session
(see 10-workplan, P0-T9) and this document updated with the measured value.

## The cartridge

| Item | Value | Source |
|------|-------|--------|
| MCU | RP2350 (Pico 2 class): 2x Cortex-M33, 150 MHz default, 520 KB SRAM in 10 banks, 3 PIO blocks (12 state machines), 16 DMA channels, 2x 4 KB scratch SRAM | FC PICO manual via `docs/pages/hardware.md`; RP2350 datasheet |
| Flash | **4 MB QSPI (inferred)** -- FC PICO GB, an independent project on the same board, builds with the Arduino "4MB (Sketch: 3.5MB, FS: 512KB)" setting. Confirm with `picotool info -a` and `flash_get_size()` at first boot. | axsann/FC_PICO_GB README |
| Overclock precedent | 276 MHz, `-O2`, on this exact board (FC PICO GB); 270 MHz with `vreg 1.30 V` and QMI clkdiv 3 on Pico 2 (RP2040 Doom `i_main.c`) | those projects |
| USB | USB-C; BOOT button for UF2 drag-and-drop; USB CDC serial for logs | `docs/pages/flashing.md` |
| Bus pins | D0..D7 = GP6..GP13, CS1 = GP17, PPU /RD = GP20, PPU /WR = GP21; PA12 (GP18), PA13 (GP19), OD_DIR (GP16) wired but unused; user key GP24, LED GP25, PWM audio GP28 | `fcppu.pio`, `docs/pages/hardware.md` |
| Console-side ROM | NROM-256 board, 32 KB PRG in an AM29F040B-class flash, no CHR-ROM; the RP2350 is the CHR device | `docs/pages/hardware.md`, `flashing.md` |
| Level shifting | On board, undocumented | `hardware.md` |

## The console

| Item | Value | Notes |
|------|-------|-------|
| CPU | 6502 at 1.789773 MHz (NTSC) | 1.662607 MHz on PAL |
| Frame | 262 lines x 341 dots, 60.0988 Hz | 312 lines, 50.007 Hz on PAL |
| Vertical blank | Lines 241-260: 20 lines = **2273 CPU cycles** (NTSC) | 70 lines (~7459 cycles) on PAL |
| NMI entry + `rti` | ~13 cycles | |
| `$2007` read/write | 4 cycles each (`lda abs` / `sta abs`) | plus 3 for the paired zero-page store/load |
| Sprite DMA (`$4014`) | 513-514 cycles | **not needed by Doom** -- reclaimed |
| Tutorial NMI, measured with py65 (`tests/bootrom/`) | 577 cycles vblank-critical (dummy + 64 mailbox reads, reply, PPU registers); 1149 total with sprite DMA and no APU pairs; **1724 total worst case** (24 pairs + sprite DMA); APU replay 23.7 cycles per pair | `tests/bootrom/tutorial_nmi_cycles.json`; entry sequence (7) included; DMA stall modelled as +513 |
| Controller read (4x majority vote, fix bank `BR_KEY_RTN`) | ~650 cycles | runs in the main loop, not the NMI |
| Handshake spin (`PICO_COM_WAIT`) | 256 x (`dex` 2 + `bne` 3) = 1280 cycles = **0.72 ms** | `docs/pages/protocol.md` says "roughly 1.3 ms"; the arithmetic says 0.72 ms. Treat 0.7 ms as the budget for any cartridge response. |
| PPU bus timing | Each PPU memory access spans 2 dots (~372 ns); /RD is asserted for roughly one dot (~186 ns). The cartridge must turn the bus around and present data inside that window. | nesdev wiki "PPU rendering" (verify; site was unreachable when this was written) |

## The bus contract (fixed ABI, from `docs/pages/protocol.md`)

These are the numbers the shipped boot ROM and firmware agree on. **Phase 1 (M1) uses them
unchanged.** Protocol v2 (03) changes only what is explicitly listed there.

| Constant | Value | Meaning |
|----------|-------|---------|
| `FC_COM_BUF_SIZE` | 64 | Mailbox bytes appended to the frame stream |
| `PPU_COUNT_VAL` | 15426 + 64 = **15490** | `fcppu_rna` zero-based report after 15491 physical reads |
| Sync window | +/-2 reads | Outside it the DMA is stopped, not restarted |
| Phase nudge | `out pins, 8` executed manually once or twice when the count is 1 or 2 short | `rp_system::ppu_dma()` |
| `VRAM_BUF_SIZE` | (36*2*240 + 64)/4 = 4336 words = 17,344 bytes | Per buffer; two buffers |
| Stream layout | 32 selected 16-bit words per visible line, linear from word 31 (bitplane 0 low byte, bitplane 1 high byte); mailbox at byte 15426 | NES-001 trace 6, `rp_system::convVram()`, strict S0 |
| Mailbox v1 | byte 0 unused (taken by FC_PICO_GB), 1 = `$FC` magic, 2..15 commands, 16..63 APU `(reg,value)` pairs terminated by `$FF` | zero page `$20`-`$5F` on the 6502 |
| Console -> cartridge | one byte per `$2007` write with the PPU address parked at `$0800`; opcodes `$2F $3F $BF $CF $DF $EF $FF`; **any other byte is controller state** and is the frame heartbeat | |
| Controller bits | A `$80`, B `$40`, Select `$20`, Start `$10`, Up `$08`, Down `$04`, Left `$02`, Right `$01` | `SysEqu.h`, `rp_system.h` |
| Mandatory dummy read | Every `$2007` read sequence discards one byte first | |
| Attribute-table quirk | Bulk uploads to `$23C0` need one extra dummy read | `SysPico.asm` |
| Boot stamp | 14 bytes at `$EFF0`, compared against `_rom[0x6FF0]`; mismatch => self-reflash | `boot-and-reflash.md` |
| Fixed 6502 entry points | `$ED00` NMI, `$EE80` IRQ, `$EF00` MAIN_SETUP, `$EF03` MAIN_LOOP, `$EFF0` stamp, `$EFFF` erase flag; fix bank `$F000` INIT, `$F003` TRANS_SYS_FONT, `$F006` KEY_RTN, `$F009`/`$F00C` beeps | |
| PPUCTRL / PPUMASK defaults | `$2000` = `%100_01_0_00` (NMI on, BG pattern table `$0000`, **sprite pattern table `$1000`**), `$2001` = `%000_11_11_0` | `SysEqu.h` -- note the comment there says "SP$0000"; the bits say `$1000` |

### Hardware calibration of the read count **(measured 2026-09-22)**

NES-001 capture `tests/fixtures/hw_trace_ntsc/trace_6.hex` resolves the former 68-vs-64
contradiction. It contains one complete pre-render line with **66** CS1-qualified reads and
14 complete visible lines with **64** each. The PPU still makes 170 total `/RD` accesses per
rendering line; CS1 selects 31 in-line background pairs plus two prefetch pairs on pre-render,
then 30 in-line pairs plus two prefetch pairs on visible lines. `/RD` stays low for 160-200 ns,
so the difference is board selection, not missed sampling edges.

The physical frame arithmetic is therefore:

```
rendering: 66 + 240*64                    = 15426
NMI:       1 buffered-read dummy + 64     =    65
physical qualifying reads                 = 15491
fcppu_rna report (zero-based last index)  = 15490
```

`fcppu_rna` executes `mov isr, !x` before `jmp x--`, explaining the one-read report bias.
The transmitter and counter use the same CS1 and `/RD` conditions, so there is no separate
68-byte consumption rate.

The stream-layout mistake was interpretive: `convVram()` runs an outer loop of 34 words, but
its source index never resets per loop. Its useful prefix is one flat row-major 256x240 image,
32 words per physical visible line. During pre-render, words 31-32 prefetch line 0's first
16 pixels; each visible line then consumes 30 remaining words plus two words prefetched for
the next line. The mailbox begins four bytes after the last picture word.

There is likewise no four-zero-byte prefix on a normal frame re-arm. Strict Mesen alignment
only produces valid mailboxes and the measured picture when buffer byte zero is served first;
the PIO program's cold-start `mov osr, null` is not a per-frame prefix. Manual phase nudges
consume one or two real buffer bytes before normal reads resume.

### Prior-art evidence (read from the sources, no hardware)

**PiPU / NES DOOM** (`rasteri/PiPU`, GPL; the origin of the technique) drives the *whole*
PPU bus from an FX2LP with no console VRAM, so it answers every fetch. Its frame structure
(`ppusquirt/nesstuff.h`) is the clearest statement of the fetch order:

```
PPUScanline = tiles[32] (NT, AT, low, high each) + sprites[8] + nexttiles[2] + 2 unused NT fetches = 170 bytes
PPUFrame    = ScanLines[241] + OtherData[32]                                                   = 41002 bytes
```

`FitFrame()` places a line's first two tiles into the previous scanline's prefetch and
tiles 2..31 into its main region. FC PICO reaches the same linear tile order with a
different board-selection window: 30 in-line pairs plus two prefetch pairs per visible
line. PiPU's full-bus 68-byte count is therefore useful PPU timing evidence, but it is not
the FC PICO cartridge's selected-read count.

PiPU's NES program reads its 32-byte block with one dummy read plus nine extra reads "to
flush the FIFO", writes the pad byte three times to `$21C0`, and sets a palette per 8x1
slice through the attribute byte in the stream. FC PICO instead gets attribute fetches
from console VRAM and uses the 65-read NMI sequence measured above.

**Sync alternative.** PiPU's FX2 does not count at all: it measures the time since `/RD`
last fell and, when the gap exceeds a threshold (`countUp > 50` iterations), treats it as
vertical blank and resets its FIFO to the frame start. The vblank gap (no pattern fetches
from the end of line 239 until the NMI's `$2007` reads) is easy to see from a PIO program.
This is recorded in 11 (R1) as the fallback if the count-based sync proves fragile.

**FC PICO GB** (`axsann/FC_PICO_GB`, the second implementation on this board) confirms
`PPU_COUNT_VAL = 15426 + FC_COM_BUF_SIZE` with a 16-byte mailbox (15,442), the same
34-word/31-head layout, and the same DMA re-arm window, though it accepts counts in
`[VAL-2, VAL]` rather than `[VAL-2, VAL+2]`. It overclocks to 276 MHz with
`vreg_set_voltage(VREG_VOLTAGE_1_15)` and `set_sys_clock_pll(1656 MHz, 6, 1)`, runs
everything on core 0, and for audio sends only *changed* APU registers per frame, with a
rule that avoids retriggering notes: `$4003/$4007/$400B/$400F` are rewritten only when the
channel was not written in the previous frame (a new note) or when the period bits changed
(`docs`: "連続書き込み ... スキップ (クリック回避)"). `fcapu` adopts the same rule.

## Console-side budgets for Doom

| Resource | Budget | Where it goes |
|----------|--------|---------------|
| NMI, NTSC | 2273 cycles, target **<= 1900** for the vblank-critical part | mailbox read at 7 cycles/byte, attribute write at 7 cycles/byte, palette write, register restore; the APU replay (24 cycles/pair measured, 15 pairs max) may legally spill past vblank |
| Main loop | everything else (~27,500 cycles/frame) | controller read (650), command execution, bulk data mode when requested |
| Zero page | fully allocated in the tutorial ROM; the Doom ROM reallocates from scratch | 03, 07 |
| PRG | 32 KB: `$8000`-`$ECFF` free for Doom code + DPCM, `$ED00`-`$EFFF` fixed, `$F000`+ untouchable | 07 |
| DPCM | samples must lie in `$C000`-`$FFF1`, 64-byte aligned, length 16n+1; realistic budget **~11 KB** | 06, 07 |

## Cartridge-side budgets for Doom

| Resource | Budget | Notes |
|----------|--------|-------|
| RAM total | 520 KB | RP2040 Doom's RP2350 layout expects statics below and a zone heap of at most 256 KB ending at `0x20070000` (`SHORTPTR_BASE 0x20030000` + `0x40000`); the 64 KB above that and the two scratch banks are free for the bus and conversion buffers |
| RP2040 Doom statics (RP2350 build) | `frame_buffer[2][320*168]` = 107,520 B; `list_buffer` = 7200 x 12 + 4096 = 90,496 B; decoders, visplane bits, palettes: ~20 KB | `pd_render.cpp`, `i_video.c` |
| FC PICO stream buffers | 2 x 17,344 = 34,688 B (v1) -- grows by 2 x 64 for the v2 mailbox | `fcbus` |
| Conversion scratch | 320 B line buffer x 2, 256 x 4 cost table, 4 x 256 x 16 dither LUT = 16 KB (or 4 KB with a 2x2 pattern), 64 B attribute shadow x 2, block cost accumulators 240 x 4 x 2 B | 04 |
| Core 1 stack | RP2040 Doom sets `PICO_CORE1_STACK_SIZE=0x4f8` (1272 B) and notes it barely fits; raise to 4 KB on RP2350 (scratch X is free) | `src/CMakeLists.txt` |
| CPU per frame at 150 MHz | 5.0 M cycles per 30 fps frame; the converter is estimated at 0.4-0.6 M | 04 |
| Flash | 4 MB (inferred): firmware `0x10000000`-`0x1007FFFF` (512 KB cap, includes boot ROM image, music, SFX), WHX at `0x10080000` (1,800,344 B for `doom1.whx`, ends `0x10237898`), asset archive at `0x10300000`, save slots at the top of flash per RP2040 Doom's `get_end_of_flash()` | 08 |
| Flash timing | RP2040 Doom's `rp2` branch programs `QMI_M0_TIMING` clkdiv 3 / rxdelay 2 at 270 MHz; at 150 MHz the SDK default is fine | `i_main.c` |

## Engine facts that shape the port

| Fact | Consequence |
|------|-------------|
| RP2040 Doom keeps the 3D view in `frame_buffer[2][320*168]` as 8-bit palette indices; the status bar, menus, HUD text and intermission screens are **not** in that buffer but drawn per scanline from "vpatch" overlay lists by `fill_scanlines()` in `i_video.c` (into 16-bit RGB for `scanvideo`). | The FC PICO video layer must re-run that composition into an 8-bit 320x200 frame. `draw_vpatch()` becomes an 8-bit writer (drop the `palette[]` lookup). See 04. |
| Frame handoff is `pd_end_frame()` -> `sem_release(render_frame_ready)`; the display side acquires it in `new_frame_stuff()` and flips `display_frame_index`; `display_frame_freed` throttles the renderer to at most two frames ahead. | Reuse both semaphores as-is; the converter takes the display-side role. |
| Core 0 runs game logic and builds column lists; core 1 draws visplanes and half the columns (`pd_core1_loop`), and in the original also composes scanlines in a low-priority IRQ (`LOW_PRIO_IRQ 31`). | The converter runs on core 1 in the same low-priority-IRQ slot; the bus ISR is a separate, higher-priority, microsecond-scale handler. |
| Palette changes arrive as `I_SetPaletteNum(n)` with n in 0..13 (`PLAYPAL`); the RP2350 build synthesises tints from palette 0 when the WHX carries only one palette. | Map the 14 palettes to 14 precomputed NES palette sets; the per-pixel LUT is built for palette 0 only. See 04. |
| Video types: `NONE`, `TEXT` (ENDOOM), `SAVING`, `DOUBLE` (level), `SINGLE` (full-screen patch: title, intermission), `WIPE`. | Each needs an 8-bit composition path; `TEXT` can be a fixed 32-column font rendering instead of the 80-column VGA text mode. |
| `I_GetTime()` is `time_us_64()` based; the tic rate is independent of display refresh. | Nothing to change for the 60.1 Hz heartbeat. |
| Input arrives via `pico_key_down/up(scancode, ...)` -> `D_PostEvent`. | The controller mapper posts synthetic key events. See 05. |
| Sound: `sound_pico_module` (ADPCM mixer) and `music_opl_module` (emu8950) through `I_PicoSoundSetMusicGenerator`. `I_UpdateSound()` is polled from the game loop and from core 1 wait loops. | Replace both modules with `sound_fcpico_module` / `music_fcpico_module` that drive the APU sequencer; `I_UpdateSound()` becomes the sequencer's per-frame pump. See 06. |
| Saves: `picoflash_sector_program()` writes 4 KB sectors; slots are found by `P_SaveGameGetExistingFlashSlotAddresses()`. | Keep. Place the slots above the asset archive. |
| `USE_ZONE_FOR_MALLOC`: `malloc` is wrapped into `Z_Malloc`; **no allocation on core 1 after startup** (`disallow_core1_malloc`). | The converter and bus layer must be statically allocated. |
| Build flags of note: `DEMO1_ONLY=1` for the super-tiny target, `NO_USE_ARGS`, `NO_FILE_ACCESS`, `USE_WHD`, `WHD_SUPER_TINY`, `NO_USE_NET`, `USE_PICO_NET` (I2C). | The fcpico target starts from `doom_tiny` flags with `USE_PICO_NET=0`, `USB_SUPPORT=0`. |
| Compiler: the README warns that binary size is tight and gcc 10.x is bad; author used arm-none-eabi-gcc 13.2.rel1. | Pin 13.2.Rel1 in CI. |
