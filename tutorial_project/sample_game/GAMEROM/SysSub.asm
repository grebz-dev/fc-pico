;===================================================================
;
;			固定バンクに置く汎用ルーチン
;
;===================================================================
;--------------------------------
; STG_COD セット  A reg -> STG_COD
;--------------------------------
SET_STG_COD:
SET_STG_COD2:
	STA	<STG_COD

;	JSR	STOP_SE

	sei
	inc	<NMI_FLG	;ハング防止

	lda  #0
	sta  <NMI_CALL_ADR+1
	sta  <NMI_CALL_BNK
	sta  <STG_COD_SUB
	sta  <KEY_NEW
	sta  <KEY_TRG
	dec  <NMI_FLG

	DISP_OFF		; 画面off

	rts



;******* GM_WAIT **********************
ST_GM_WKEY:
	CHK_BIT	<KEY_TRG, #KEY_ABRS
	bne  st_gm_w01

ST_GM_WAIT:
	dec <GM_WAIT
	bne  st_gm_w00

st_gm_w01:
	inc  <STG_COD_SUB
st_gm_w00:
	rts

ST_GM_WAIT2:
	jsr  SLOW_DEC_GM_WAIT
	beq  st_gm_w01
	rts


;******* フェード終了待ち **********************
ST_FADE_WAIT:
	lda  PALFADE_TIME
	bne  st_gm_w00
	inc  <STG_COD_SUB
	rts

;-------------------------------------
; 8フレーム毎にカウントダウンする GM_WAIT
; カウントがゼロならゼロフラグセット
;-------------------------------------
SLOW_DEC_GM_WAIT:
	lda  <SYS_TIMER
	and  #$07
	bne  .endstg52
	dec  <GM_WAIT
.endstg52:
	rts


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



;-----------------------------------
; 余ったスプライトをクリアーする
; y reg = スプライトの開始位置
;-----------------------------------
clearObj:
	cpy  #0
	beq  clearObj_end
clearObj2:
	lda  #SP_CLR_Y
.spclr_loop
	sta  OBJ_BUF,y
	iny
	iny
	iny
	iny
	bne .spclr_loop
clearObj_end:
	rts



;=======================
; 文字列描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   DRAW_STRING で文字列の格納アドレスを指定
;=======================
DRAW_STRING_SUB:
	sta  <SRC_ADR+1
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
	ldy  #0
.crst00:
	lda  [SRC_ADR],Y
	beq  .crst01
	lda  #0
	sta  $2007
	iny
	bne  .crst00
.crst01:
	rts


;=======================
; 16進数　数字描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   A reg 描画する数値
;=======================
DRAW_HEX_BYTE:
	tay
	lsr  a
	lsr  a
	lsr  a
	lsr  a
	jsr  DRAW_HEX_BYTE2
	tya
DRAW_HEX_BYTE2:
	and  #$0f
	cmp  #10
	bcs  .drhx00
	clc
	adc  #'0'
	jmp  .drhx01
.drhx00:
	clc
	adc  #'A' -10
.drhx01:
	sta  $2007
	rts

;=======================
; 16進数　数字描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   A reg 描画する数値
;=======================
DRAW_HEX_BYTE_GM:
	tay
	lsr  a
	lsr  a
	lsr  a
	lsr  a
	jsr  DRAW_HEX_BYTE2_GM
	tya
DRAW_HEX_BYTE2_GM:
	and  #$0f
	clc
	adc  #1
.drhx01_GM:
	sta  $2007
	rts

;=======================
; 2進化10進 8ビット加算
;   A reg 加算する値（BCD値）
;   X reg $1xx のワークの下位アドレス8bit
;=======================

BCD_ADD:
	; 加算する値を上下4ビットずつに分離
	tay
	and  #$0F
	sta  <TMP_SV0
	tya
	and  #$F0
	lsr  a
	lsr  a
	lsr  a
	lsr  a
	sta  <TMP_SV1
	
	lda  $100,x
	tay
	and  #$0F
	sta  <TMP_SV2
	tya
	and  #$F0
	lsr  a
	lsr  a
	lsr  a
	lsr  a
	sta  <TMP_SV3

	; 下位4ビットを加算
	lda  <TMP_SV0
	clc
	adc  <TMP_SV2
	cmp  #10
	bcc  BCD_00
	sbc  #10
	inc  <TMP_SV1
BCD_00:
	sta  <TMP_SV2

	; 上位4ビットを加算
	lda  <TMP_SV1
	clc
	adc  <TMP_SV3
	cmp  #10
	bcc  BCD_01
	sbc  #10
	jsr  BCD_01
	inx
	lda  #1
	jmp  BCD_ADD

BCD_01:
	asl  a
	asl  a
	asl  a
	asl  a
	ora  <TMP_SV2
	sta  $100,x
	rts


;=======================
; 2進化10進 インクリメント
;   X reg $1xx のワークの下位アドレス8bit
;=======================
BCD_INC:
	lda  $100,x
	and  #$0F
	cmp  #9
	beq  .BCD_I00
	inc  $100,x
	rts
.BCD_I00:
	lda  $100,x
	and  #$F0
	cmp  #$90
	beq  .BCD_I01
	clc
	adc  #$10
	sta  $100,x
	rts
.BCD_I01:
	lda  #$0
	sta  $100,x
	inx
	jmp  BCD_INC


;=======================
; 2進化10進 デクリメント
;   X reg $1xx のワークの下位アドレス8bit
;=======================
BCD_DEC:
	lda  $100,x
	and  #$0F
	beq  .BCD_D00
	dec  $100,x
	rts
.BCD_D00:
	lda  $100,x
	and  #$F0
	beq  .BCD_D01
	sec
	sbc  #$10
	clc
	adc  #$09
	sta  $100,x
	rts
.BCD_D01:
	lda  #$99
	sta  $100,x
	inx
	jmp  BCD_DEC

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
; SPRITE   CLEAR       *
;=======================
SYS_CLEAR_SP:
SPT_CLR_RTN:
	ldx #0
	lda #$F1
.loop:
	sta  $0200,x
	sta  $0201,x
	inx
	inx
	inx
	inx
	bne  .loop
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
	lda  #0
	sta  <NMI_FLG	; 割り込み許可

	jsr  WAIT_VSYNC	; IRQを働かせるため、次フレームになるまで待つ。

	lda  #FLG_PPU2001
	sta  <FLG_2001	; 画面ON
	rts

;------------------------------------------------------------------------------
;				表示off
;------------------------------------------------------------------------------
_disp_off_sub:
	lda  #0
	sta  <FLG_2001	; 画面OFF
	jsr  WAIT_VSYNC	; 画面offになるのは次のフレームからなので、待つ。

	lda  #1
	sta  <NMI_FLG	; 割り込み禁止
	rts



;--------------------
; VSYNC 待ち
;--------------------
WAIT_VSYNC:
	lda  <FLG_2000
	beq  .end
	lda  <SYS_TIMER
.loop:
	cmp  <SYS_TIMER
	beq  .loop		; NMI終了待ち
.end
	rts

;========================================
; Random System
;========================================

;*****************************************
; Areg に 0-255の擬似乱数を返す
;*****************************************
GET_RND:
	stx  <TMP_SVX
	lda  RND_SEL
	and  #3
	inc  RND_SEL
	tax
	lda  RND_WK0,X
	adc  #155
	sta  RND_WK0,X
	ldx  <TMP_SVX
	rts


;*****************************************
; Areg に 乱数の最大値セット
;
; Areg に 指定した数値未満の擬似乱数を返す
;*****************************************
GET_RND_N:
	sta  <TMP_SVA
.get_rndn_00:
	jsr  GET_RND
	cmp  <TMP_SVA
	bcs  .get_rndn_00
	rts


