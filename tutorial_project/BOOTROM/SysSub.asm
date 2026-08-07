;===================================================================
;
;			固定バンクに置く汎用ルーチン
;
;===================================================================
;--------------------------------
; STG_COD セット  A reg -> STG_COD
;--------------------------------
SET_STG_COD:
;	pha
;	JSR	STOP_BGM
;	pla
SET_STG_COD2:
	STA	<STG_COD

;	JSR	STOP_SE

	sei
	inc	<NMI_FLG	;ハング防止

	LDA	#0
	sta <HIRQ_ENA

	sta <NMI_CALL_ADR+1
	sta <NMI_CALL_BNK
	STA	<STG_COD_SUB
	STA	<KEY_NEW
	STA	<KEY_TRG
	dec	<NMI_FLG

	DISP_OFF		; 画面off

	RTS



;******* GM_WAIT **********************
ST_GM_WKEY:
	CHK_BIT	<KEY_TRG, #KEY_ABRS
	BNE	st_gm_w01

ST_GM_WAIT:
	DEC	<GM_WAIT
	BNE	st_gm_w00

st_gm_w01:
	INC	<STG_COD_SUB
st_gm_w00:
	RTS

ST_GM_WAIT2:
	jsr  SLOW_DEC_GM_WAIT
	beq  st_gm_w01
	RTS


;******* フェード終了待ち **********************
ST_FADE_WAIT:
	lda PALFADE_TIME
	bne	st_gm_w00
	inc	<STG_COD_SUB
	rts

;-------------------------------------
; 8フレーム毎にカウントダウンする GM_WAIT
; カウントがゼロならゼロフラグセット
;-------------------------------------
SLOW_DEC_GM_WAIT:
	LDA	<SYS_TIMER
	AND	#$07
	BNE	.endstg52
	DEC	<GM_WAIT
.endstg52:
	RTS


;=======================
; [SCR_ADR] から１バイト A reg に入れてアドレスをインクリメント
;  IN: SRC_ADR 転送元アドレス 16bit
;  OUT: A reg  取得したデータ
;=======================
getSCR_ADR_DATA:
	sty  <TMP_SYS
	ldy  #0
	lda  [SRC_ADR],Y
	php
	ldy  <TMP_SYS
	incw <SRC_ADR
	plp
	rts

incSCR_ADR:
	incw <SRC_ADR
	rts

decSCR_ADR:
	decw <SRC_ADR
	rts


;----------------------
;  SRC_ADRに加算
;  Areg = LOW
;  Xreg = High
;----------------------
addSCR_ADR:
	clc
	adc  <SRC_ADR+0
	sta  <SRC_ADR+0
	txa
	adc  <SRC_ADR+1
	sta  <SRC_ADR+1
	rts


;=======================
; Areg を [DST_ADR] にセットしてアドレスをインクリメント
;  IN: DST_ADR 転送元アドレス 16bit
;  破壊 Y
;=======================
setDST_ADR_DATA:
	ldy   #0
	sta   [DST_ADR],Y
incDST_ADR:
	inc  <DST_ADR
	bne  .end
	inc  <DST_ADR+1
.end	
	rts




;=======================
; キャリーフラグ反転
;=======================
revCFlag:
	bcc  .set
	clc
	rts
.set
	sec
	rts



SYS_CLEAR_SP:
SPT_CLR_RTN:
	ldy  #0
;-----------------------------------
; 余ったスプライトをクリアーする
; y reg = スプライトの開始位置
;-----------------------------------
clearObj:
;	cpy #0
;	beq .end
	lda #SP_CLR_Y
.spclr_loop
	sta $200,y
	iny
	iny
	iny
	iny
	bne .spclr_loop
.end
	rts



;=======================
; 文字列描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   DRAW_STRING で文字列の格納アドレスを指定
;=======================
DRAW_STRING_SUB:
	sta  <SRC_ADR+1
DRAW_STRING_SUB2:
	ldy  #0
.drst00:
	lda  [SRC_ADR],Y
	beq  .drst01
	cmp  #' '
	bne  .set
	lda  #0
.set
	sta  $2007
	iny
	bne  .drst00
.drst01:
	rts


;=======================
; 文字列クリアー
;   SET_VRAM で転送先VRAMアドレスを指定
;   DRAW_STRING で文字列の格納アドレスを指定
;=======================
CLR_STRING_SUB:
	sta  <SRC_ADR+1
        LDY  #0
.crst00:
        LDA  [SRC_ADR],Y
	BEQ  .crst01
	LDA  #0
        STA  $2007
        INY
	BNE  .crst00
.crst01:
        RTS


;=======================
; 16進数　数字描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   A reg 描画する数値
;=======================
DRAW_HEX_BYTE:
        TAY
        LSR A
        LSR A
        LSR A
        LSR A
	JSR	DRAW_HEX_BYTE2
	TYA
DRAW_HEX_BYTE2:
	jsr  convHEX2
	sta  $2007
	rts

convHEX2:
	and  #$0f
	cmp  #10
	bcs  .drhx00
	clc
	adc  #'0'
	bne  .drhx01
.drhx00:
	clc
	adc #'A' -10
.drhx01:
	rts



;=======================
; 16進数　数字描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   A reg 描画する数値
;=======================
DRAW_HEX_BYTE_GM:
	TAY
	LSR A
	LSR A
	LSR A
	LSR A
	JSR	DRAW_HEX_BYTE2_GM
	TYA
DRAW_HEX_BYTE2_GM:
	AND #$0f
	clc
	ADC #1
.drhx01_GM:
    STA  $2007
    RTS

;=======================
; 2進化10進 8ビット加算
;   A reg 加算する値（BCD値）
;   X reg $3xx のワークの下位アドレス8bit
;=======================

BCD_ADD:
	; 加算する値を上下4ビットずつに分離
	TAY
	AND  #$0F
	STA  <TMP_SV0
	TYA
	AND  #$F0
	LSR  A
	LSR  A
	LSR  A
	LSR  A
	STA  <TMP_SV1
	
	LDA  $300,X
	TAY
	AND  #$0F
	STA  <TMP_SV2
	TYA
	AND  #$F0
	LSR  A
	LSR  A
	LSR  A
	LSR  A
	STA  <TMP_SV3

	; 下位4ビットを加算
	LDA  <TMP_SV0
	CLC
	ADC  <TMP_SV2
	CMP  #10
	BCC  BCD_00
	SBC  #10
	INC  <TMP_SV1
BCD_00:
	STA  <TMP_SV2

	; 上位4ビットを加算
	LDA  <TMP_SV1
	CLC
	ADC  <TMP_SV3
	CMP  #10
	BCC  BCD_01
	SBC  #10
	JSR  BCD_01
	INX
	LDA  #1
	JMP  BCD_ADD

BCD_01:
	ASL  A
	ASL  A
	ASL  A
	ASL  A
	ORA  <TMP_SV2
	STA  $300,X
	RTS


;=======================
; 2進化10進 インクリメント
;   X reg $3xx のワークの下位アドレス8bit
;=======================
BCD_INC:
	LDA  $300,X
	AND  #$0F
	CMP  #9
	BEQ  .BCD_I00
	INC  $300,X
	RTS
.BCD_I00:
	LDA  $300,X
	AND  #$F0
	CMP  #$90
	BEQ  .BCD_I01
	CLC
	ADC  #$10
	STA  $300,X
	RTS
.BCD_I01:
	LDA  #$0
	STA  $300,X
	INX
	JMP  BCD_INC


;=======================
; 2進化10進 デクリメント
;   X reg $3xx のワークの下位アドレス8bit
;=======================
BCD_DEC:
	LDA  $300,X
	AND  #$0F
	BEQ  .BCD_D00
	DEC  $300,X
	RTS
.BCD_D00:
	LDA  $300,X
	AND  #$F0
	BEQ  .BCD_D01
	SEC
	SBC  #$10
	CLC
	ADC  #$09
	STA  $300,X
	RTS
.BCD_D01:
	LDA  #$99
	STA  $300,X
	INX
	JMP  BCD_DEC


;---------------------------------------------
; Areg の値をBCDに変換:99以上の値は99になる
;---------------------------------------------
convBCD:
	cmp  #99
	bcc  .no_over
	lda  #$99
	rts

.no_over
	ldx  #0
.loop
	cmp  #10
	bcc  .end
	sbc  #10
	inx
	bne  .loop
.end
	sta  <TMP_SYS
	txa
	asl  a
	asl  a
	asl  a
	asl  a
	ora  <TMP_SYS
	rts


;=======================
; VRAM CLEAR
;=======================
SYS_CLEAR_BG:
	SET_VRAM_ADD2 #$2000
SYS_CLEAR_BG2:
	lda	#$00
	ldy	#0
	jsr SYS_VRAM_WLP
	jsr SYS_VRAM_WLP
	jsr SYS_VRAM_WLP

SYS_VRAM_WLP:
	sta  $2007
	dey
	bne  SYS_VRAM_WLP
	rts



;------------------------------------------------------------------------------
;				表示on
;------------------------------------------------------------------------------
_disp_on_sub:
	lda	#0
	sta	<NMI_FLG	; 割り込み許可

	JSR	WAIT_VSYNC	; IRQを働かせるため、次フレームになるまで待つ。

	lda  #FLG_PPU2001
	sta  <FLG_2001	; 画面ON
	lda  #1
	sta  <HIRQ_ENA
	rts

_disp_on_sub2:

	lda	#0
	sta	<NMI_FLG	; 割り込み許可

	JSR	WAIT_VSYNC	; IRQを働かせるため、次フレームになるまで待つ。

	lda  #%000_01_11_0
	sta  <FLG_2001	; 画面ON
	lda  #1
	sta  <HIRQ_ENA
	rts


;------------------------------------------------------------------------------
;				表示off
;------------------------------------------------------------------------------
_disp_off_sub:
	LDA	#0
	STA	<FLG_2001	; 画面OFF
	STA	<HIRQ_ENA
	JSR	WAIT_VSYNC	; 画面offになるのは次のフレームからなので、待つ。

	lda	#1
	sta	<NMI_FLG	; 割り込み禁止
	rts

