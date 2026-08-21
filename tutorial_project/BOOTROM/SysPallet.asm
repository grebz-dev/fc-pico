;/// @file SysPallet.asm
;/// @brief Palette fade engine and the only writer of the hardware palette.
;/// @ingroup bootrom
;///
;/// All palette changes go through the #PAL_WRK shadow; @ref transPALLET is what
;/// moves them to `$3F00`, and it must run inside vertical blank.
;///
;/// Fades are computed per entry against a mask, so groups of colours can be
;/// faded independently. Palette index `$0F` is special-cased as black.
;========================================
; Pallet System
;========================================
DEF_FADE_SPD	equ 4   ;///< Default frames between fade steps. ; デフォルトフェード速度


;*****************************************
;黒フェードイン
;*****************************************
;/// @brief Starts a fade in from black.
;/// @ingroup bootrom
SET_FADE_IN_B:
	lda	#DEF_FADE_SPD
;/// @brief As @ref SET_FADE_IN_B, with a caller-supplied speed.
;/// @ingroup bootrom
SET_FADE_IN_B2:
	ldy	#-$40
	ldx	#$10
	bne fade_set_end
;*****************************************
;黒フェードアウト
;*****************************************
;/// @brief Starts a fade out to black.
;/// @ingroup bootrom
SET_FADE_OUT_B:
	lda	#DEF_FADE_SPD
;/// @brief As @ref SET_FADE_OUT_B, with a caller-supplied speed.
;/// @ingroup bootrom
SET_FADE_OUT_B2:
	ldy	#0
	ldx	#-$10
	bne fade_set_end
;*****************************************
;白フェードイン
;*****************************************
;/// @brief Starts a fade in from white.
;/// @ingroup bootrom
SET_FADE_IN_W:
	lda	#DEF_FADE_SPD
;/// @brief As @ref SET_FADE_IN_W, with a caller-supplied speed.
;/// @ingroup bootrom
SET_FADE_IN_W2:
	ldy	#$40
	ldx	#-$10
	bne fade_set_end
;*****************************************
;白フェードアウト
;*****************************************
;/// @brief Starts a fade out to white.
;/// @ingroup bootrom
SET_FADE_OUT_W:
	lda	#DEF_FADE_SPD
;/// @brief As @ref SET_FADE_OUT_W, with a caller-supplied speed.
;/// @ingroup bootrom
SET_FADE_OUT_W2:
	ldy	#0
	ldx	#$10
;/// @brief Common tail of the fade setters: stores the parameters and flags the palette dirty.
;/// @ingroup bootrom
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
;/// @brief Blocks until the running fade completes, re-enabling NMI first.
;/// @ingroup bootrom
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
	
;/// @brief Advances the fade by one step; called once per frame from the main loop.
;/// @ingroup bootrom
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
;/// @brief Uploads #PAL_WRK to `$3F00`, applying the fade. @warning Must run inside vertical blank.
;/// @ingroup bootrom
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


;/// @brief Bit masks selecting which palette group each fade step affects.
;/// @ingroup bootrom
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


