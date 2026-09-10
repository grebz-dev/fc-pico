# 07 -- The Doom boot ROM (6502)

A new **erasable bank** (`$8000`-`$EFFF`) for the console, assembled with the same
`nesasm 2.51+autozp` the tutorial uses, installed by the cartridge through the unchanged
permanent **fix bank** (`$F000`-`$FFFF`, `BOOTROM_FIX/`, `bootrom_fixr.bin`,
MD5 `1A9A2AC85F1E7A15AF9DF47013432BF9`).

## Contract with the fix bank (must hold, see `docs/pages/boot-and-reflash.md`)

| Address | Symbol | The fix bank... |
|---------|--------|-----------------|
| `$ED00` | `ROM_NMI_ENTRY` | vectors NMI here (`jmp NMI`) |
| `$EE80` | `ROM_IRQ_ENTRY` | vectors IRQ here (`rti`) |
| `$EF00` | `MAIN_SETUP` | calls once after boot checks |
| `$EF03` | `MAIN_LOOP` | jumps to; never returns |
| `$EFF0` | `DB_ROM_VER` | 14 bytes compared with the cartridge's `FP_COM_VER` reply |
| `$EFFF` | `IS_ROM_ERACE` | non-zero = interrupted reflash, retry |
| `$F000`.. | `INIT`, `TRANS_SYS_FONT`, `KEY_RTN` (`$F006`), `BEEP_PI`, `BEEP_PO` | are provided to us |

The fix bank also owns: RAM test, controller 1 read with majority vote (`BR_KEY_RTN`),
`CHK_ROMVER`, `ROM_ERACE`, `ROM_UPDATE` (pages `$80`-`$EF` from `FP_COM_ROM`), the JEDEC flash
routines copied to `$500`. It expands a system font into both pattern tables at boot -- that
writes go to the cartridge (CS1) and are ignored; harmless.

`$2000`/`$2001` are left with NMI off by the fix bank; `MAIN_SETUP` turns things on.

## Bank layout (`PG_main.asm`)

| Bank | Range | Contents |
|------|-------|----------|
| 0 | `$8000`-`$9FFF` | code: NMI, main loop, init, data mode, controller 2, fades, macros' subroutines |
| 1 | `$A000`-`$BFFF` | code overflow, tables, boot palette, "loading" nametable (optional) |
| 2 | `$C000`-`$DFFF` | **DPCM samples** (64-byte aligned; `.incbin "gen/dpcm_bank.bin"`) |
| 3 | `$E000`-`$ECFF` | DPCM samples continued |
| 3 | `$ED00` | `jmp NMI` |
| 3 | `$EE80` | `rti` |
| 3 | `$EF00`/`$EF03` | `jmp UR_MAIN_SETUP` / `jmp UR_MAIN_LOOP` |
| 3 | `$EFF0` | `DB "DOOM-02-000001"` -- from `gen/version.inc` (protocol version, build number), **not** the wall clock |
| 3 | `$EFFF` | `db 0` |
| -- | `$F000` | `.incbin "bootrom_fixr.bin"` (copied from `tutorial_project/BOOTROM/`) |

DPCM samples must not cross `$ED00`; `tools/dpcm_pack.py` places them and fails if the budget
(`$C000`-`$ECFF` = 11,520 bytes) is exceeded. The tutorial's `macro.h` is reused verbatim
(`SET_VRAM_ADD2`, `DISP_ON/OFF`, `TBL_JUMP`, string helpers); `SysSub.asm`, `SysPallet.asm`
(fades), `SysPico.asm` (data mode), `SysNES.asm` (`WAIT_VSYNC`) are copied and trimmed;
`SysArduino.asm` and every FC-EXA vestige are dropped; opcode `EQU`s come from
`gen/protocol.inc`.

## RAM map (`defs/ram.inc`)

The fix bank owns addresses it reads and writes across the bank boundary; those are fixed.
From `BOOTROM_FIX/SysEqu.h`: scratch `$00`-`$0D` (`W_AR`, `W_BR`, `TMP_SV*`, `SRC_ADR`,
`DST_ADR`, `TMP_SYS`, `TMP_SYS2`), `RAM_PAGE $25` / `MEM_DISP $26` / `PICO_MODE $51`
(boot-time only, before `MAIN_SETUP`), `KEY_REL/TRG/OLD/NEW $82`-`$85`, `REP_KEY/NEW/CNT
$8A`-`$8C` (written by `KEY_RTN` at `$F006` every frame), `FLG_2000 $B0`, `FLG_2001 $B1`,
`NMI_FLG $B3`, `PICO_DATA_BUF`/`FLASH_SAVE_BUF $400`, `FLASH_EXEC_BUF $500`.

| Range | Use |
|-------|-----|
| `$00`-`$1F` | scratch, pointers, NMI register saves (as the tutorial; `$0C`/`$0D` are the fix bank's controller-vote scratch) |
| `$20`-`$5F` | **mailbox v2 bytes 0-63** (v1 layout: flags, magic, commands, APU pairs, palette) -- also where v1 tools expect it; `$25`, `$26`, `$51` are only used by the fix bank before `MAIN_SETUP` runs |
| `$60`-`$7F` | `PICO_COM`, `PICO_STAGE`, protocol version seen, pad 2 state (`KEY2_*`, majority-vote scratch), frame counters |
| `$82`-`$8C` | pad 1 state -- **fixed by the fix bank** |
| `$90`-`$AF` | scroll shadows, fade state pointers, application step |
| `$B0`-`$BF` | `FLG_2000`, `FLG_2001`, `NMI_FLG` at their fix-bank addresses; `SYS_TIMER`, `STG_COD` |
| `$C0`-`$FF` | **mailbox v2 bytes 64-127** (the attribute table). The wire order is still one contiguous 128-byte block; the NMI's unrolled reads simply target two zero-page windows. |
| `$0100` | stack |
| `$0200`-`$02FF` | unused (was OAM; Doom never does sprite DMA) |
| `$0300`-`$03FF` | palette work (`PAL_WRK`), fade state |
| `$0400`-`$04FF` | `PICO_DATA_BUF` bulk staging (shared with the fix bank's flash buffers, which are only live during a reflash) |
| `$0500`-`$05FF` | `FLASH_EXEC_BUF` -- leave free |
| `$0600`-`$07FF` | free |

## The v2 NMI, with its cycle budget

Order is dictated by the PPU's internal address latch: every `$2006` write sets `t` and `v`,
and rendering copies `t` into `v` at the pre-render line; so **all `$2006`/`$2007` traffic
comes first, then `$2000` and the two `$2005` writes restore `t` to (nametable 0, scroll 0,0)**,
and only then anything that can spill past vblank.

| Step | Code shape | Cycles (worst) |
|------|-----------|----------------|
| entry, save A/X/Y, `bit $2002`, reentrancy check | as tutorial | 30 |
| `SET_VRAM_ADD2 #$0800`; dummy `lda $2007` | | 16 |
| read 128 mailbox bytes to zero page | unrolled `lda $2007 / sta <zp` x128 | 896 |
| magic check (`$FC` at offset 1) | | 8 |
| if `ATTR_VALID`: `SET_VRAM_ADD2 #$23C0`; 64 x `lda <zp / sta $2007` | unrolled | 12 + 448 |
| if `PAL_VALID`: `SET_VRAM_ADD2 #$3F00`; 16 x `lda <zp / sta $2007` | unrolled | 12 + 112 |
| controller packet: `SET_VRAM_ADD2 #$0800`; `lda #FP_COM_KEY / sta $2007`; pad1; pad2 | | 12 + 21 |
| `$2000 <- FLG_2000`, `$2001 <- FLG_2001`, `$2005 <- 0`, `$2005 <- 0` | | 28 |
| **vblank-critical subtotal** | | **1595** |
| APU replay: up to 16 pairs, `ldy/bmi/inx/lda,x/inx/sta $4000,y/cpx/bne` | loop | 16 x 24 = 384 |
| clear `NMI_FLG`, restore registers, `rti` | | 20 |
| **total** | | **1999** (NTSC vblank = 2273) |

The APU replay after the PPU work may legally spill past the end of vblank (APU registers are
not rendering-sensitive), so the real constraint is the 1595-cycle critical section, which has
a 30% margin. If measurement (P2-T3) shows the critical section over 1900 cycles, options in
order: read the attribute block only when `ATTR_VALID` (the cartridge knows what it sent and
adjusts the expected read count: v2.1), send half the attribute table per frame, drop the
palette block (send palettes through `PF_COM_VRAM` pokes).

The heartbeat is sent *before* the APU replay so the cartridge re-arms the DMA as early as
possible. Nothing after the packet may touch `$2006`/`$2007`.

`jobPICO` (v1 command executor: `PF_COM_DMOD`, fades, VRAM pokes) runs from the main loop as
in the tutorial, reading the first 16 mailbox bytes.

### The v2 NMI, as nesasm (draft to be assembled in P2-T2)

Names come from `gen/protocol.inc`; `RCV_PICO_BUF` is the tutorial's unrolled 8-byte read
macro, `SND_PICO_BUF` its mirror image (8 x `lda <zp / sta $2007`).

```asm
NMI:
	bit  $2002                 ; clear vblank flag, reset the $2005/$2006 toggle
	sta  <NMI_SVA
	incw <SYS_TIMER
	lda  <NMI_FLG              ; re-entrancy guard
	beq  .ok
	lda  <NMI_SVA
	rti
.ok
	inc  <NMI_FLG
	stx  <NMI_SVX
	sty  <NMI_SVY

	; 1. mailbox: 128 bytes from the cartridge port ($2006/$2007 traffic first)
	SET_VRAM_ADD2 #$0800
	lda  $2007                 ; mandatory dummy read
	RCV_PICO_BUF  MBX_ZP_LO+$00 ; bytes 0..7   -> $20..$27
	RCV_PICO_BUF  MBX_ZP_LO+$08
	RCV_PICO_BUF  MBX_ZP_LO+$10
	RCV_PICO_BUF  MBX_ZP_LO+$18
	RCV_PICO_BUF  MBX_ZP_LO+$20
	RCV_PICO_BUF  MBX_ZP_LO+$28
	RCV_PICO_BUF  MBX_ZP_LO+$30
	RCV_PICO_BUF  MBX_ZP_LO+$38 ; bytes 56..63 -> $58..$5F
	RCV_PICO_BUF  MBX_ZP_HI+$00 ; bytes 64..71 -> $C0..$C7 (attribute table)
	RCV_PICO_BUF  MBX_ZP_HI+$08
	RCV_PICO_BUF  MBX_ZP_HI+$10
	RCV_PICO_BUF  MBX_ZP_HI+$18
	RCV_PICO_BUF  MBX_ZP_HI+$20
	RCV_PICO_BUF  MBX_ZP_HI+$28
	RCV_PICO_BUF  MBX_ZP_HI+$30
	RCV_PICO_BUF  MBX_ZP_HI+$38 ; bytes 120..127 -> $F8..$FF

	; 2. validity: a torn mailbox is neither applied nor executed
	lda  <MBX_ZP_LO+MBX_MAGIC
	cmp  #PF_MAGIC_CODE
	beq  .valid
	lda  #0
	sta  <MBX_ZP_LO+MBX_CMD    ; PF_COM_NONE, so jobPICO runs nothing
	sta  <MBX_ZP_LO+MBX_FLAGS
	beq  .no_vram
.valid
	; 3. attribute table, 64 bytes
	lda  <MBX_ZP_LO+MBX_FLAGS
	and  #MBX_FLAG_ATTR_VALID
	beq  .no_attr
	SET_VRAM_ADD2 #$23C0
	SND_PICO_BUF  MBX_ZP_HI+$00
	SND_PICO_BUF  MBX_ZP_HI+$08
	SND_PICO_BUF  MBX_ZP_HI+$10
	SND_PICO_BUF  MBX_ZP_HI+$18
	SND_PICO_BUF  MBX_ZP_HI+$20
	SND_PICO_BUF  MBX_ZP_HI+$28
	SND_PICO_BUF  MBX_ZP_HI+$30
	SND_PICO_BUF  MBX_ZP_HI+$38
.no_attr
	; 4. BG palette, 16 bytes
	lda  <MBX_ZP_LO+MBX_FLAGS
	and  #MBX_FLAG_PAL_VALID
	beq  .no_vram
	SET_VRAM_ADD2 #$3F00
	SND_PICO_BUF  MBX_ZP_LO+MBX_PAL
	SND_PICO_BUF  MBX_ZP_LO+MBX_PAL+8
.no_vram

	; 5. controller packet: the v2 heartbeat, last $2006/$2007 traffic of the frame
	SET_VRAM_ADD2 #$0800
	lda  #FP_COM_KEY
	sta  $2007
	lda  <KEY_NEW              ; pad 1, from the fix bank's KEY_RTN in the main loop
	sta  $2007
	lda  <KEY2_NEW             ; pad 2, from KEY_RTN2 in this bank
	sta  $2007

	; 6. restore t: nametable 0, scroll (0,0); rendering settings
	lda  <FLG_2000
	sta  $2000
	lda  <FLG_2001
	sta  $2001
	lda  #0
	sta  $2005
	sta  $2005
	; ---- end of the vblank-critical section ----

	; 7. APU replay: (reg,value) pairs, terminated by a byte with bit 7 set
	lda  <MBX_ZP_LO+MBX_FLAGS
	and  #MBX_FLAG_APU_VALID
	beq  .apu_end
	ldx  #0
.apu_loop
	ldy  <MBX_ZP_LO+MBX_APU,x  ; zp,X: 4 cycles
	bmi  .apu_end
	inx
	lda  <MBX_ZP_LO+MBX_APU,x
	inx
	sta  $4000,y               ; abs,Y: 5 cycles
	cpx  #MBX_APU_LEN
	bne  .apu_loop
.apu_end

	; 8. epilogue
	ldy  <NMI_SVY
	ldx  <NMI_SVX
	lda  #0
	sta  <NMI_FLG
	lda  <NMI_SVA
	rti
```

Cycle accounting per step is the table above; `tests/bootrom/nmi_harness.py` measures the
real thing once the ROM assembles (the same harness already measures the tutorial NMI, see
01).

## Main loop

```
UR_MAIN_LOOP:
    jsr WAIT_VSYNC          ; SYS_TIMER changed by NMI
    jsr KEY_RTN             ; fix bank, pad 1, majority vote -> KEY_NEW
    jsr KEY_RTN2            ; main bank copy of the same for pad 2 -> KEY2_NEW
    jsr jobPICO             ; commands from the last mailbox (DMOD, fades)
    jsr PAL_FADE_SYSTEM
    jmp MAIN_LOOP
```

`KEY_NEW`/`KEY2_NEW` are what the next NMI sends. The read happens right after vblank, so the
state is at most one frame old when it is sent.

## Init (`UR_MAIN_SETUP`)

1. Rendering off; clear nametable 0 (tile 0 everywhere) and attribute table (0); write a
   16-entry black BG palette; scroll 0.
2. `SET_VRAM_ADD2 #$0800`; write `FP_COM_INI, 0`; wait (`PICO_COM_WAIT`); write
   `FP_COM_HELLO, 2`.
3. `$2000 <- %100_01_0_00` (NMI on, BG at `$0000`, sprites at `$1000`), `$2001 <- %000_11_11_0`
   (BG **and sprites** enabled -- the fetch pattern the count is calibrated for), `DISP_ON`.
4. Fall into the main loop. The screen stays black until the cartridge starts streaming; the
   firmware shows a "FC PICO DOOM / LOADING" pattern until the engine's first frame.

Optionally (Phase 5) the boot ROM draws its own "loading" text using the fix-bank font before
step 3, so the console is never blank.

## Data mode

`xPF_COM_DMOD` and the `FP_COM_DRQ`/`FP_COM_DLD` loop are copied from `SysPico.asm` unchanged,
including the attribute-table dummy-read quirk. The firmware uses data mode only at init (full
palette + attribute upload) and when the user resets the game.

## Building on Linux/CI

`nesasm.exe` is a 32-bit Windows PE. Options, in order of preference:

1. **Wine** (`wine32` on Ubuntu 24.04: `dpkg --add-architecture i386 && apt install wine32:i386`).
   Bit-exact with what the vendor ships. `doom/bootrom/build.sh` wraps it; CI proves the
   toolchain by re-assembling the tutorial ROM with `dbdate.h` pinned and checking
   `rom.NES` MD5 `B6CD675342B6C8AD79E537E2C9860579` (`docs/pages/build-pipeline.md`).
2. NESASM_X86 2.51+autozp rebuilt from source (the "minachun" distribution) as a native Linux
   binary, accepted only if it reproduces the same MD5.
3. Porting the sources to `ca65` -- last resort; the macro dialect (`TBL_JUMP`, `\@` locals,
   `.if`) makes this a real port.

`bin_catcut`, `binlink`, `Bin2C` are replaced by `tools/nes/bincut.py`, `tools/respack.py`,
`tools/bin2c.py`, each checked against the tutorial's committed outputs.

## Tests

- **Reproducibility**: assembling `doom/bootrom` twice gives identical `doom.nes`; the fix bank
  region equals `bootrom_fixr.bin`; the stamp equals `version.inc`.
- **NMI cycle count** (`tests/bootrom/test_nmi_cycles.py`, `py65`): load `doom.nes`, install a
  fake `$2007` that serves a 128-byte mailbox with all flags set and 16 APU pairs, run `NMI` at
  `$ED00` until `rti`, assert `mpu.processorCycles` for the critical section (up to and
  including the `$2005` writes -- detect by the second `$2005` store) <= 1900 and the total
  <= 2200.
- **Register order**: same harness asserts the sequence of `$2006` writes and that no
  `$2006`/`$2007` access follows the controller packet.
- **Co-simulation** (Mesen2, 09): the NMI's `rti` scanline is < 261 in every frame of a
  3000-frame demo run; VRAM `$23C0`-`$23FF` and `$3F00`-`$3F0F` equal the mailbox the model sent
  for that frame.
- **Reflash**: co-sim boots the *tutorial* ROM against the Doom firmware model and reaches the
  Doom ROM's `UR_MAIN_SETUP` (address breakpoint) within 60 s of emulated time.
