@page platform_findings Platform findings

What was learned by running the tutorial protocol on hardware, in the MesenCE
cartridge model (@ref emulator_cartridge) and in host tests (@ref shared_bus_core).
Each entry says how it is known:

- **measured**: observed on hardware, on the configuration named;
- **verified**: checked by a test or emulator scenario in this repository;
- **unresolved**: observed, cause unknown.

Unless stated otherwise the configuration is the tutorial boot ROM and firmware on an
NTSC NES-001. Other consoles, regions and ROMs may differ.

## Chip select and read counts

- **Selection is by address, `$0800`-`$0FFF` (measured, verified).** The cartridge
  answers the pattern fetches of the tutorial's nametable 0, which uses tile `$80`
  (`$0800`), and the boot ROM's `$2007` accesses parked at `$0800`. The font at
  `$0000`, nametable 1's tile `$00` and the sprite patterns stay in CHR RAM. A wider
  decode (`$0000`-`$0FFF` or `$0000`-`$1FFF`) does not reproduce the hardware counts;
  the emulator mapper keeps those masks only as diagnostics (`FCPICO_CS1_MASK`).
- **66 and 64 (measured).** A logic capture of the cartridge bus shows 66 selected
  reads on the pre-render line and 64 on each visible line, with every `/RD` pulse
  low for 160-200 ns. The capture (`trace_6.hex`) is kept with the
  [FC PICO Doom port](https://github.com/grebz-dev/rp2040-doom/tree/fc-pico/fcpico/tests/fixtures/hw_trace_ntsc).
- **15490 is a zero-based count (measured, verified).** One frame is
  `66 + 240 * 64 = 15426` picture reads, plus the NMI's dummy read and 64 mailbox
  reads: 15491 physical reads. `fcppu_rna` reports the index of the last read, 15490,
  which is #PPU_COUNT_VAL. Models that count reads must subtract one
  (`PPU_COUNTER_REPORT_BIAS` in `fcbus_protocol.h`).
- **The ±2 window is the tolerance, not jitter (measured, verified).** In steady state the
  emulator and the hardware report exactly 15490 on every frame.

## Stream layout

- **The picture is flat (verified).** Line `y`, tile `x` is word `31 + 32 * y + x`;
  the mailbox starts at byte 15426. `fcbus_stream_word()` and the generated layout
  descriptor give the same geometry.
- **`convVram()` reads past the canvas (verified).** It converts 240 passes of 34
  words from a 256 x 240 canvas, reading 3,840 bytes beyond it. The extra words are
  never displayed, but the read is outside the frame buffer.

## Data mode and the frame link

- **VRAM pokes are disabled in the shipped boot ROM (verified).** `xPF_COM_VRAM`,
  `xPF_COM_SE` and `xPF_COM_BGM` are inside `.if 0`. Palette and attribute changes
  therefore need data mode; rp_system::update()'s per-frame pokes have no effect with
  this ROM.
- **Data mode costs a resynchronisation (verified, consistent with hardware).** The
  data-mode exchange happens between heartbeats, so its reads make the next frame's
  count wrong. That heartbeat stops the DMA and a later one re-arms it. Hardware
  serial logs from the Doom port show the same `stops=1 resyncs=1` after startup.
- **`PF_COM_DMOD` can be sent once more after `FP_COM_DRQ` (verified).** The command
  may already be in the mailbox being built when the console answers, and a stopped
  heartbeat keeps that mailbox. The console then runs one more data-mode round, which
  the cartridge ends at once with #PF_COM_NONE. This is rp_system's behaviour too.
- **Data-mode frames count only a few reads (verified).** The boot ROM blanks the
  picture around data mode, so the heartbeats there report the 64 mailbox reads plus
  the exchange, not 15490 plus extra. In the MesenCE `demo` scenario an upload frame
  counts 111 or 184, the extra empty round 71, and the blanked frames 64. The test
  console in `sim/cartmodel/tests/test_cartmodel.c` keeps streaming the picture
  during data mode and reads the whole reply header, so its counts there (15490 plus
  the exchange) are not console counts; take those from the emulator.
- **Sprites are fetched from `$1000` (verified from the source).** `FLG_PPU2000` in
  `BOOTROM/SysEqu.h` is `%100_01_0_00` = `$88`: bit 3 is set, so 8x8 sprites use the
  pattern table at `$1000`, which never reaches the cartridge. Its comment says both
  tables are at `$0000`; only the background table is.
- **Attribute pages arrive one byte late (measured, unresolved).** The boot ROM takes
  a dummy read before attribute (`$23C0`) uploads only, and its comment says the cause
  was never found. The host model serves the extra byte; without it the emulator shows
  every attribute shifted by one.

## Debugger reads

- **Inspection must not consume cartridge bytes (verified).** The cartridge is a
  stream: an extra read changes everything after it. The emulator mapper only counts
  real renderer fetches and CPU `$2007` reads; a scenario reads the whole PPU address
  space through the debugger every frame and requires an identical cartridge trace.

## Native graphics

Generic NES limits, which matter when a cartridge application draws with tiles and
sprites instead of the streamed picture:

- four background and four sprite palettes of three colours plus a shared backdrop;
- one attribute selection per 16 x 16 block;
- 64 sprites, at most eight per scanline, in 8 x 8 and 8 x 16 mode alike. Emulator
  runs keep the hardware limit on (`RemoveSpriteLimit` false) so screenshots show
  what a console shows;
- switching between screens that use OAM differently (for example a menu over a game)
  needs the complete OAM state of the screen being returned to. Restoring only the
  entries the other screen changed left stale sprites in the Doom port.

## Build and flashing

- **Native assembly reproduces the vendor ROM (verified).** NESASM CE at
  `6fc41cda` produces the same PRG; only two header bytes differ, which
  `tools/nes/normalize_ines.py` clears. See @ref build_pipeline.
- **The distributed firmware was built with arduino-pico 6.0.0 (inferred).** The UF2
  embeds the core's install path
  (`.../packages/rp2040/hardware/rp2040/6.0.0/...`). A Linux build with that core is
  the same size but not byte-identical, because the embedded paths differ. See
  @ref reproducible_build.
- **RP2350 UF2s contain an extra "absolute" family block (verified).** Besides the
  RP2350 ARM Secure blocks (`0xe48bff59`), picotool output includes a block with
  family `0xe48bff57`, a workaround for RP2350 erratum E10. UF2 checks must allow it.
- **Firmware can be updated without the BOOT button.** `bin/arduino_picoboot.exe`
  opens the board's serial port at 1200 baud, which makes the arduino-pico core reboot
  into BOOTSEL; every `upload.bat` uses it. On Linux, `stty -F /dev/ttyACM0 1200`
  does the same (macOS: `stty -f /dev/cu.usbmodem<N> 1200`); then load the UF2 with
  `picotool load -x`. The tool takes the first COM port that `mode` lists, so
  disconnect other serial devices first. Keep the console off while flashing
  (@ref flashing).

## What each kind of test proves

| Evidence | Shows | Does not show |
|---|---|---|
| Host C tests (`platform/tests`) | Protocol state machine, mailbox, tables, data mode, sync decisions | Anything about the console |
| Emulator scenarios (`sim/mesen`) | The real boot ROM against the protocol model: read counts, mailbox validity, reflash, input, data-mode uploads | RP2350 timing, PIO behaviour, bus electrical margins |
| Native ROM build | The boot ROM bytes and the permanent bank | That the ROM runs |
| Hardware | Everything above on one console | Other consoles and regions |

A passing emulator run is not evidence of hardware timing. Keep measured results,
inferred explanations and unresolved observations separate when adding to this page.
