



;================
;=スコア加算=====
;================

SCR_ADD:
	phxy	;xy push 疑似命令
	LDX	#GM_SCORE & $ff
	JSR	BCD_ADD
	LDA    #1
	STA    SCR_CHG_SW
	plxy	;xy pop 疑似命令
	RTS


SCR_ADD_10:
	phxy	;xy push 疑似命令
	TAX
	LDA	TBL_BCDx10,X
	LDX	#GM_SCORE & $ff
	JSR	BCD_ADD
	LDA    #1
	STA    SCR_CHG_SW
	plxy	;xy pop 疑似命令
	RTS

SCR_ADD_100:
	phxy	;xy push 疑似命令
	LDX	#(GM_SCORE+1) & $ff
	JSR	BCD_ADD
	LDA    #1
	STA    SCR_CHG_SW
	plxy	;xy pop 疑似命令
	RTS

SCR_ADD_1000:
	phxy	;xy push 疑似命令
	TAX
	LDA	TBL_BCDx10,X
	LDX	#(GM_SCORE+1) & $ff
	JSR	BCD_ADD
	LDA    #1
	STA    SCR_CHG_SW
	plxy	;xy pop 疑似命令
	RTS


TBL_BCDx10:
	DB $00,$10,$20,$30,$40,$50,$60,$70,$80,$90


;----------------------------
; フォーメーション変更
;----------------------------
changeForm:
	lda  PLY_FORM
	and  #$03
	bne  .end		; フォーメーションチェンジ中
	inc  PLY_FORM
	lda  #SE_PLY_FORM
	jmp  PLAY_SE
.end
	rts

;----------------------------
; ホーミング弾発射
;----------------------------
shotHorming:
	sta  <TMP_SVA	; X pos
	sty  <TMP_SVY
	ldx  #0
.loop
	lda PSHOT_A_Y,x
	bne .next

	txa
	asl  a
	asl  a
	tay
	lda  ENEMY_NT_KIND,y
	beq  .next
	lda  ENEMY_NT_HP,y
	beq  .next
	bmi  .next
	
	jsr  .set
	
.next
	inx
	inx
	cpx  #2*8
	bne  .loop
	rts

.set
	lda  #0
	sta  PSHOT_A_WX,x
	sta  PSHOT_A_WY,x
	lda  POS_PLY_Y
	sta  PSHOT_A_Y,x
	sta  <PRM_Y_POS		; 基準点Y

	lda  POS_PLY_X
	sta PSHOT_A_X,x
	sta <PRM_X_POS		; 基準点X

;	lda  #$F0		; 真上
;	lda  #$00+$30	; 真上
;	sta PSHOT_DIR,x

	lda  ENEMY_NT_X+1,y
	sta  <W_AR+0 		; ターゲットX

	lda  ENEMY_NT_Y+1,y
	sta  <W_AR+1 		; ターゲットY

	jsr  getAngleENT
	lda  <TMP_SVA
	ora  #$C0
	sta PSHOT_DIR,x

	lda  #SE_SHOT_A
	jmp  PLAY_SE




;----------------------------
; ショット処理
;----------------------------

PLY_SHOT_A:
	lda  PLY_ANM_NO
	cmp  #PLY_AN_DEAD
	beq  .end

	cmp  #PLY_AN_SHOTA
	beq  .end

	lda  #PLY_AN_SHOTA
	jsr  SET_PLY_ANM

.end
	rts



;----------------------------
; プレーヤー移動処理
;----------------------------
PLY_MOVE:

PLY_MOVE1:
	jmp  PLY_ANM_PROG



;------------------------------------
; 実行中のアニメに対応した処理を実行
;------------------------------------
PLY_ANM_PROG:
	lda  PLY_ANM_NO
	TBL_JUMP
	JPTBL  .WAIT		; 0 待機
	JPTBL  .CHARGE		; 1 チャージ
	JPTBL  .SHOTA		; 2 ショットA
	JPTBL  .SHOTB		; 3 ショットB
	JPTBL  .DEAD		; 4 死亡アニメ
	

.WAIT		; 0 待機 -----------------------------
;	rts


.CHARGE		; 1 チャージ -----------------------------
	; チャージアップ
;	rts

.SHOTA		; 2 ショットA -----------------------------
	lda  <FLM_TIMER
	and  #$07
	bne  .end

	ldy  POS_PLY_Y
	lda  POS_PLY_X
	jmp  setPlyShotA

.SHOTB		; 3 ショットB -----------------------------
	rts


.DEAD		; 4 死亡アニメ -----------------------------
.end
	rts

;------------------------
; アニメセット
;------------------------
SET_PLY_ANM:
	cmp  PLY_ANM_NO
	beq  .end
	sta  PLY_ANM_NO
.end
	rts





;-----------------------------------
; 爆発演出セット
; a reg = x座標
; y reg = y座標
;-----------------------------------
setBakuEfc:
	cmp  #8
	bcs   .x00
	lda  #8
.x00
	pha		; X pos
	tya
	pha		; Y pos
	ldy #0
.loop
	lda BAKU_EFC_CNT,y
	beq .end
.next
	iny
	iny
	iny
	cpy #3*(BAKU_EFC_SUU -1)
	bne .loop
.end
	lda #2
	sta BAKU_EFC_CNT,y
	pla
	sta BAKU_EFC_Y,y
	pla
	sta BAKU_EFC_X,y
	rts

;-----------------------------------
; ヒット演出セット（ダメージ無しの演出）
; a reg = x座標
; y reg = y座標
;-----------------------------------
setHitEfc:
	jsr  setBakuEfc
	lda #18
	sta BAKU_EFC_CNT,y
	rts

;-----------------------------------
; ヒット演出セット（ダメージ演出）
; a reg = x座標
; y reg = y座標
;-----------------------------------
setDameEfc:
	jsr  setBakuEfc
	lda #13
	sta BAKU_EFC_CNT,y
	rts

 .if 0
;-----------------------------------
; 敵ノーマル弾全クリアー
;-----------------------------------
clearAllEnemyNT:
	ldy #0
.loop
	lda #0
	sta ENEMY_NT_KIND,y
	tya
	clc
	adc  #ENEMY_NT_SIZE
	tay
	cpy #ENEMY_NT_SIZE*ENEMY_NT_SUU

	bne .loop
	rts
 .endif


;-----------------------------------
; SP敵セット
; a reg = x座標
; y reg = y座標
;-----------------------------------
setEnemyNT3:
	pha		; X pos
	tya
	pha		; Y pos
	ldy  #0
.loop
	lda  ENEMY_NT_KIND,y
	beq  setEnemyNT_SET
.next
	tya
	clc
	adc  #ENEMY_NT_SIZE
	tay
	cpy  #ENEMY_NT_SIZE*8
	bne .loop

.end
	pla
	pla
	sec
	rts

;-----------------------------------
; 敵ノーマル弾セット
; a reg = x座標
; y reg = y座標
;-----------------------------------
setEnemyNT2:
	pha		; X pos
	tya
	pha		; Y pos
	lda  <ENEMY_NT_FLFG
	bne  .end
	ldy  #ENEMY_NT_SIZE*8
.loop
	lda  ENEMY_NT_KIND,y
	beq  setEnemyNT_SET
.next
	tya
	clc
	adc  #ENEMY_NT_SIZE
	tay
	cpy  #ENEMY_NT_SIZE*ENEMY_NT_SUU
	bne .loop

	; 空きワークなし
	inc  <ENEMY_NT_FLFG
.end
	pla
	pla
	sec
	rts


;-----------------------------------
; 敵ノーマル弾セット
; a reg = x座標
; y reg = y座標
;-----------------------------------
setEnemyNT:
	pha		; X pos
	tya
	pha		; Y pos
	lda  <ENEMY_NT_FLFG
	bne  .end
	ldy  #0
.loop
	lda  ENEMY_NT_KIND,y
	beq  setEnemyNT_SET
.next
	tya
	clc
	adc  #ENEMY_NT_SIZE
	tay
	cpy  #ENEMY_NT_SIZE*ENEMY_NT_SUU
	bne .loop

	; 空きワークなし
	inc  <ENEMY_NT_FLFG
.end
	pla
	pla
	sec
	rts

setEnemyNT_SET
	lda  #0
	sta  ENEMY_NT_Y+0,y
	sta  ENEMY_NT_X+0,y
	sta  ENEMY_NT_MP,y
	sta  ENEMY_NT_DT,y
	sta  ENEMY_NT_HP,y

	lda  #NTK_ANGLE
	sta  ENEMY_NT_KIND,y
	pla
	sta  ENEMY_NT_Y+1,y
	pla
	sta  ENEMY_NT_X+1,y
	clc
	rts

;-----------------------------------
; 自機ノーマル弾セット
; a reg = x座標
; y reg = y座標
;-----------------------------------
setPlyShotA:
	sta  <TMP_SVA	; X pos
	sty  <TMP_SVY
	ldx  #2*8
	ldy  #PSHOT_A_SUU-8
.loop
	lda PSHOT_A_Y,x
	beq .set
	inx
	inx
	dey
	bne  .loop
	; 空きワークなし
	rts

.set
	lda #0
	sta PSHOT_A_WX,x
	sta PSHOT_A_WY,x
	lda <TMP_SVY
	sta PSHOT_A_Y,x
	sta  <PRM_Y_POS		; 基準点Y

	lda <TMP_SVA
	sta PSHOT_A_X,x
	sta <PRM_X_POS		; 基準点X

;	lda  #$F0		; 真上
	lda  #$80+$30	; 真上
	sta PSHOT_DIR,x

 .if 0
	; ターゲットサーチチェック
	txa
	and  #$7*2
	asl  a
	asl  a
	tay
	lda  ENEMY_NT_KIND,y
	beq  .end
	lda  ENEMY_NT_HP,y
	beq  .end
	bmi  .end

.set2
	lda  ENEMY_NT_X+1,y
	sta  <W_AR+0 		; ターゲットX

	lda  ENEMY_NT_Y+1,y
	sta  <W_AR+1 		; ターゲットY

	jsr  getAngleENT
	lda  <TMP_SVA
	ora  #$C0
	sta PSHOT_DIR,x
 .endif

.skip
	; 鳴らし過ぎなので少し間引く
	lda  <FLM_TIMER
	and  #$07
	bne  .end
	lda  #SE_SHOT_A
	jmp  PLAY_SE
.end
	rts



;----------------------------------
; プレーヤー死亡セット
;----------------------------------
setPlayerDead:
	ldy  <DEMO_FG
	bne  .end

	lda  PLY_MUTEKI_TM
	bne  .end
	
  .if DEBUG_NO_GAME_OVER
  .else
	lda  #DAM_BG_FLASH_INIT
	sta  DAM_BG_FLASH
	lda  #MUTEKI_TIME
	sta  PLY_MUTEKI_TM

	lda  PLY_LIFE
	beq  .end
	dec  PLY_LIFE
	bne  .end_dame


	sta  PLY_MUTEKI_TM
	; 死亡アニメセット
	lda  #PLY_AN_DEAD
	sta  PLY_ANM_NO

	lda  #SE_PLY_DEAD
	jmp  PLAY_SE
  .endif

.end
	rts

.end_dame
	lda  #SE_PLY_DAME
	jmp  PLAY_SE

;----------------------------------
; プレーヤー死亡エフェクトセット
;----------------------------------
setPlayerDeadEffect:
	phxy
	lda  <SYS_TIMER
	and  #$03
	bne  .end
	
	jsr GET_RND
	and #$1f
	adc POS_PLY_Y
	sbc #15
	tay

	jsr GET_RND
	and #$1f
	adc POS_PLY_X
	sbc #10

	jsr  setBakuEfc


.end
	plxy
	rts


