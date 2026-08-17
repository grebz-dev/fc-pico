;/// @file PG_main.asm
;/// @brief Top-level assembly unit for the erasable boot ROM bank.
;/// @ingroup bootrom
;///
;/// Declares the iNES header, lays out the four 8 KB banks, includes every other
;/// source, and publishes the fixed entry points the permanent fix bank calls
;/// into. The prebuilt fix bank is appended here with @c .incbin.
;///
;/// Cartridge type: **mapper 0 (NROM-256)**, 32 KB PRG, **no CHR-ROM** -- the
;/// RP2350 occupies the CHR role. @see @ref hardware
;///
;/// @warning The entry addresses below are hard-coded in `BOOTROM_FIX/PG_main.asm`
;///          as well. Moving one means editing both. @see @ref boot_reflash

	.list			; リスティングファイル出力
	.mlist			; リスティングファイル上でマクロを展開

	.INCLUDE	"defDebug.h"

        .inesprg 2				; プログラムバンク数
        .ineschr 0				; CHR バンク数
        .inesmir 1				; BGラーリング
        .inesmap 0				; mapper #0


ROM_NMI_ENTRY	EQU $ED00   ;///< Fixed NMI trampoline address, `$ED00`. 
ROM_IRQ_ENTRY	EQU $EE80   ;///< Fixed IRQ trampoline address, `$EE80`. 

INIT			EQU $F000   ;///< Permanent bank entry: cold boot. 
TRANS_SYS_FONT	EQU $F003   ;///< Permanent bank entry: install the system font. 
KEY_RTN			EQU $F006   ;///< Permanent bank entry: read the controller. 
BEEP_PI			EQU $F009   ;///< Permanent bank entry: high beep. 
BEEP_PO			EQU $F00C   ;///< Permanent bank entry: low beep. 



	.INCLUDE	"SysEqu.h"
	.INCLUDE	"macro.h"
	.INCLUDE	"defGame.h"

	.code


	;========================================
	; ゲームバンク $00
	;========================================
	.BANK		0
       ORG     $8000


	.BANK		1
       ORG     $A000



	.BANK		2
      ORG     $C000

	.INCLUDE	"AplGame.asm"		;ゲーム本体

;	.INCLUDE	"SysVRAMT.asm"
	.include	"SysNES.asm"
	.INCLUDE	"SysData.asm"
	.INCLUDE	"SysPallet.asm"
	.INCLUDE	"SysSub.asm"

;/// @brief Per-frame application dispatch: advances the frame timer, then jumps via #STG_COD.
;/// @ingroup bootrom
PLY_MAIN_S:
;/// @brief Jump-table body of @ref PLY_MAIN_S.
;/// @ingroup bootrom
PLY_MAIN:
	inc  <FLM_TIMER

	lda  <STG_COD	;
	TBL_JUMP
	JPTBL	JMP_NEXT_STG	; 0
	JPTBL	JMP_NEXT_STG	; 1
	JPTBL	APL_GAME		; 2
	JPTBL	JMP_RTS			; 4
	JPTBL	JMP_RTS			; 5
	JPTBL	JMP_RTS			; 6
	JPTBL	JMP_RTS			; 7


;/// @brief Step handler that simply advances to the next step.
;/// @ingroup bootrom
JMP_NEXT_STG:
	INC	<STG_COD
;/// @brief Step handler that does nothing.
;/// @ingroup bootrom
JMP_RTS:
	RTS


	.BANK		3
       ORG     $E000


	.INCLUDE	"SysArduino.asm"
	.include	"SysPico.asm"
	.include	"SysNMI.asm"

;-------------------------------------------------------------------------------
; NMI割り込みエントリ
;-------------------------------------------------------------------------------
	ORG     ROM_NMI_ENTRY
	jmp  NMI
;-------------------------------------------------------------------------------
; HIRQ割り込みエントリ
;-------------------------------------------------------------------------------
	ORG     ROM_IRQ_ENTRY
;/// @brief IRQ trampoline at `$EE80`. Returns immediately; IRQs are unused on this cartridge.
;/// @ingroup bootrom
IRQ_ENTRY:
	rti


	ORG     $EF00
;/// @brief Fixed entry at `$EF00`, called once by the permanent bank after boot.
;/// @ingroup bootrom
MAIN_SETUP:
	jmp  UR_MAIN_SETUP
;/// @brief Fixed entry at `$EF03`, the application's endless loop.
;/// @ingroup bootrom
MAIN_LOOP:
	jmp  UR_MAIN_LOOP

	ORG     $EFF0
;/// @brief Build stamp at `$EFF0`, compared against the cartridge's copy. @see CHK_ROMVER
;/// @ingroup bootrom
DB_ROM_VER:
	.INCLUDE	"dbdate.h"

	ORG     $EFFF
;/// @brief Erase-in-progress marker at `$EFFF`; non-zero means a reflash was interrupted.
;/// @ingroup bootrom
IS_ROM_ERACE:
	db  0			; ROMが消去されていたら $FFが格納されている
;	db  0xff		; ROMが消去されていたら $FFが格納されている

       ORG     $F000

 	.incbin		"bootrom_fixr.bin"


