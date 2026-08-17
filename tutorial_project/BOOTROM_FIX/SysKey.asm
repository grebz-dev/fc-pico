;========================================
; Key System
;========================================

;*****************************************
;キー入力システム
;*****************************************
;	拡張ＰＡＤ対応および、ΔＰＣＭ再生によるノイズ除去処理を追加しました。
;
;								渡部
;****************
; KEY RTN       *
;****************
;
BR_KEY_RTN:
	;----------------------------------------------------------------------
	; 4 回読込み版
	;----------------------------------------------------------------------
	lda	<KEY_NEW
	sta	<KEY_OLD

	 jsr	.read			; 6+153
	 sta	<KEY_CH1		; 3
	 jsr	.read			; 6+153
	 pha				; 3
	  jsr	.read			; 6+153
	  sta	<KEY_CH3		; 3
	  jsr	.read			; 6+153  (645)
	 pla				; (2nd read)
	cmp	<KEY_CH1
	beq	.store			; XX--
	cmp	<KEY_NEW		; (4th read)
	beq	.store			; -X-X
	lda	<KEY_CH3		; X-X-
					; -XX-
					; --XX
.store:
	sta	<KEY_NEW

;;	lda	<KEY_NEW
	eor	<KEY_OLD
	pha
	 and	<KEY_NEW
	 sta	<KEY_TRG
	pla
	and	<KEY_OLD
	sta	<KEY_REL

;;	RTS

	;--v--v-- キーリピートを追加 (2016-05-21 門真)
	lda	<KEY_TRG
	and	#(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)
	beq	.main			; 新たに押されたキーがない?

	tay
	lda  ro_keytable, y		; 同時押し対策 (下上右左の順に優先)
	sta	<REP_KEY		; 新たに押されたキーをリピート用に設定
	lda	#REP_WAIT
	sta	<REP_CNT		; 初回ウェイト
	.if	1
	 .if	 REP_WAIT
	  bne	.press			; =bra  押し始めは押下あり
	 .else
	  beq	.press			; =bra  押し始めは押下あり
	 .endif
	.else
	 lda	#0			; 押し始めは押下なし (trigger に任せる)
	 beq	.set			; =bra
	.endif
.main:
	lda	<KEY_NEW
	and	<REP_KEY
	beq	.set			; リピート用キーが押されていない (a=0)?

	dec	<REP_CNT		; ウェイトのカウントダウン
	beq	.press			; 初回ウェイト終了か?
	lda	<REP_CNT
	eor	#-REP_INTERVAL
	cmp	#1			; c = 0:一致 / 1:不一致
	lda	#0			; カウンタ初期値 または 押下キーなし
	bcs	.set			; 2 回目以降のウェイト終了でないか?

	sta	<REP_CNT		; カウンタを戻す。
.press:
	lda	<REP_KEY
.set:
	sta	<REP_NEW		; リピートにより ON/OFF される押下状態
	rts

	;----------------------------------------------------------------------
	; 4 回読込み版 - 1 回分サブ
	;----------------------------------------------------------------------
	; こちらの版は 2 フレーム間での比較を行わないため、
	; 反応が良くなって既存のゲームバランスに影響が出てしまった。
	; そのため、やむなく使用しないこととなった。
.read:
	lda	#1			; 2
	sta	<KEY_NEW		; 3
	sta	 $4016			; 4
	lsr	 a			; 2  =lda #0
	sta	 $4016			; 4
.read_loop:
	lda	 $4016			; 4x
	and	#(%11<<0)		; 2x
	cmp	#(%01<<0)		; 2x
	rol	<KEY_NEW		; 5x
	bcc	.read_loop		; 3x
					;-1
	lda	<KEY_NEW		; 3
	nop				; 2  調整用
	rts				; 6  (153)

	;----------------------------------------------------------------------
;;	Align	16
ro_keytable:		;2143
	.db	%0000	;----
	.db	%0001	;---R
	.db	%0010	;--L-
	.db	%0001	;--lR
	.db	%0100	;-D--
	.db	%0100	;-D-r
	.db	%0100	;-Dl-
	.db	%0100	;-Dlr
	.db	%1000	;U---
	.db	%1000	;U--r
	.db	%1000	;U-l-
	.db	%1000	;U-lr
	.db	%0100	;uD--
	.db	%0100	;uD-r
	.db	%0100	;uDl-
	.db	%0100	;uDlr
	;--^--^--


 .if 0

KEY_RTN2:
	;----------------------------------------------------------------------
	; 4 回読込み版
	;----------------------------------------------------------------------
	lda  KEY2_NEW
	sta  KEY2_OLD

	jsr  .read			; 6+153
	sta  KEY2_CH1		; 3
	jsr  .read			; 6+153
	pha					; 3
	jsr  .read			; 6+153
	sta  KEY2_CH3		; 3
	jsr  .read			; 6+153  (645)
	pla					; (2nd read)
	cmp  KEY2_CH1
	beq  .store			; XX--
	cmp  KEY2_NEW		; (4th read)
	beq  .store			; -X-X
	lda  KEY2_CH3		; X-X-
					; -XX-
					; --XX
.store:
	sta  KEY2_NEW

	eor  KEY2_OLD
	pha
	and  KEY2_NEW
	sta  KEY2_TRG
	pla
	and  KEY2_OLD
	sta  KEY2_REL

;;	RTS

	;--v--v-- キーリピートを追加 (2016-05-21 門真)
	lda  KEY2_TRG
	and  #(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)
	beq  .main			; 新たに押されたキーがない?

	tay
	lda  ro_keytable, y		; 同時押し対策 (下上右左の順に優先)
	sta  REP2_KEY		; 新たに押されたキーをリピート用に設定
	lda  #REP_WAIT
	sta  REP2_CNT		; 初回ウェイト
	.if	1
	 .if	 REP_WAIT
	  bne	.press			; =bra  押し始めは押下あり
	 .else
	  beq	.press			; =bra  押し始めは押下あり
	 .endif
	.else
	 lda	#0			; 押し始めは押下なし (trigger に任せる)
	 beq	.set			; =bra
	.endif
.main:
	lda  KEY2_NEW
	and  REP2_KEY
	beq	.set			; リピート用キーが押されていない (a=0)?

	dec  REP2_CNT		; ウェイトのカウントダウン
	beq	.press			; 初回ウェイト終了か?
	lda  REP2_CNT
	eor	#-REP_INTERVAL
	cmp	#1			; c = 0:一致 / 1:不一致
	lda	#0			; カウンタ初期値 または 押下キーなし
	bcs	.set			; 2 回目以降のウェイト終了でないか?

	sta  REP2_CNT		; カウンタを戻す。
.press:
	lda  REP2_KEY
.set:
	sta  REP2_NEW		; リピートにより ON/OFF される押下状態
	rts

	;----------------------------------------------------------------------
	; 4 回読込み版 - 1 回分サブ
	;----------------------------------------------------------------------
	; こちらの版は 2 フレーム間での比較を行わないため、
	; 反応が良くなって既存のゲームバランスに影響が出てしまった。
	; そのため、やむなく使用しないこととなった。
.read:
	lda  #1			; 2
	sta  KEY2_NEW		; 3
	sta  $4016			; 4
	lsr	 a			; 2  =lda #0
	sta	 $4016			; 4

.read_loop:
	lda	 $4017			; 4x
	and	#(%11<<0)		; 2x
	cmp	#(%01<<0)		; 2x
	rol	KEY2_NEW		; 5x
	bcc	.read_loop		; 3x
					;-1
	lda	KEY2_NEW		; 3
	nop				; 2  調整用
	rts				; 6  (153)

 .endif
