
UR_MAIN_SETUP:

	;===============================
	;	サウンド・乱数初期化
	;===============================
;@	jsr  EXS_RESET		; 拡張システムリセット

;@	jsr  INIT_SOUND

;@	jsr  VRAMT_INIT	; VRAM転送システム初期化

	;===============================
	;	FC-EXA 初期化
	;===============================
	jsr  BOOT_EXA


	rts

;=====================================================

UR_MAIN_LOOP:
	jsr WAIT_VSYNC
	JOB_TIMEOUT
;@	JSR	KEY_RTN2	;--- キー入力チェック -----
	JSR	KEY_RTN		;--- キー入力チェック -----

;	sei
;	lda  <HIRQ_ENA
;	beq  .no_irq
;	cli			; 割り込み解除
;.no_irq

	JSR	PLY_MAIN_S	; メイン処理呼び出し

	jsr  PAL_FADE_SYSTEM
	jmp  MAIN_LOOP


;--------------------
; VSYNC 待ち
;--------------------
WAIT_VSYNC:
	lda  <SYS_TIMER
.loop:
	cmp  <SYS_TIMER
	beq  .loop		; NMI終了待ち

	rts

;==============================================================================
;
;						起動時メモリーテストシステム
;
;==============================================================================
;===============================
; RAMエラー検出
;===============================
chk_ram_sub:
	ldy  #0
.chk_ram_s00:
	lda  #$FF
	sta  [SRC_ADR],y
	cmp  [SRC_ADR],y
	bne  .ram_error
;	beq  .ram_error

	lda  #$00
	sta  [SRC_ADR],y
	cmp  [SRC_ADR],y
	bne  .ram_error
	iny
	bne  .chk_ram_s00

	inc  <SRC_ADR+1
	rts

.ram_error
	DEBUG_HALT
	rts




;==============================================================================
;
;						起動時EXAチェックシステム
;
;==============================================================================

BOOT_EXA:
	ldx  #1
	jsr  WAIT_VBLANK_X_SD

	jsr  EXS_RESET		; 拡張システムリセット

	; 拡張RAM存在チェック
	lda  $6000
	tax
	eor  #$FF
	sta  $6000
	cmp  $6000
	bne  .noexa00
	stx  $6000

	RSTAT_EXA
	cmp  #STAT_NOEXA
	bne  .exa00
	; EXA無し
.noexa00
 	inc  <EXA_MODE
	rts
.exa00

;@	jsr STOP_SE
;@	jsr  STOP_BGM

;	lda  #%000_01_0_00		; NO-NMI
;	sta	 $2000
;	lda  #%000_01_110
;	sta	 $2001

	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*5 + 1
	DRAW_STRING2 "FC-EXA BOOT"
	jsr  VBLANK_END_SD

	;---- STAT_BOOT検出 ------------------------
.boot_exa01
	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*7 + 1
	RSTAT_EXA
	pha
	jsr  DRAW_HEX_BYTE

	DRAW_STRING2 ">STAT_BOOT"
	jsr  VBLANK_END_SD

	pla
	cmp  #STAT_BOOT
	bne  .boot_exa01

;@	lda  #SE_CUR_SEL
;@	jsr  PLAY_SE

	;---- STAT_SEALED検出 ------------------------
.boot_exa02
	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*8 + 1
	RSTAT_EXA
	pha
	jsr  DRAW_HEX_BYTE

	DRAW_STRING2 ">STAT_SEALED"
	jsr  VBLANK_END_SD

	pla
	cmp  #STAT_SEALED
	bne  .boot_exa02

	;---- 封印解除コマンド送信 ------------------------
	ldx  #1
	jsr  WAIT_VBLANK_X_SD

;@	lda  #SE_CUR_SEL
;@	jsr  PLAY_SE

	jsr  EXS_INIT

	;---- 封印解除待ち ------------------------
.boot_exa03
	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*9 + 1
	RSTAT_EXA
	pha
	jsr  DRAW_HEX_BYTE

	DRAW_STRING2 ">STAT_WAIT"
	jsr  VBLANK_END_SD

	pla
	cmp  #STAT_WAIT
	bne  .boot_exa03

;@	lda  #SE_CUR_SEL
;@	jsr  PLAY_SE
	
	ldx  #10
	jsr  WAIT_VBLANK_X_SD

	;---- 拡張RAMテスト ------------------------
	jsr  KEY_RTN
	CHK_BIT	<KEY_NEW, #KEY_SEL
	beq  .skip_exramtest 

	jsr  exram_sub
	bcs  .error_exram

	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*12 + 1

	DRAW_STRING2 "EXRAM_TEST OK"
	jsr  VBLANK_END_SD

	ldx  #180
	jsr  WAIT_VBLANK_X_SD
	
	jmp  .skip_exramtest

.error_exram
	jsr  VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*12 + 1

	DRAW_STRING2 "EXRAM_TEST NG"
	jsr  VBLANK_END_SD

	ldx  #180
	jsr  WAIT_VBLANK_X_SD



.skip_exramtest
	lda  #0
	sta	 $2000
	sta	 $2001
	ldx  #1
	jsr  WAIT_VBLANK_X_SD
	rts

exram_sub
	;---------------------------
	; RAMバンク切り替えテスト
	;---------------------------
	ldx  #0
.loop
	stx  <TMP_SVX
	txa
	jsr  exram_sub2
	bcs  .error
	ldx  <TMP_SVX
	inx
	cpx  #4
	bne  .loop

	;---------------------------
	; RAMバンク RAM独立性テスト
	;---------------------------
	ldx  #0
.loop2
	stx  <TMP_SVX
	txa
	jsr  exram_sub3
	bcs  .error
	ldx  <TMP_SVX
	inx
	cpx  #4
	bne  .loop2

	
	clc
	rts
.error
	rts

exram_sub2
	jsr  setWRAM_BANK
	bcs  .end
	ldx  <TMP_SVX
	lda  $6000
	sta  GM_TMP0,x
	stx  $6000
.end
	rts

exram_sub3
	jsr  setWRAM_BANK
	bcs  .end
	ldx  <TMP_SVX
	cpx  $6000
	bne  .end
	lda  GM_TMP0,x
	sta  $6000

	ldy  #$60
.loop
	jsr  .MEMCHK_SUB
	cpy  #$80
	beq  .loop

	clc
	rts
.end
	sec
	rts


;--------------------------------
; メモリーチェック文字列セット
; Y reg = チェックアドレスインデックス
;--------------------------------
.MEMCHK_SUB:
	tya
	pha
	sta  <DST_ADR+1
	ldy  #0
	sty  <DST_ADR+0
.loop_nozp
	lda  [DST_ADR],y
	eor  #$FF
	sta  [DST_ADR],y
	cmp  [DST_ADR],y
	bne  .MEMCHK_ERROR
	eor  #$FF
	sta  [DST_ADR],y
	cmp  [DST_ADR],y
	bne  .MEMCHK_ERROR

	iny
	bne  .loop_nozp
;.skip
	pla
	tay
	rts


.MEMCHK_ERROR:
	SET_VRAM_ADD2	#$2000 + 32*12 + 14
	lda  <DST_ADR+1
	cmp  #$40
	bcc  .mce_00

	SET_VRAM_ADD2	#$2000 + 32*12 + 23-1
.mce_00
	
	lda  #'N'
	sta  $2007
	lda  #'G'
	sta  $2007
	BEEP $104,%11110011
.loop_me
	lda  #0
	jsr  WAIT_VBLANK_X

	jmp  .loop_me




;==============================================================================
;
;					NMI禁止型　画面表示システム ※拡張
;
;==============================================================================

;--------------------------------
; Xレジで指定フレームウェイト　サウンド処理あり
;--------------------------------
WAIT_VBLANK_X_SD:
	jsr  VBLANK_START

	jsr  VBLANK_END_SD

	dex
	bne  WAIT_VBLANK_X_SD
	rts


VBLANK_END_SD:
	RESET_SCR_XY

	phxy
 .if 1
;@	PUSH_BANK
;@	CHG_BANK_LB SOUND_SYSTEM
;@	jsr  SOUND_SYSTEM
;@	POP_BANK
 .endif

	WAIT_VBLANK_END

	
	plxy
	rts

 .if 0
VBLANK_START2:
	WAIT_VBLANK_END
	lda  #%000_01_0_00		; NO-NMI
	sta	 $2000
	lda  #%000_01_110
	sta	 $2001
	WAIT_VBLANK_END

 .endif
 
;--------------------------------
; Xレジで指定フレームウェイト
;--------------------------------
WAIT_VBLANK_X:
	jsr  VBLANK_START

	jsr  VBLANK_END

	dex
	bne  WAIT_VBLANK_X
	rts


VBLANK_END:
	RESET_SCR_XY
	WAIT_VBLANK_END
	rts


VBLANK_START:
	jsr PAL_FADE_SYSTEM

	incw  <SYS_TIMER
	jsr   KEY_RTN		;--- キー入力チェック -----
	WAIT_VBLANK

;--- スプライトDMA転送 ----- （※512 clock消費）
	lda  #2			;ここに必要
	sta  $4014		;ここに必要

	jsr  transPALLET
	
	rts



