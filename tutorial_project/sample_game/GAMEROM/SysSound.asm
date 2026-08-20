;======================================================
;			サウンドシステム
;======================================================

_nsf_init		EQU $8010	; NSF init address 
_nmi_main		EQU $8084	; NSF play address
_nsd_init		EQU $80A1
_nsd_set_dpcm	EQU $80AB	; ax = Pointer of ⊿PCM infomation Struct
_nsd_main		EQU $80B2
_nsd_play_bgm	EQU $8137	; ax = Pointer of BGM
_nsd_stop_bgm	EQU $8219
_nsd_play_se	EQU $8239	; ax = Pointer of SE
_nsd_stop_se	EQU $82B7
_nsd_snd_init	EQU $8AB6

_nsd_table_idx	EQU $8F6E	; テーブルインデックス


bgm_addr	EQU (_nsd_table_idx+2)
se_addr		EQU (_nsd_table_idx+2)

STOP_SE_SYS	 EQU  _nsd_stop_se

STOP_SE:
	jmp  STOP_SE_SYS



;===============================
; BGM再生
;===============================
PLAY_BGM:
	STA  <REQ_BGM_NO
    RTS

;===============================
; SE再生
;===============================
PLAY_SE_FORCE:
	STA	<REQ_SE_NO
	rts

PLAY_SE:
	phxy
;	ldy  <DEMO_FG
;	bne  .end

	ldy  #2
	tax
.loop
	ldx  REQ_SE_NO,y
	beq  .set
	dey
	bne  .loop
.set
	sta  REQ_SE_NO,y
.end
	plxy
	RTS



;===============================
; サウンド初期化
;===============================
INIT_SOUND:
	lda  #15
	sta  <MASTER_VOL
	jmp  _nsd_init

;---------------------------------------------
;	A	bit 0	BGM Status (0:Stop / 1:Play)
;		bit 1	SE  Status (0:Stop / 1:Play)

MUSDRV_GET_STATE:
	lda  __flag
	rts





STOP_BGM:
STOP_BGM_SYS:
	; BGM 停止時に DPCM が止まらないので無理矢理止める (2016-05-15 門真)
	lda	#%00001111
	sta	 $4015
	jmp  _nsd_stop_bgm




;*****************************************
;サウンドシステム
;*****************************************
SOUND_SYSTEM:
	lda  <DEMO_FG
	beq  .snd_sys40
	lda  #0
	sta  <REQ_BGM_NO
.snd_sys40
	lda  <SEQ_CTR
	beq  .snd_sys02
	dec  <SEQ_CTR		; BGM再生ウェイト
	jmp  .snd_sys00
.snd_sys02:
	lda  <REQ_BGM_NO
	beq  .snd_sys00
	jsr  _playBGM
.skip00
	lda  #0
	sta  <REQ_BGM_NO
.snd_sys00:
	lda  <REQ_SE_NO3
	beq  .snd_sys01_2
	jsr  _playSE
.snd_sys01_2:
	lda  <REQ_SE_NO2
	beq  .snd_sys01_3
	jsr  _playSE
.snd_sys01_3:
	lda  <REQ_SE_NO
	beq  .snd_sys01_4
	jsr  _playSE
.snd_sys01_4:
	lda  #0
	sta  <REQ_SE_NO
	sta  <REQ_SE_NO2
	sta  <REQ_SE_NO3

	; --- main start -----

 	jmp  _nsd_main

	
_playSE
	asl	a
	tay
	;SE再生開始
	lda  se_addr + 0,y
	ldx  se_addr + 1,y
	jmp  _nsd_play_se
.end
	rts

_playBGM
	asl	a
	tay
	lda  bgm_addr + 0,y
	ldx  bgm_addr + 1,y
	jmp  _nsd_play_bgm
	rts


