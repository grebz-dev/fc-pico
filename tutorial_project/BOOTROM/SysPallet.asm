;========================================
; Pallet System
;========================================
DEF_FADE_SPD	equ 4	; デフォルトフェード速度


;*****************************************
;黒フェードイン
;*****************************************
SET_FADE_IN_B:
	lda	#DEF_FADE_SPD
SET_FADE_IN_B2:
	ldy	#-$40
	ldx	#$10
	bne fade_set_end
;*****************************************
;黒フェードアウト
;*****************************************
SET_FADE_OUT_B:
	lda	#DEF_FADE_SPD
SET_FADE_OUT_B2:
	ldy	#0
	ldx	#-$10
	bne fade_set_end
;*****************************************
;白フェードイン
;*****************************************
SET_FADE_IN_W:
	lda	#DEF_FADE_SPD
SET_FADE_IN_W2:
	ldy	#$40
	ldx	#-$10
	bne fade_set_end
;*****************************************
;白フェードアウト
;*****************************************
SET_FADE_OUT_W:
	lda	#DEF_FADE_SPD
SET_FADE_OUT_W2:
	ldy	#0
	ldx	#$10
fade_set_end:
	sta PALFADE_TIME
	sta	PALFADE_CNT
	stx	PALFADE_ADD
	sty	PALFADE_VAL
	PAL_CHG
	rts

;*****************************************
;フェード終了待ち
;*****************************************
WAIT_FADE_END:
	lda  <FLG_2000
	sta	 $2000				; このタイミングでNMI発生
.loop
	jsr  WAIT_VSYNC
	jsr  PAL_FADE_SYSTEM
	lda	 PALFADE_TIME
	bne  .loop
	jmp  WAIT_VSYNC


;*****************************************
;パレットフェードシステム
;*****************************************
	
PAL_FADE_SYSTEM:
	lda	PALFADE_TIME
	beq	.ret		; フェードタイムが０なら何もしないでリターン
	dec	PALFADE_CNT
	bne	.ret		; カウントダウン中ならリターン
	sta	PALFADE_CNT

	PAL_CHG
	lda	PALFADE_ADD
	clc
	adc	PALFADE_VAL
	sta	PALFADE_VAL
	beq	.end_fade
	cmp #$50
	beq	.end_fade
	cmp #-$50
	beq	.end_fade
.ret
	rts

.end_fade
	lda	#0
	sta	PALFADE_TIME
;	sta	PALFADE_MASK
	rts

;*****************************************
;パレットＰＰＵ転送システム
;*****************************************
transPALLET:
	lda  <PAL_CHG_FG
	beq  .end

	ldx  #0
	stx  <PAL_CHG_FG
	lda  #$3F
	sta  $2006	; hi
	stx  $2006	; low
	
	lda  PALFADE_VAL
	BNE  .fadepal00
	; ダイレクト転送
.loop
	lda  PAL_WRK,x
	sta  $2007
	inx
	cpx  #32
	bne  .loop
.end
	rts

	; フェード中転送
.fadepal00
	bmi  .fadepal01

.palwcre020		; 加算転送 （白フェード用）
	ldy  #0
.loop_w0
	lda  tblFadeMask,y
	and  PALFADE_MASK
	jsr  sub_palwcre
	iny
	cpy  #8
	bne  .loop_w0
	rts



.fadepal01	; 減算転送 （黒フェード用）
	ldy  #0
.loop_b0
	lda  tblFadeMask,y
	and  PALFADE_MASK
	jsr  sub_palbcre
	iny
	cpy  #8
.	bne  .loop_b0
	rts


tblFadeMask:
	db  $01,$02,$04,$08,$10,$20,$40,$80

;--------------------
; 白フェードサブ
;--------------------
sub_palwcre
	bne  sub_paldirect
.loop
	lda	PAL_WRK,x
	cmp	#$0F		; $0Fは特殊扱い
	bne	.palwcre030
	lda	#$F0
.palwcre030
	; 白以上なら白にする
	clc
	adc	PALFADE_VAL
	cmp	#$40
	bcc	.palwcre040
	lda	#$30
.palwcre040
	sta  $2007
	inx
	txa
	and  #$03
	bne  .loop
	rts

;--------------------
; 黒フェードサブ
;--------------------
sub_palbcre
	bne  sub_paldirect

.loop
    lda  PAL_WRK,X
	cmp  #$0F		; $0Fは特殊扱いで何もしない
	beq  .palbcre040
.palbcre030
	; 黒以下なら黒にする
	clc
	adc  PALFADE_VAL
	bpl  .palbcre040
	lda  #$0F
.palbcre040
	sta  $2007
	inx
	txa
	and  #$03
	bne  .loop
	rts

;--------------------
; ダイレクト転送
;--------------------
sub_paldirect
.loop
	lda	PAL_WRK,x
	sta  $2007
	inx
	txa
	and  #$03
	bne  .loop
	rts


