

;***********************************************************************
;	データ転送関連システム
;***********************************************************************


;=======================
; 背景データ 2400       *
;=======================
CLEAR_BG_24:
	pha
	SET_VRAM_ADD2 #$2400
	pla
	jmp  CLEAR_BG_00

;=======================
; 背景データ 2C00       *
;=======================
CLEAR_BG_2C:
	pha
	SET_VRAM_ADD2 #$2C00
	pla
	jmp  CLEAR_BG_00

BAK_CLR_RTN:
	lda  #$00
CLEAR_BG:
	pha
	SET_VRAM_ADD2 #$2000
	pla

CLEAR_BG_00:
	ldy  #0
	jsr  CLEAR_VRAM
	jsr  CLEAR_VRAM
	jsr  CLEAR_VRAM

CLEAR_VRAM:
	sta  $2007
	dey
	bne  CLEAR_VRAM
	rts


;=======================
; パレット初期セットサブ
;  IN: SRC_ADR 転送元アドレス 16bit
;=======================
setPalData:
	ldy  #32
	SET_DATA_DST PAL_WRK
setPalData2:
	PAL_CHG
	jmp  memcpy



;=======================
; ROMの指定バンクのデータをRAMにコピーする
;  IN: SRC_ADR 転送元アドレス 16bit
;  IN: DST_ADR 転送先アドレス 16bit
;  IN: Y 転送サイズ
;  破壊 A,Y
;=======================
memcpy:
.loop
	dey
	lda  [SRC_ADR],y
	sta  [DST_ADR],y
	cpy  #0
	bne  .loop
	rts

