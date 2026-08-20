;============================================
; 	選択画面系サブルーチン
;============================================

initSelDisp:

	jsr  STOP_SE

	lda  #0
	JSR     CLEAR_BG_2C
	lda  #0
	JSR     CLEAR_BG
	JSR     SPT_CLR_RTN

	
	
	LDA	#0
	sta  <BG_SCR_Y
	sta  <BG_SCR_X
	sta  <GM_TMP0
	sta  <PUSH_CTR

	SET_DATA_SRC  PAL_OVER_ADR
	jmp setPalData

PAL_CLEAR_ADR:
PAL_OVER_ADR:
	PAL_CLEAR



;----------------------
; ハイスコアチェック
;----------------------
CHK_HISCORE:
	LDA	#0
	STA	<TMP_DISP2
	LDX	#3			; 上の桁から見ていく
;        LDY     #BGM_CLEAR
.hichk_00:
	DEX
	BMI	.hichk_02
	LDA	GM_HISCORE,X
	CMP	GM_SCORE,X
	BEQ	.hichk_00		; 今の桁が同じなら下の桁を見にいく
	BCS	.hichk_02		; 今の桁でハイスコアより低いなら終わる

	; デバッグモード中はハイスコアを更新しないようにした (2016-06-01 門真)
;	lda	 DEBUG_FLG
;	bne	.update_e

	; ハイスコア更新
	LDA	GM_SCORE+2
	STA	GM_HISCORE+2
	LDA	GM_SCORE+1
	STA	GM_HISCORE+1
	LDA	GM_SCORE+0
	STA	GM_HISCORE+0
	INC	<TMP_DISP2
	jsr save_hiscore
.update_e:
;	LDY     #BGM_CLEAR2
.hichk_02:
        TYA
	RTS


save_hiscore:
	lda  GM_HISCORE+2
	sta  HISCORES+2
	lda  GM_HISCORE+1
	sta  HISCORES+1
	lda  GM_HISCORE+0
	sta  HISCORES+0
;@	jmp  save_SAVEDATA
	rts


;load_hiscore:
;	lda  HISCORES+2
;	sta  GM_HISCORE+2
;	lda  HISCORES+1
;	sta  GM_HISCORE+1
;	lda  HISCORES+0
;	sta  GM_HISCORE+0
;	rts



STR_CLEAR_2:
	DRAW_STRING2 "SCORE "
	rts



DRAW_SCORE:
	LDA	GM_SCORE+2
	JSR	DRAW_HEX_BYTE
	LDA	GM_SCORE+1
	JSR	DRAW_HEX_BYTE
	LDA	GM_SCORE+0
	JSR	DRAW_HEX_BYTE
	lda #0				;ダミー０
	jmp	DRAW_HEX_BYTE2

;-----------------------------------
; PUSH ANY BUTTON 点滅描画処理
;-----------------------------------

DRAW_PUSH_ANY_BUTTON
	INC	<PUSH_CTR
	LDA	<PUSH_CTR
	AND	#$10
	BEQ  DRAW_PUSH_ANY_BUTTON_C
	DRAW_STRING STR_PUSH_W
	RTS
DRAW_PUSH_ANY_BUTTON_C:
	DRAW_STRING STR_PUSH_C
	RTS

STR_PUSH_W
	DB "PUSH ANY BUTTON",0
STR_PUSH_C
	DB "               ",0



