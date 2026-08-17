
;***********************************************************************
;	FC-PICO関連システム
;***********************************************************************

PF_MAGIC_CODE = $FC		; 受け取ったコマンドの可否チェックコード
;----------------------------------------------
;
; PICOからファミコン　コマンド
;
;
;----------------------------------------------
PF_COM_NONE = 0			; コマンドなし
PF_COM_DMOD = 1			; 表示OFFにしてデータ転送モードへ
PF_COM_FDIN = 2			; フェードイン
PF_COM_FDOT = 3			; フェードアウト

PF_COM_SE   = $80		; SEセット: $80 + SE_NO
PF_COM_BGM  = $A0		; BGMセット:$A0 + BGM_NO
PF_COM_VRAM = $C0		; VRAM 書き換え: $C0 +adrH,ardL,dt



;----------------------------------------------
;
; PICOからファミコンへデータセット
;
; 画面OFF限定で大量のデータをPICOからファミコンへデータを送るコマンド
;
;----------------------------------------------
PF_DAT_VRAM = $80		; VRAM 書き換え:adrH,ardL,size,data....
; --> size = 0 は256バイト 256バイト以上送りたい場合は分割して送る
PF_DAT_RAM  = $81		; VRAM 書き換え:adrH,ardL,size,data....

PF_DAT_STEP = $82		; データモードを抜けてファミコンの指定ステップへ

;----------------------------------------------
;
; ファミコンからPICO　コマンド
;
;----------------------------------------------
FP_COM_ACK	= $0F		; PICOからのコマンド正常終了応答
FP_COM_NAK	= $1F		; PICOからのコマンド失敗終了応答
FP_COM_VER	= $2F		; BIOS-ROMのバージョン取得：0x0FバイトのROMバージョン文字列
FP_COM_ROM	= $3F		; BIOS-ROMのROMデータ要求コマンド： FP_COM_ROM,アドレスH : 0x100分のROMデータ読み出す

FP_COM_LOG	= $BF		; ログ
FP_COM_DRQ	= $CF		; データリクエスト
FP_COM_DLD	= $DF		; データロード
FP_COM_RST	= $EF		; PICOリスタート
FP_COM_INI	= $FF		; PICO初期化


;------------------------------------------
;
;		PICO コマンド処理
;
;------------------------------------------
jobPICO:
	ldx  #2
jobPICO_NEXT:
	cpx  #16
	beq  jobPICI_end
	lda  PICO_BUF0,x
	bmi  .job00

	TBL_JUMP
	JPTBL	jobPICI_end		; 0 コマンド終了
	JPTBL	xPF_COM_DMOD	; 1 表示OFFにしてデータ転送モードへ
	JPTBL	xPF_COM_FDIN	; 2 フェードイン
	JPTBL	xPF_COM_FDOT	; 3 フェードアウト

.job00
	cmp  #PF_COM_BGM
	bcc  xPF_COM_SE
	cmp  #PF_COM_VRAM
	bcc  xPF_COM_BGM
	bcs  xPF_COM_VRAM

jobPICI_end:
	lda  #0
	sta  <PICO_BUF2
	rts

;-------------------
; サウンド関連
;-------------------
xPF_COM_BGM:
	and  #$1F
	bne  .play
;@	jsr  STOP_BGM
	inx
	jmp  jobPICO_NEXT
.play
;@	jsr  PLAY_BGM
	inx
	jmp  jobPICO_NEXT
	
xPF_COM_SE:
	and  #$1F
;@	jsr  PLAY_SE
	inx
	jmp  jobPICO_NEXT


;-------------------
; VRAM書き換え
;-------------------
xPF_COM_VRAM:
 .if 0
	pha
	lda  #1
	jsr  getVRAMT_BUF

	pla						; adrH
	and  #$3F
	cmp  #$3F
	php
	jsr  writeVRAMT_DATA
	lda  PICO_BUF1,x		; adrL
	jsr  writeVRAMT_DATA
	lda  PICO_BUF2,x		; Data
	jsr  writeVRAMT_DATA
	jsr  endVRAMT_DATA
	plp
	bne  .nopal
	lda  PICO_BUF1,x		; adrL
	and  #$1F
	tay
	lda  PICO_BUF2,x		; Data
	sta  PAL_WRK,y
  .endif
.nopal
	inx
	inx
	inx
	jmp  jobPICO_NEXT


;-------------------
; フェードイン 処理終了
;-------------------
xPF_COM_FDIN:
	txa
	pha
	jsr SET_FADE_IN_B
	jsr WAIT_FADE_END
	pla
	tax
	inx
	jmp  jobPICO_NEXT


;-------------------
; フェードアウト 処理終了
;-------------------
xPF_COM_FDOT:
	txa
	pha
 	jsr SET_FADE_OUT_B
	jsr WAIT_FADE_END
	pla
	tax
	inx
	jmp  jobPICO_NEXT

;-------------------
; 表示OFFにしてデータ転送モードへ
;-------------------
xPF_COM_DMOD:
	DISP_OFF
	lda  #0
	sta	 $2000

xPF_COM_DMOD_loop:
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_DRQ
	sta  $2007

	jsr  PICO_COM_WAIT

	SET_VRAM_ADD2	#$0800

	; PICOからデータ受診
	lda  $2007	;dummy read

	lda  $2007
	sta  <PICO_BUF0		; com
	lda  $2007
	sta  <PICO_BUF1		; magic no($FC)
	lda  $2007
	sta  <PICO_BUF2		; adr L
	lda  $2007
	sta  <PICO_BUF3		; adr H
	lda  $2007
	sta  <PICO_BUF4		; size L
	lda  $2007
	sta  <PICO_BUF5		; size H

;	jsr setFP_COM_LOG

	; コマンド有効性チェック
	lda  <PICO_BUF1
	cmp  #PF_MAGIC_CODE
	bne  errPF_COM_DMOD_end


	lda  #0
	sta  <PICO_BUF1

	lda  <PICO_BUF0
	cmp  #PF_DAT_VRAM
	beq  xPF_DAT_VRAM
	cmp  #PF_DAT_RAM
	beq  xPF_DAT_RAM
	cmp  #PF_DAT_STEP
	beq  xPF_DAT_STEP

xPF_COM_DMOD_end:
;	jsr PICO_COM_WAIT
;	jsr setDebugLog
;	jsr setFP_COM_LOG
	lda #FLG_PPU2000
	sta	 $2000
	DISP_ON
	jsr SET_FADE_IN_B

	jmp  jobPICI_end


xPF_DAT_STEP:
	inc	<NMI_FLG	;ハング防止
	lda  <PICO_BUF2
	sta  <STG_COD

	jmp  xPF_COM_DMOD_end


errPF_COM_DMOD_end:
	lda  #1
	jsr  setErrorLog
	jmp  xPF_COM_DMOD_end





;----------------------------------------------
;
; PICOからファミコンへデータセット
;
;----------------------------------------------
xPF_DAT_VRAM
	jsr  xPF_DAT_SUB
	lda  <PICO_BUF3		; adr H
	cmp  #$23
	bne  .pfrloop
	lda  <PICO_BUF2		; adr L
	cmp  #$C0
	bne  .pfrloop
	lda  $2007			; アトリビュートだったら1バイト追加でダミー読み出し（原因がわからないが１バイトずれる）

.pfrloop
	lda  $2007
	sta  PICO_DATA_BUF,y
	iny
	cpy  <PICO_BUF0
	bne  .pfrloop

	; vram adr set
	lda  <PICO_BUF3		; adr H
    sta  $2006
	lda  <PICO_BUF2		; adr L
	sta  $2006

	ldy  #0
.pfwloop
	lda  PICO_DATA_BUF,y
	sta  $2007
	iny
	cpy  <PICO_BUF0
	bne  .pfwloop

	lda  <PICO_BUF5		; size H
	beq  .end
	dec  <PICO_BUF5		; size H
	inc  <PICO_BUF1		; data no
	jmp  xPF_DAT_VRAM
.end

 .if 0
	jsr  setDebugLog
 .endif
	lda  <PICO_BUF3		; adr H
	cmp  #$3F
	bne  .end2
	;---------------------------------
	; copy PICO_DATA_BUF -> PAL_WRK
	;---------------------------------
	ldy  #0
.loop_pal
	lda  PICO_DATA_BUF,y
	sta  PAL_WRK,y
	iny
	cpy  #$20
	bne  .loop_pal
.end2
	jmp  xPF_COM_DMOD_loop


xPF_DAT_RAM
	jsr  xPF_DAT_SUB
.loop
	lda  $2007
	sta  [PICO_BUF2],y
	iny
	cpy  <PICO_BUF0
	bne  .loop

	lda  <PICO_BUF5		; size H
	beq  .end
	dec  <PICO_BUF5		; size H
	inc  <PICO_BUF1		; data no
	jmp  xPF_DAT_RAM
.end
	jmp  xPF_COM_DMOD_loop


xPF_DAT_SUB
	ldy  <PICO_BUF4		; size L
	lda  <PICO_BUF5		; size H
	beq  .pf00
	ldy  #0
.pf00
	sty  <PICO_BUF0		; read size

	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_DLD
	sta  $2007
	lda  <PICO_BUF1		; data no
	sta  $2007

	jsr  PICO_COM_WAIT

	; PICOからデータ取得
	SET_VRAM_ADD2	#$0800
	lda  $2007	; ダミーリード
	ldy  #0
	rts

setDebugLog:
 .if 1
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_LOG
	sta  $2007
	lda  PICO_DATA_BUF+0
	sta  $2007
	lda  PICO_DATA_BUF+1
	sta  $2007
	lda  PICO_DATA_BUF+2
	sta  $2007
	lda  PICO_DATA_BUF+3
	sta  $2007
	jsr  PICO_COM_WAIT
 .endif
	rts

setErrorLog:
 .if 1
	pha
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_LOG
	sta  $2007
	lda  #$EA
	sta  $2007
	pla
	sta  $2007
	jmp  PICO_COM_WAIT
 .endif

PICO_COM_WAIT:
	ldx  #0
.wait
	dex
	bne  .wait
	rts

setFP_COM_LOG:
  .if 1
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_LOG
	sta  $2007

	lda  <PICO_BUF0		; com
	sta  $2007
	lda  <PICO_BUF1		; magic no($FC)
	sta  $2007
	lda  <PICO_BUF2		; adr L
	sta  $2007
	lda  <PICO_BUF3		; adr H
	sta  $2007
	lda  <PICO_BUF4		; size L
	sta  $2007
	lda  <PICO_BUF5		; size H
	sta  $2007
 .endif
	jmp  PICO_COM_WAIT

