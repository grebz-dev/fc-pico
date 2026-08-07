
	.list			; リスティングファイル出力
	.mlist			; リスティングファイル上でマクロを展開

	.INCLUDE	"defDebug.h"

        .inesprg 2				; プログラムバンク数
        .ineschr 0				; CHR バンク数
        .inesmir 1				; BGラーリング
        .inesmap 0				; mapper #0


ROM_NMI_ENTRY	EQU $ED00
ROM_IRQ_ENTRY	EQU $EE80

INIT			EQU $F000
TRANS_SYS_FONT	EQU $F003
KEY_RTN			EQU $F006
BEEP_PI			EQU $F009
BEEP_PO			EQU $F00C



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

PLY_MAIN_S:
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


JMP_NEXT_STG:
	INC	<STG_COD
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
IRQ_ENTRY:
	rti


	ORG     $EF00
MAIN_SETUP:
	jmp  UR_MAIN_SETUP
MAIN_LOOP:
	jmp  UR_MAIN_LOOP

	ORG     $EFF0
DB_ROM_VER:
	.INCLUDE	"dbdate.h"

	ORG     $EFFF
IS_ROM_ERACE:
	db  0			; ROMが消去されていたら $FFが格納されている
;	db  0xff		; ROMが消去されていたら $FFが格納されている

       ORG     $F000

 	.incbin		"bootrom_fixr.bin"


