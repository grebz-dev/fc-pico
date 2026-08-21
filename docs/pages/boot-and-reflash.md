@page boot_reflash Boot and self-reflash

The cartridge can rewrite the console's boot ROM through the PPU bus. This page covers
how the two banks are split so that is survivable, and how the update runs.

## Two banks

The 32 KB PRG image is assembled as two independent units:

| Bank | Range | Source | Erasable |
|---|---|---|---|
| Main | `$8000`-`$EFFF` | `BOOTROM/` | **yes** |
| Fix | `$F000`-`$FFFF` | `BOOTROM_FIX/` | **no** |

The fix bank owns the reset vectors, the RAM test, the version check, and the flash
erase/program routines. It is the part that must never be destroyed, because it is what
performs the update. A failed reflash therefore leaves a cartridge that still boots and
can retry.

`BOOTROM_FIX` is assembled first, cut out with `bin_catcut`, and included into the main
build as `bootrom_fixr.bin`.

## The bank ABI

The two banks are separately assembled, so they communicate through fixed addresses.
The fix bank publishes a jump table at `$F000`:

| Address | Entry |
|---|---|
| `$F000` | `INIT` |
| `$F003` | `TRANS_SYS_FONT` |
| `$F006` | `KEY_RTN` |
| `$F009` | `BEEP_PI` |
| `$F00C` | `BEEP_PO` |

The main bank publishes:

| Address | Entry |
|---|---|
| `$ED00` | `ROM_NMI_ENTRY` -> @ref NMI |
| `$EE80` | `ROM_IRQ_ENTRY` |
| `$EF00` | `MAIN_SETUP` |
| `$EF03` | `MAIN_LOOP` |
| `$EFF0` | `DB_ROM_VER` -- 14-byte build stamp |
| `$EFFF` | `IS_ROM_ERACE` -- non-zero once erase has begun |

@warning These addresses are hard-coded on both sides. Moving one requires editing both
`BOOTROM/PG_main.asm` and `BOOTROM_FIX/PG_main.asm`.

## Cold boot

`BR_INIT` at `$F000`, in order: mask interrupts, clear PPU registers, wait for vertical
blank, toggle A12, clear OAM and palettes, clear the nametables, expand the system font
into both pattern tables, clear RAM (skipping `$0100` so high scores survive a reset),
then read the controller twice.

Then the branch that decides the boot path:

@dot
digraph boot {
  rankdir=TB;
  node [shape=box, fontname="Helvetica", fontsize=9];
  edge [fontname="Helvetica", fontsize=8];
  init  [label="BR_INIT"];
  rst   [label="send FP_COM_RST\nto cartridge"];
  start [label="Start held?", shape=diamond];
  flag  [label="IS_ROM_ERACE != 0?", shape=diamond];
  mem   [label="BOOT_MEMTEST"];
  ver   [label="CHK_ROMVER:\ncompare $EFF0 stamp\nwith cartridge copy", shape=diamond];
  erase [label="ROM_ERACE\n(erase $8000-$EFFF)"];
  upd   [label="ROM_UPDATE\n(reprogram from cartridge)"];
  run   [label="MAIN_SETUP -> MAIN_LOOP"];
  init -> rst -> start;
  start -> erase [label="yes"];
  start -> flag  [label="no"];
  flag  -> upd   [label="yes"];
  flag  -> mem   [label="no"];
  mem -> ver;
  ver -> erase [label="mismatch"];
  ver -> run   [label="match"];
  erase -> upd; upd -> init [label="jmp INIT"];
}
@enddot

So there are two ways into an update: **hold Start at power-on**, or let the version
check fail.

## The version handshake

`CHK_ROMVER` sends #FP_COM_VER and compares the 14-byte reply against its own
`DB_ROM_VER` at `$EFF0`. The cartridge answers from `_rom[0x6FF0]` -- its compiled-in
copy of the same ROM image (rp_system::ver_dma()).

Equal means the console is running the ROM the cartridge expects. Unequal means it is
not, and the console reflashes itself.

If the reply does not even begin `"20"`, the cartridge is not responding at all and the
console prints **`PICO NOT FOUND`** and halts. That message means the link is dead, not
that the ROM is wrong.

@warning `BOOTROM/m.BAT` regenerates `dbdate.h` with the current timestamp on **every**
run. Rebuild the ROM without also rebuilding the firmware and every subsequent boot will
trigger a reflash. See @ref build_pipeline.

## Programming the flash

`ROM_ERACE` scans upward from `$8000` for the first non-`$FF` byte and erases sector by
sector, stopping when the destination pointer reaches `$F000` -- the fix bank is never
touched.

`ROM_UPDATE` requests each page with #FP_COM_ROM and programs it with bytes read
straight out of `$2007`.

Both `CPU_FlashSectorElase` and `CPU_FlashPorgram` **copy themselves into RAM** at
`FLASH_EXEC_BUF` (`$500`) and run from there, with interrupts masked. This is
mandatory: a flash device cannot be read while it is busy, so code executing from it
would fetch garbage as its own next instruction.

The unlock sequence is standard JEDEC, mapped through the `$8000` window:

```asm
lda #$AA : sta $D555
lda #$55 : sta $AAAA
lda #$80 / #$A0 : sta $D555     ; erase setup / program
```

`$D555 & 0x7FF = 0x555` and `$AAAA & 0x7FF = 0x2AA`, matching the `MASK_A10` command
mask that `rom/flashdevice.nut` records for the AM29F040B.

Erase polls DQ3 then DQ6; programming polls DQ6 by comparing two consecutive reads and
then verifies against a copy staged in `FLASH_SAVE_BUF`.

@note `SysBootRom.asm` contains a verbatim Japanese quotation of the AM29F040 datasheet
section on sector-erase timing, inside a `.if 0` block. It is reference material, not
code.

## Recovery

If an update fails, the fix bank is intact, so the console still boots into `BR_INIT`,
still finds `IS_ROM_ERACE` set, and retries `ROM_UPDATE`. Out-of-band recovery with a
kazzo programmer is described in @ref flashing.
