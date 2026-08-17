@page generated_resources Generated resources

One C file under `tutorial_project/tuto1_hw/res/` is a machine-generated hex array.
It is excluded from the Doxygen input -- a multi-thousand-line hex dump makes a
useless reference page -- so it is described here instead.

## The archive

`res/resdata.c` defines a single blob:

```c
const unsigned char _resdata[48340] __attribute__((aligned(4))) = { 0x.., ... };
const int _resdata_length = 48340;
```

@warning The alignment is required, not cosmetic. This blob is streamed by 32-bit
DMA into the PIO transmit FIFO; an unaligned array would fault or corrupt the stream.

Inside it are four assets, preceded by an index table of `int` pairs -- offset then
size, one pair per entry, in `binlink.lst` order. The offsets are relative to the
start of the archive, so the table pointer doubles as the base pointer.

| Id (`res/res_id.h`) | Offset | Size | Upstream |
|---|---|---|---|
| `NES_ROM` | 32 | 32784 | `BOOTROM/rom.NES`, **header included** |
| `NSF_SOUND` | 32816 | 7332 | `mml/sound.nsf` |
| `CHR_OBJ` | 40148 | 4096 | `res/OBJ.chr` |
| `CHR_FONT` | 44244 | 4096 | `res/font.chr` |

Nothing reads the assets by symbol any more. They are fetched by id:

```cpp
const unsigned char *p = getResData( CHR_FONT );
int                  n = getResDataSize( CHR_FONT );
```

@note The id constants are derived from the file names by `binlink`, as
`<EXT>_<BASENAME>` uppercased -- `rom.NES` becomes `NES_ROM`. Renaming an input in
`binlink.lst` silently renames the constant.

## A second archive

An id of 10000 or more selects a second archive, read straight from flash at
`RES_DATA_ADR` (`0x10200000`) instead of from `_resdata`; see `getResHead()`. That
archive is built by passing `10000` as the id base to `binlink`, and is flashed
separately from the firmware:

```
picotool load -o 0x10200000 -t bin res2.bin
```

See `tutorial_project/upload/`. Nothing in the build checks that the firmware image
has not grown past `0x10200000`.

## Regenerating

```
cd tutorial_project/tuto1_hw/res
conv.bat
```

`binlink` reads the boot ROM and the music from `BOOTROM/` and `mml/` directly, so
those must be rebuilt first if they have changed:

```
cd tutorial_project/BOOTROM
m.BAT           :: assemble -> rom.NES
```

@warning `BOOTROM/update.bat` no longer feeds the firmware. It still runs `Bin2C` to
produce `rom.c`, then deletes it; only the `copy rom.nes ..\ROM\` remains useful.
The ROM reaches the firmware through `conv.bat` now. See @ref build_pipeline.

## What each one is for

### `NES_ROM` — the console's program image

The cartridge's own copy of the boot ROM. Both consumers skip the 16-byte iNES header
themselves (`getResData( NES_ROM ) + 0x10`), so the pointer they work from is CPU
`$8000`:

- rp_system::ver_dma() streams `_rom[0x6FF0]`, the 16-byte build stamp, in reply to
  #FP_COM_VER.
- rp_system::rom_dma() streams any 256-byte page in reply to #FP_COM_ROM, which is how
  the console reflashes itself.

@warning This entry **is** the ROM the console will end up running. Rebuilding the ROM
without re-running `conv.bat` and reflashing the firmware leaves the two permanently
disagreeing, so every boot triggers a reflash. See @ref boot_reflash.

### `CHR_FONT` — the text glyphs

Standard NES 2bpp pattern data, 256 tiles of 16 bytes. `initResData()` resolves it
into the global `_font` pointer, which is passed explicitly to each call:

```cpp
c.drawString("HELLO WORLD!", 8*10, 100, _font);
```

### `CHR_OBJ` — the sprite sheet

Same format, resolved into `_acOBJ` by `initResData()`. Bound once with `c.setSprData(_acOBJ)`, then selected per draw with
`Canvas::setSprChr()`. Multi-tile sprites use a stride of `0x10` per row.

### `NSF_SOUND` — the music

An NSD.Lib-generated NSF: `NESM\x1a`, 25 songs, load `$8000`, init `$8010`, play
`$8084`. Those addresses are hard-coded in rp_nsfplayer.h.

Handed to the player at startup:

```cpp
nsf.init();
nsf.setNSF( getResData( NSF_SOUND ) );
```

See @ref audio_page.

## Also excluded

| Pattern | Why |
|---|---|
| `*.lst` | nesasm listings, regenerated every build (145 KB and 216 KB) |
| `*.log` | Assembler logs |
| `*/bin/*` | Checked-in toolchain executables |
| `sys/pio/fcppu.pio.h` | pioasm output; `fcppu.pio` is the source of truth |
| `_command.lnk` | Windows shortcuts for opening a shell, not build inputs |

@note `fcppu.pio.h` is checked in but was generated from a slightly earlier revision
than `fcppu.pio` -- the instruction order in `fcppu_dir` differs. **`fcppu.pio` is
authoritative.** Regenerate the header with `sys/pio/pioasm.exe` if you change it.
