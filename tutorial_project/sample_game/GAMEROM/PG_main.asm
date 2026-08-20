
	.list			; リスティングファイル出力
	.mlist			; リスティングファイル上でマクロを展開

	.INCLUDE	"defDebug.h"

        .inesprg 2		; プログラムバンク数
        .ineschr 1		; CHR バンク数
        .inesmir 1		; 0:V 垂直２画面（ 水平ミラー） 1:H 水平２画面（垂直ミラー）
        .inesmap 0		; mapper #0

	.INCLUDE	"defMacro.h"

	.INCLUDE	"defRAM.h"
	.INCLUDE	"defGame.h"
	.INCLUDE	"defMission.h"
	.INCLUDE	".\chr\pallet.h"


	.code

	;========================================
	; ゲームバンク0
	;========================================
	.BANK		0
	ORG  $8000

	.INCBIN		"mml\sound.bin"				;4090 bytes
	.INCLUDE	".\SysSound.asm"

	.BANK		1
     ORG      $A000





	.BANK		2
      ORG     $C000

	.INCLUDE	".\cfg\cfgGame.h"
	.INCLUDE	".\cfg\cfgEnemyNT.h"
	.INCLUDE	".\cfg\cfgStage.h"
	.INCLUDE	".\cfg\cfgMissonSP.h"		; サブルーチン
	.INCLUDE	".\cfg\cfgMissonHara.h"
	.INCLUDE	".\cfg\cfgMissonAnime.h"


	.INCLUDE	"AplTitle.asm"
	.INCLUDE	"AplMiHara.asm"	;
	.INCLUDE	"AplLicense.asm"
	.INCLUDE	"AplClear.asm"
	.INCLUDE	"AplOver.asm"

	.INCLUDE	"AplGameDisp.asm"
	.INCLUDE	"AplGame.asm"		;ゲーム本体
	.INCLUDE	"AplGameSub.asm"

	.INCLUDE	"AplEnemy.asm"
	.INCLUDE	"AplMissionFunc.asm"	;
	.INCLUDE	"AplMission.asm"	;


	.BANK		3
      ORG     $E000
;-----------------------------
;  $E000
;  ゲーム変数初期化　FC PICO用
; 
;-----------------------------
jvcFCP_GAME_INIT:
	jsr  FCP_GAME_INIT
	brk
;-----------------------------
;  $E004
;  ゲーム本体処理　FC PICO用
; 
;-----------------------------
jvcFCP_GAME_MAIN:
	jsr  FCP_GAME_MAIN
	brk



FCP_GAME_INIT:
	lda  #0
	sta  <DEMO_FG

	lda  #ST_MAIN        ;プレイ画面へ
	STA	<STG_COD

	LDA	#0
	sta <NMI_CALL_ADR+1
	sta <NMI_CALL_BNK
	STA	<STG_COD_SUB
	STA	<KEY_NEW
	STA	<KEY_TRG

	ldx  #low( CLEAR_300W_TOP )
	lda  #0
.loop
	sta  $300,x
	inx
	bne  .loop

	jsr  PLY_LIFE_SET
	lda #POS_PLY_X_INIT
	sta POS_PLY_X
	lda #POS_PLY_Y_INIT
	sta POS_PLY_Y

	lda  #PLY_AN_WAIT
	jsr  SET_PLY_ANM

	jsr initMission
	rts


FCP_GAME_MAIN:
	jsr updateMission
	jsr moveGameObj

	lda  PLY_ANM_NO
	cmp  #PLY_AN_DEAD
	bne  .plydm_20

	jsr  setPlayerDeadEffect


.plydm_20
	jsr  MAKE_RND
	incw  <SYS_TIMER
	inc <FLM_TIMER
	rts

	.INCLUDE	"AplGameMove.asm"
	.INCLUDE	"AplGameMovePly.asm"
	.INCLUDE	"AplGameInit.asm"
	.INCLUDE	"AplBgStar.asm"
	.INCLUDE	"AplSelSub.asm"


PLY_MAIN_S:
PLY_MAIN:
	inc  <FLM_TIMER
	lda  <STG_COD	;
	TBL_JUMP
	JPTBL	JMP_NEXT_STG	; 0
	JPTBL	JMP_NEXT_STG2	; 1
	JPTBL	TIT_STG			; 2
	JPTBL	JMP_NEXT_STG	; 3
	JPTBL	JMP_NEXT_STG	; 4
	JPTBL	APL_GAME		; 5
	JPTBL	CLEAR_STG		; 6
	JPTBL	OVER_STG		; 7
	JPTBL	APL_LICENSE		; 8


JMP_NEXT_STG2:

JMP_NEXT_STG:
	INC	<STG_COD
JMP_RTS:
	RTS



	.INCLUDE	".\SysKey.asm"
	.INCLUDE	".\SysPallet.asm"
	.INCLUDE	".\SysData.asm"

	.INCLUDE	".\SysSub.asm"
	.include	".\SysNES.asm"
	.include	".\SysINIT.asm"



; HIRQ割り込みエントリ
IRQ_ENTRY:
	rti


	ORG	$FFFA
	DW	NMI
	DW	INIT
	DW	IRQ_ENTRY


;========================================
; CHR-ROM 0x0000 - 0x1FFF
;========================================
	.BANK		4
       ORG     $0000
	.INCBIN		"chr\font.chr"
	.INCBIN		"chr\OBJ_SP.chr"

