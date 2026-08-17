@page flashing Flashing the cartridge

Three separate things can be updated, by three different routes.

| Target | Route | Hardware needed |
|---|---|---|
| RP2350 firmware | USB-C, UF2 drag-and-drop | none |
| 6502 boot ROM | in-band self-reflash | none |
| 6502 boot ROM | out-of-band, `anago` | kazzo programmer |

## RP2350 firmware

Hold **BOOT** while connecting USB-C. The cartridge enumerates as a mass-storage
device; copy the `.uf2` onto it and it reboots into the new firmware.

This is the normal path and needs no tools beyond a Windows PC and a cable. The
cartridge does not need to be opened.

## Boot ROM, in-band

The cartridge can reprogram the console's flash through the PPU bus. Triggered either
by holding **Start** at power-on, or automatically when the build stamps disagree.

The console then erases `$8000`-`$EFFF` and reprograms it with bytes streamed from
`_rom[]` in the firmware. The permanent fix bank at `$F000` is never erased, so a failed
update can be retried.

@warning The console's ROM is therefore only ever as current as the **firmware's copy of
it**. Updating the firmware updates the ROM image too. This is the intended workflow;
see @ref build_pipeline.

Full detail in @ref boot_reflash.

## Boot ROM, out-of-band

Needed at least once for a blank cartridge, and for recovery if both banks are damaged.

```
cd tutorial_project/rom
rom.bat
```

which runs:

```
anago Ffe nrom_wx.af rom.nes AM29F040B AM29F040B
```

- `F` — flash/program mode
- `fe` — full transfer on both the CPU and PPU sides
- `nrom_wx.af` — board description
- `AM29F040B` — device name, given twice (CPU-side and PPU-side)

`anago` is the host tool for the kazzo Famicom dumper/writer, scripted in Squirrel.

### Board description

`nrom_wx.af` declares an NROM board:

```
board <- {
    mappernum = 0, ppu_ramfind = false, vram_mirrorfind = true,
    cpu_rom = { size_base = 0x8000, size_max = 0x8000, banksize = 0x2000 },
    ppu_rom = { size_base = 0x2000, size_max = 0x2000, banksize = 0x2000 }
};
```

`cpu_transfer` interleaves bank-latch writes to `$E000` with programming of each 8 KB
window. The "wx" board has a write-enable latch there, which is also why `macro.h`'s
`CHG_BANK_A` writes `$E000` even though a plain NROM cartridge has nothing to bank.

### Device table

`flashdevice.nut` describes the part:

```
["AM29F040B"] = { capacity = 4*mega, pagesize = 1, erase_wait = 8000,
                  erase_require = true, retry = false,
                  id_manufacurer = 0x01, id_device = 0xa4,
                  command_mask = MASK_A10 }
```

`MASK_A10` is `0x7FF`, matching the `$D555`/`$AAAA` unlock addresses the 6502 uses for
its own in-band programming.

@note `BOOTROM/defDebug.h` records `FLASH_DEV_CODE = $A4` (agrees) but
`FLASH_MAN_CODE = $C2` (disagrees with `0x01` here). The 6502 code does not check the
manufacturer ID, so the discrepancy is inert -- but do not rely on that constant.

Other parts `anago` supports: `W29C020`, `W29C040`, `W49F002`, `AT49F002`, `EN29F002T`,
`PM29F002T`, `MBM29F080A`, `SST39SF040`.

## If the cartridge will not start

From the manual, in order:

1. Press reset.
2. Reseat the cartridge.
3. Try a genuine console or a different clone -- the graphics technique depends on exact
   PPU bus behaviour and not all clones match. See @ref hardware.
4. If nothing works, contact impact soft (see @ref references).

A console showing **`PICO NOT FOUND`** has a working boot ROM but is getting no reply
from the cartridge; that is a link failure, not a corrupt ROM.
