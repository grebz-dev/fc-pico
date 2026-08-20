;=====================================
;
;	ミッション　HARADIUS制御プログラム
;
;
;=====================================







mi_Hara:
	lda MISSON_STEP
	TBL_JUMP
	JPTBL	mi_HaraInit		; 0
	JPTBL	mi_HaraMove		; 1
	JPTBL	mi_HaraClear	; 2
	JPTBL	mi_HaraOUT		; 3



;-----------------------
;		ボス初期化
;-----------------------

mi_HaraInit:
	jsr  initMissionControl
	jsr  setGameCommonPal

	inc MISSON_STEP
	
	rts


;-----------------------
;		移動
;-----------------------
mi_HaraMove:
	jsr  MissionAnimeHARA
	jsr  mainMissionControl
	bcc .next

	lda #0
	sta <BG_SCR_Y

	lda #120
	sta MISSON_TMP




	inc MISSON_STEP
	rts

.next
;	jsr  moveLasterBG
;	jsr  hitLasterBG
	rts


;-----------------------
;		BG消去
;-----------------------
mi_HaraClear:
	inc MISSON_STEP
.wait
	rts

;-----------------------
;		ボス消滅
;-----------------------

mi_HaraOUT:
	jmp getMission


