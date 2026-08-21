;/// @file SysData.asm
;/// @brief Screen and palette bulk-fill helpers.
;/// @ingroup bootrom


;***********************************************************************
;	データ転送関連システム
;***********************************************************************


;=======================
; 背景データ 2400       *
;=======================
;/// @brief Clears nametable `$2400`.
;/// @ingroup bootrom
CLEAR_BG_24:
	pha
	SET_VRAM_ADD2 #$2400
	pla
	jmp  CLEAR_BG_00

;=======================
; 背景データ 2C00       *
;=======================
;/// @brief Clears nametable `$2C00`.
;/// @ingroup bootrom
CLEAR_BG_2C:
	pha
	SET_VRAM_ADD2 #$2C00
	pla
	jmp  CLEAR_BG_00

;/// @brief Shared tail of the nametable clear routines.
;/// @ingroup bootrom
BAK_CLR_RTN:
	lda  #$00
;/// @brief Clears nametable `$2000`.
;/// @ingroup bootrom
CLEAR_BG:
	pha
	SET_VRAM_ADD2 #$2000
	pla

;/// @brief Clears a nametable to tile 0.
;/// @ingroup bootrom
CLEAR_BG_00:
	ldy  #0
	jsr  CLEAR_VRAM
	jsr  CLEAR_VRAM
	jsr  CLEAR_VRAM

;/// @brief Clears the whole of VRAM.
;/// @ingroup bootrom
CLEAR_VRAM:
	sta  $2007
	dey
	bne  CLEAR_VRAM
	rts


;=======================
; パレット初期セットサブ
;  IN: SRC_ADR 転送元アドレス 16bit
;=======================
;/// @brief Loads 32 palette bytes into #PAL_WRK and flags the palette dirty.
;/// @ingroup bootrom
setPalData:
	ldy  #32
	SET_DATA_DST PAL_WRK
;/// @brief As @ref setPalData, with the source address already in #SRC_ADR.
;/// @ingroup bootrom
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
;/// @brief Copies Y bytes from #SRC_ADR to #DST_ADR, descending.
;/// @ingroup bootrom
memcpy:
.loop
	dey
	lda  [SRC_ADR],y
	sta  [DST_ADR],y
	cpy  #0
	bne  .loop
	rts

