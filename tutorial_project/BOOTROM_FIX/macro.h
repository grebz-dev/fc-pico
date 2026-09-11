;/// @file macro.h
;/// @brief The assembler macro library: arithmetic, VRAM access, strings and flow control.
;/// @ingroup bootrom
;///
;/// nesasm has no inline functions, so anything that would be a small helper is a
;/// macro here. The important groups are:
;///
;/// - **16-bit arithmetic** -- `LD_W`, `ADD_W`, `addw`, `subw`, `incw`, `decw`
;/// - **VRAM addressing** -- `SET_VRAM_ADD2` and friends, which set `$2006`.
;///   `SET_VRAM_ADD2 #$0800` is how the cartridge port is opened. @see @ref protocol
;/// - **Display control** -- `DISP_ON`, `DISP_OFF`, made macros specifically to
;///   avoid IRQ-line noise when rendering is toggled
;/// - **Flow control** -- `TBL_JUMP` / `JPTBL` build jump tables by pushing an
;///   address minus one and executing `RTS`
;/// - **String drawing** -- `DRAW_STRING2` embeds its text inline and fakes a
;///   return address so execution resumes past the data
;///
;/// @warning This file is **byte-identical** in `BOOTROM/` and `BOOTROM_FIX/`,
;///          including this header. Edit both or neither. @see @ref conventions
;/// @note The banking macros (`CHG_BANK_*`, `PUSH_BANK`, `POP_BANK`) and the
;///       scanline-IRQ macros (`HIRQ_*`) are inert on this NROM cartridge.
;******************************************************************************
;	MOVE関係
;******************************************************************************
;/// @brief Loads a 16-bit immediate into a zero-page word.
;/// @ingroup bootrom
LD_W MACRO
	lda  \2
	sta  \1
	lda  \2+1
	sta  \1+1
	ENDM

;******************************************************************************
;	算術関係
;******************************************************************************

;/// @brief Adds a 16-bit value to a zero-page word.
;/// @ingroup bootrom
ADD_W MACRO
	clc
	lda  \1
	adc  \2
	sta  \1
	lda  \1+1
	adc  \2+1
	sta  \1+1
	ENDM

;------------------------------------------------------------------------------
;				バイト足し演算
;------------------------------------------------------------------------------
;/// @brief Adds an 8-bit value to a memory location.
;/// @ingroup bootrom
add	MACRO
	clc
	adc	\1
	ENDM
;------------------------------------------------------------------------------
;				バイト引き演算
;------------------------------------------------------------------------------
;/// @brief Subtracts an 8-bit value from a memory location.
;/// @ingroup bootrom
sub	MACRO
	sec
	sbc	\1
	ENDM
;------------------------------------------------------------------------------
;				ワード足し演算
;				xy + \1\2 = xy
;------------------------------------------------------------------------------
;/// @brief Adds a 16-bit value to a zero-page word, with carry.
;/// @ingroup bootrom
addw	MACRO
	pha

	tya
	add	\2
	tay
	bne	.byte_over\@
	bcc	.byte_over\@

	sec
.byte_over\@:
	txa
	adc	\1
	tax

	pla
	ENDM
;------------------------------------------------------------------------------
;				ワード引き演算
;				xy - \1\2 = xy
;------------------------------------------------------------------------------
;/// @brief Subtracts a 16-bit value from a zero-page word, with borrow.
;/// @ingroup bootrom
subw	MACRO
	pha

	tya
	sub	\2
	tay

	txa
	sbc	\1
	tax

	pla
	ENDM

;------------------------------------------------------------------------------
;				ワードインクリメント
;				\1\2 + 1 = \1\2
;------------------------------------------------------------------------------
;/// @brief Increments a 16-bit zero-page word.
;/// @ingroup bootrom
incw	MACRO
	inc	\1
	bne	.iend\@

	inc	\1 + 1
.iend\@:

	ENDM

;------------------------------------------------------------------------------
;				ワードデクリメント
;				\1\2 - 1 = \1\2
;------------------------------------------------------------------------------
;/// @brief Decrements a 16-bit zero-page word.
;/// @ingroup bootrom
decw	MACRO
	lda	\1
	bne	.dend\@

	dec	\1 + 1
.dend\@:
	dec	\1

	ENDM

;------------------------------------------------------------------------------
;		擬似ワードレジスタ　AR に固定ワードセット
;------------------------------------------------------------------------------
;/// @brief Loads an immediate into the #W_AR arithmetic register.
;/// @ingroup bootrom
SET_AR	MACRO
	LDA	\1 & $ff
        STA	<W_AR
	LDA	\1 >> 8
        STA	<W_AR+1
	ENDM

;------------------------------------------------------------------------------
;		擬似ワードレジスタ　BR に固定ワードセット
;------------------------------------------------------------------------------
;/// @brief Loads an immediate into the #W_BR arithmetic register.
;/// @ingroup bootrom
SET_BR	MACRO
	LDA	\1 & $ff
        STA	<W_BR
	LDA	\1 >> 8
        STA	<W_BR+1
	ENDM

;------------------------------------------------------------------------------
;		擬似ワードレジスタ　AR にメモリー上のワード値セット
;------------------------------------------------------------------------------
;/// @brief Loads #W_AR from a memory word.
;/// @ingroup bootrom
SET_AR_M	MACRO
	LDA	\1
        STA	<W_AR
	LDA	\1+1
        STA	<W_AR+1
	ENDM

;------------------------------------------------------------------------------
;		擬似ワードレジスタ　BR にメモリー上のワード値セット
;------------------------------------------------------------------------------
;/// @brief Loads #W_BR from a memory word.
;/// @ingroup bootrom
SET_BR_M	MACRO
	LDA	\1
        STA	<W_BR
	LDA	\1+1
        STA	<W_BR+1
	ENDM

;------------------------------------------------------------------------------
;				対象のメモリーのビットをセットする
;				対象メモリーアドレス = \1
;				セットするビット     = \2
;------------------------------------------------------------------------------
;/// @brief Sets the given bits in a memory location.
;/// @ingroup bootrom
SET_BIT		MACRO
	LDA	\1
	ORA	\2
	STA	\1
	ENDM

;------------------------------------------------------------------------------
;				対象のメモリーのビットをクリアーする
;				対象メモリーアドレス = \1
;				クリアーするビット   = \2
;------------------------------------------------------------------------------
;/// @brief Clears the given bits in a memory location.
;/// @ingroup bootrom
CLR_BIT		MACRO
	LDA	\1
	AND	$ff - \2
	STA	\1
	ENDM

;------------------------------------------------------------------------------
;				対象のメモリーのビットをチェックする
;				対象メモリーアドレス = \1
;				チェックするビット   = \2
;------------------------------------------------------------------------------
;/// @brief Tests the given bits in a memory location, setting Z accordingly.
;/// @ingroup bootrom
CHK_BIT		MACRO
	LDA	\1
	AND	\2
	ENDM


;------------------------------------------------------------------------------
;				XYレジスタをスタックに退避
;------------------------------------------------------------------------------
;/// @brief Pushes X and Y.
;/// @ingroup bootrom
phxy		MACRO
	sta  <TMP_SYS
	txa
	pha
	tya
	pha
	lda  <TMP_SYS
	ENDM

;------------------------------------------------------------------------------
;				XYレジスタをスタックから復帰
;------------------------------------------------------------------------------
;/// @brief Pops Y and X, restoring the order pushed by @c phxy.
;/// @ingroup bootrom
plxy		MACRO
	sta  <TMP_SYS
	pla
	tay
	pla
	tax
	lda  <TMP_SYS
	ENDM

;------------------------------------------------------------------------------
;				SRC_ADRをスタックに退避
;------------------------------------------------------------------------------
;/// @brief Pushes the 16-bit #SRC_ADR pointer.
;/// @ingroup bootrom
phSRC_ADR	MACRO
	lda  <SRC_ADR+0
	pha
	lda  <SRC_ADR+1
	pha
	ENDM

;------------------------------------------------------------------------------
;				XYレジスタをスタックから復帰
;------------------------------------------------------------------------------
;/// @brief Pops the 16-bit #SRC_ADR pointer.
;/// @ingroup bootrom
plSRC_ADR	MACRO
	pla
	sta  <SRC_ADR+1
	pla
	sta  <SRC_ADR+0
	ENDM

;******************************************************************************
;	表示関係
;******************************************************************************
;	画面切り替え時にＩＲＱ割り込みラインでのノイズを対処すべく、
;	表示on、表示offは、マクロ化しました。→DISP_ON, DISP_OFF
;	それに伴い、NMI_FLGの操作も併せて、こちらに移動しました。
;							渡部
;------------------------------------------------------------------------------
;				表示on
;------------------------------------------------------------------------------
;/// @brief Enables rendering, waiting for a frame boundary first.
;/// @note A macro rather than a call so that toggling rendering does not
;///       disturb the IRQ line and put noise on screen.
;/// @ingroup bootrom
DISP_ON		MACRO
	jsr  _disp_on_sub
	ENDM

;------------------------------------------------------------------------------
;				表示on NO SP
;------------------------------------------------------------------------------
;/// @brief Enables background rendering but leaves sprites disabled.
;/// @ingroup bootrom
DISP_ON_NSP		MACRO
	jsr  _disp_on_sub2
	ENDM

;------------------------------------------------------------------------------
;				表示off
;------------------------------------------------------------------------------
;/// @brief Disables rendering, waiting for a frame boundary first.
;/// @warning Required before bulk transfers: with rendering on there is not
;///          enough `$2007` bandwidth. @see xPF_COM_DMOD
;/// @ingroup bootrom
DISP_OFF	MACRO
	jsr  _disp_off_sub
	ENDM

;------------------------------------------------------------------------------
;				VRAMアドレスセット
;				VRAMアドレス = \1(16bit adr)
;------------------------------------------------------------------------------
;/// @brief Points the PPU address register at a 16-bit address.
;/// @warning `SET_VRAM_ADD2 #$0800` does **not** address video memory. It opens
;///          the cartridge port: pattern-table space is decoded by the
;///          cartridge, so the following `$2007` accesses reach the RP2350.
;///          @see @ref protocol
;/// @ingroup bootrom
SET_VRAM_ADD2	MACRO
	lda #HIGH (\1)
    sta $2006
	lda #LOW  (\1)
	sta $2006
	ENDM

;------------------------------------------------------------------------------
;				VRAMアドレスセット
;				VRAMアドレス = \1(16bit adr)
;				加算値 = \2
;------------------------------------------------------------------------------
;/// @brief Points the PPU address register at an address held in memory.
;/// @ingroup bootrom
SET_VRAM_ADD3	MACRO
	clc
	lda #LOW  (\1)
	adc  \2
	pha
	lda #HIGH (\1)
	adc #0
    sta $2006
	pla
	sta $2006
	ENDM

;------------------------------------------------------------------------------
;				VRAMアドレスセット
;				VRAMアドレス = \1(16bit adr)
;				加算値 = \2
;------------------------------------------------------------------------------
;/// @brief Points the PPU address register at an indexed address.
;/// @ingroup bootrom
SET_VRAM_ADD4	MACRO
	clc
	lda #LOW  (\1)
	adc  \2
	pha
	lda #HIGH (\1)
	adc \2 +1
    sta $2006
	pla
	sta $2006
	ENDM

;------------------------------------------------------------------------------
;				VRAMアドレスセット
;				VRAMアドレス = \1(16bit adr)
;------------------------------------------------------------------------------
;/// @brief Points the PPU address register at a nametable cell given as column and row.
;/// @ingroup bootrom
SET_VRAM_LOC	MACRO
	lda #HIGH (\1)
    sta <TMP_ADR0+1
	lda #LOW  (\1)
    sta <TMP_ADR0+0
	ENDM

;/// @brief As @c SET_VRAM_LOC, adding an offset.
;/// @ingroup bootrom
SET_VRAM_LOC_ADD	MACRO
    lda <TMP_ADR0+1
    sta $2006
    lda <TMP_ADR0+0
    sta $2006
	ENDM

;/// @brief Advances the PPU address by one nametable row.
;/// @ingroup bootrom
ADD_VRAM_LOC_CR	MACRO
	clc
	lda  <TMP_ADR0+0
	adc  #32
	sta  <TMP_ADR0+0
	lda  <TMP_ADR0+1
	adc  #0
	sta  <TMP_ADR0+0
	ENDM


;------------------------------------------------------------------------------
;				VRAMアドレスセット
;				VRAMアドレス = \1(16bit adr)
;------------------------------------------------------------------------------
;/// @brief Points the PPU address register at page A, offset zero.
;/// @ingroup bootrom
SET_VRAM_ADD_A_00	MACRO
        STA  $2006
        LDA	#$00
        STA  $2006
	ENDM




;------------------------------------------------------------------------------
;				データアドレスセット
;				データアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Loads #SRC_ADR with an immediate address.
;/// @ingroup bootrom
SET_DATA_SRC	MACRO
	lda	#LOW (\1)
	sta	<SRC_ADR
	lda #HIGH (\1)
    sta	<SRC_ADR+1
	ENDM

;------------------------------------------------------------------------------
;				データアドレスセット
;				データアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Loads #DST_ADR with an immediate address.
;/// @ingroup bootrom
SET_DATA_DST	MACRO
	lda	#LOW (\1)
	sta	<DST_ADR
	lda #HIGH (\1)
    sta	<DST_ADR +1
	ENDM


;------------------------------------------------------------------------------
;				データアドレスセット
;				データアドレス = \1 (16bit adr)
;				データアドレス = \2 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Loads both #SRC_ADR and #DST_ADR for a copy.
;/// @ingroup bootrom
SET_DATA_ADR	MACRO
	lda	#LOW (\2)
	sta	\1
	lda #HIGH (\2)
    sta	\1+1
	ENDM

;------------------------------------------------------------------------------
;				パレット転送
;				データアドレス = \1 (アドレス)
;------------------------------------------------------------------------------
;/// @brief Uploads a 32-byte palette through #PAL_WRK. @see transPALLET
;/// @ingroup bootrom
TRANS_PAL	MACRO
	lda  #LOW (\1)
	sta  <SRC_ADR
	lda  #HIGH (\1)
    sta  <SRC_ADR+1
	jsr  SET_PAL_DATA
	ENDM

;------------------------------------------------------------------------------
;				ネームテーブル＆パレット転送
;				データアドレス = \1 (バンク付アドレス)
;------------------------------------------------------------------------------
;/// @brief Copies a block of tile data into a nametable.
;/// @ingroup bootrom
DRAW_BG_DATA	MACRO
	lda  #LOW (\1)
	sta  <SRC_ADR
	lda  #HIGH (\1)
    sta  <SRC_ADR+1
	ldx  #BANK (\1) /2
	jsr  TRANS_NAME_PAL
	ENDM

;------------------------------------------------------------------------------
;				ネームテーブル転送
;				データアドレス = \1 (バンク付アドレス)
;------------------------------------------------------------------------------
;/// @brief As @c DRAW_BG_DATA, without setting the palette.
;/// @ingroup bootrom
DRAW_BG_DATA_NP	MACRO
	lda  #LOW (\1)
	sta  <SRC_ADR
	lda  #HIGH (\1)
    sta  <SRC_ADR+1
	ldx  #BANK (\1) /2
	lda  <A0_BNK		;
	pha			;現行のバンクを保存
	txa
	jsr  CHG_A0_BANK
	jsr  TRANS_NAMETBL_SUB

	pla
	jsr  CHG_A0_BANK
	ENDM


;------------------------------------------------------------------------------
;				文字列描画
;				\1 = 文字列
;------------------------------------------------------------------------------
;/// @brief Draws a string literal written inline at the call site.
;/// @details Pushes a fake return address so that execution resumes after the
;///          embedded text rather than trying to run it.
;/// @ingroup bootrom
DRAW_STRING2 MACRO
	LDA  #HIGH (.end\@ -1)
	PHA
	LDA  #LOW (.end\@ -1)
	PHA
	LDA  #LOW (.tbl\@ )
	STA  <SRC_ADR
	LDA  #HIGH (.tbl\@ )
	jmp  DRAW_STRING_SUB
.tbl\@:
    db  \1,0
.end\@:
	ENDM

;------------------------------------------------------------------------------
;				文字列描画
;				データソースアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Draws a NUL-terminated string from a labelled address.
;/// @ingroup bootrom
DRAW_STRING	MACRO
	lda	#\1 & $ff
	STA	<SRC_ADR
	LDA	#\1 >> 8
;;	STA	<SRC_ADR+1
	JSR	DRAW_STRING_SUB
	ENDM

;------------------------------------------------------------------------------
;				文字列描画（クリアー切り替え付き）
;				データソースアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Draws a string using the system tile set.
;/// @ingroup bootrom
DRAW_STRING_S	MACRO
	lda	#\1 & $ff
	STA	<SRC_ADR
	LDA	#\1 >> 8
;;	STA	<SRC_ADR+1
	JSR	DRAW_STRING_S_SUB
	ENDM

;------------------------------------------------------------------------------
;				文字列描画（テーブル選択型）
;				A reg = テーブル選択番号
;				テーブルデータアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Draws the string selected by an index into a table.
;/// @ingroup bootrom
DRAW_STRING_TBL_SEL	MACRO
	ASL	A
	TAX
	LDA	\1,X
        STA	<SRC_ADR
	LDA	\1+1,X
;;	STA	<SRC_ADR+1
	JSR	DRAW_STRING_SUB
	ENDM

;------------------------------------------------------------------------------
;/// @brief Flags the palette shadow dirty so @ref transPALLET uploads it next vertical blank.
;/// @ingroup bootrom
PAL_CHG	MACRO
	inc  <PAL_CHG_FG
	ENDM


;******************************************************************************
;	RTS関係
;******************************************************************************

;/// @brief Returns if the zero flag is set.
;/// @ingroup bootrom
rts_z	MACRO
	bne .tbl\@
	rts
.tbl\@:
	ENDM

;/// @brief Returns if the zero flag is clear.
;/// @ingroup bootrom
rts_nz	MACRO
	beq .tbl\@
	rts
.tbl\@:
	ENDM

;/// @brief Returns if the carry flag is set.
;/// @ingroup bootrom
rts_c	MACRO
	bnc .tbl\@
	rts
.tbl\@:
	ENDM

;/// @brief Returns if the carry flag is clear.
;/// @ingroup bootrom
rts_nc	MACRO
	bcs .tbl\@
	rts
.tbl\@:
	ENDM


;******************************************************************************
;	テーブル関係
;******************************************************************************

;------------------------------------------------------------------------------
;				テーブルジャンプ
;				A reg = ジャンプ先テーブル番号
;------------------------------------------------------------------------------
;/// @brief Dispatches through the jump table that follows.
;/// @details Pushes the target address minus one and executes `RTS`, which is
;///          the standard 6502 idiom for an indexed jump.
;/// @ingroup bootrom
TBL_JUMP	MACRO
	ASL	A
	stx  <TMP_SYS
	TAX
	LDA	.tbl\@+1,X
	PHA
	LDA	.tbl\@+0,X
	ldx  <TMP_SYS
	PHA
	RTS
.tbl\@:
	ENDM

;------------------------------------------------------------------------------
;  テーブルジャンプ先　宣言用マクロ　スタックを使う場合ジャンプ先-1 をプッシュ
;------------------------------------------------------------------------------
;/// @brief Declares one entry of a @c TBL_JUMP table.
;/// @ingroup bootrom
JPTBL	MACRO
	DW	\1 -1
	ENDM

;------------------------------------------------------------------------------
;				バンク付テーブルジャンプ
;				A reg = ジャンプ先テーブル番号
;------------------------------------------------------------------------------
;/// @brief Bank-aware @c TBL_JUMP. @note Degenerates to a plain jump on this NROM cartridge.
;/// @ingroup bootrom
BNK_TBL_JUMP	MACRO
	sta  <TMP_SYS
	stx  <TMP_SYS2
	asl a
	clc
	adc  <TMP_SYS
	tax

	; 各ミッションタイプに応じた初期化処理を呼び出し
	lda  .tbl\@+1,x
	pha
	lda  .tbl\@+0,x
	pha

	lda  .tbl\@+2,x		;BANK
	jsr  SYS_CHG_BANK
	ldx  <TMP_SYS2
	rts
.tbl\@:
	ENDM

;------------------------------------------------------------------------------
;  バンク付ジャンプ先テーブル
;------------------------------------------------------------------------------
;/// @brief Declares one entry of a @c BNK_TBL_JUMP table.
;/// @ingroup bootrom
BNK_JPTBL2	MACRO
	DW	(\1 -1)
	DB  #BANK ( \1 ) / 2
	ENDM



;------------------------------------------------------------------------------
;  バンク付アドレステーブル
;------------------------------------------------------------------------------
;BNK_ADR	MACRO
;	DW	\1
;	DB  #BANK ( \1 ) /2
;	ENDM





;------------------------------------------------------------------------------
;  バンク間コール ※A000 に配置された他バンクのサブルーチンを呼び出す
;				コール先アドレス = \1 (16bit adr + bank)
;------------------------------------------------------------------------------
;BNK_CALL	MACRO
;	jsr	BANK_CALL_SUB
;	DW	(\1 -1)
;	DB  #BANK ( \1 ) / 2
;	ENDM

;/// @brief Calls a routine in another bank. @note A plain `jsr` here; NROM has no banking.
;/// @ingroup bootrom
BNK_CALL	MACRO
	jsr	\1
	ENDM


;=================================================================
; 				ジャンプベクターチェック付きCALL
;=================================================================
;/// @brief Calls through a jump vector, first checking it holds a `JMP` opcode.
;/// @note Guards against calling into an erased or unprogrammed ROM region.
;/// @ingroup bootrom
JVC_CALL MACRO
	lda  \1
	cmp  #$4C
	bne  .iend\@
	jsr  \1
.iend\@:
	ENDM

;=================================================================
; 				ジャンプベクターチェック付きJMP Areg 破壊
;=================================================================
;/// @brief Jumps through a validated jump vector. @see JVC_CALL
;/// @ingroup bootrom
JVC_JMP MACRO
	lda  \1
	cmp  #$4C
	bne	.iend\@
	jmp  \1
.iend\@:
	ENDM


;=================================================================
; 				ジャンプベクターチェック付きJMP Xreg 破壊
;=================================================================
;/// @brief Jumps through a validated jump vector selected by X.
;/// @ingroup bootrom
JVC_JMPX MACRO
	ldx  \1
	cpx  #$4C
	bne	.iend\@
	jmp  \1
.iend\@:
	ENDM


;=================================================================
; 				空のジャンプベクター
;=================================================================
;/// @brief Placeholder jump vector for an entry that is not implemented.
;/// @ingroup bootrom
DMY_JVC_JMP MACRO
	db $FF,$FF,$FF
	ENDM

;=================================================================
; 				NMIユーザー処理登録用マクロ
;=================================================================
;/// @brief Installs a user vertical-blank hook. @see NMI_CALL_ADR
;/// @ingroup bootrom
SET_NMI_CALL	MACRO
	LDA	#HIGH (\1)
	STA <NMI_CALL_ADR+1
	LDA	#LOW  (\1)
	STA <NMI_CALL_ADR
	LDA	#BANK (\1) /2
	STA <NMI_CALL_BNK
	ENDM


;=================================================================
; 				NMIユーザー処理解除用マクロ
;=================================================================
;/// @brief Removes the user vertical-blank hook.
;/// @ingroup bootrom
CLR_NMI_CALL	MACRO
	LDA	#0
	STA <NMI_CALL_ADR+1
	ENDM

;=================================================================
; 				RAM転送システム用のマクロ
;=================================================================

;------------------------------------------------------
; VRAM転送先アドレス指定マクロ
;------------------------------------------------------
;/// @brief Sets the address for the VRAM transfer queue. @note Unused in this build.
;/// @ingroup bootrom
SET_VRAMT_ADR	MACRO
	lda   #HIGH (\1)
	jsr   writeVRAMT_DATA
	lda   #LOW  (\1)
	jsr   writeVRAMT_DATA
	ENDM


;------------------------------------------------------------------------------
;				テーブル選択
;				A reg = テーブル選択番号
;				テーブルアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Selects a table entry by index.
;/// @ingroup bootrom
TBL_SELECT	MACRO
	ASL	A
	TAX
	LDA	\1,X
        STA	<SRC_ADR
	LDA	\1+1,X
        STA	<SRC_ADR+1
	ENDM

;------------------------------------------------------------------------------
;				テーブル選択
;				A reg = テーブル選択番号
;				テーブルアドレス = \1 (16bit adr)
;------------------------------------------------------------------------------
;/// @brief Selects a table entry by index, 16-bit variant.
;/// @ingroup bootrom
TBL_SELECT2	MACRO
	ASL	A
	TAX
	LDA	\1,X
        STA	<DST_ADR
	LDA	\1+1,X
        STA	<DST_ADR +1
	ENDM



;-------------------------------------------------------------------------------
; H-IRQ用マクロ
;-------------------------------------------------------------------------------
; H-IRQエントリ設定
;/// @brief Installs the scanline IRQ entry point. @note Vestigial; no such hardware here.
;/// @ingroup bootrom
HIRQ_ENTRY_SET	macro
		lda	#LOW(\1)
		sta	<HIRQ_ENTRY_ADR
		lda	#HIGH(\1)
		sta	<HIRQ_ENTRY_ADR+1
		endm


; H-IRQ関数設定
;/// @brief Installs the scanline IRQ handler. @note Vestigial.
;/// @ingroup bootrom
HIRQ_FUNC_SET	macro
		lda	#LOW(\1)
		sta	<HIRQ_FUNC_ADR
		lda	#HIGH(\1)
		sta	<HIRQ_FUNC_ADR+1
		endm


;-------------------------------------------------------------------------------
; メモリー制御
;-------------------------------------------------------------------------------

;   A reg クリアーデータ
;   Y reg クリアーサイズ ( 0 は 256バイト )
;	\1 = クリアーメモリーアドレス (16bit adr)

;/// @brief Fills a memory range with zero.
;/// @ingroup bootrom
CLEAR_MEM	macro

.loop\@:
	dey
	sta   \1,Y
	bne   .loop\@

		endm


;/// @brief Copies a table of bytes into RAM.
;/// @ingroup bootrom
COPY_TBL	macro
	ldx  #0
.loop\@:
	lda  \1,x
	sta  \2,x
	inx
	cpx  #\3
	bne   .loop\@
		endm


;	\1 = 転送元アドレス (BNK+16bit adr)
;	\2 = 転送先アドレス (16bit adr)
;	\3 = 転送サイズ (8bit)

;/// @brief Copies a block of memory, 8-bit length.
;/// @ingroup bootrom
COPY_MEM	macro
	lda  #LOW (\1)
	sta  <SRC_ADR
	lda  #HIGH (\1)
    sta  <SRC_ADR+1

	lda  #LOW (\2)
	sta  <DST_ADR
	lda  #HIGH (\2)
    sta  <DST_ADR+1

	ldy  #(\3)
	jsr  memcpy

	endm

;	\1 = 転送元アドレス (BNK+16bit adr)
;	\2 = 転送先アドレス (16bit adr)
;	\3 = 転送サイズ (16bit)

;/// @brief Copies a block of memory, 16-bit length.
;/// @ingroup bootrom
COPY_MEM16	macro
	lda  #LOW (\1)
	sta  <SRC_ADR
	lda  #HIGH (\1)
    sta  <SRC_ADR+1

	lda  #LOW (\2)
	sta  <DST_ADR
	lda  #HIGH (\2)
    sta  <DST_ADR+1

	ldy  #LOW (\3)
	ldx  #HIGH (\3)

	lda  #BANK (\1) /2

	jsr  memcpy_x

	endm



;-------------------------------------------------------------------------------
; BEEP
;-------------------------------------------------------------------------------

;/// @brief Emits a tone by writing the APU pulse registers directly.
;/// @note Used for boot progress cues before the sound driver exists.
;/// @ingroup bootrom
BEEP	MACRO
	lda #0
	sta $4015

;	lda $4015		; サウンドレジスタ
;	ora #%00000001	; 矩形波チャンネル１を有効にする
	lda #%00000001	; 矩形波チャンネル１を有効にする
	sta $4015

	lda #%10111111	; Duty比・長さ無効・減衰無効・減衰率
	sta $4000	; 矩形波チャンネル１制御レジスタ１

;	lda #%10101011	; スイープ有効・変化率・方向・変化量
;	lda #%00101011	; スイープ有効・変化率・方向・変化量
	lda #(\2)	; スイープ有効・変化率・方向・変化量
	sta $4001	; 矩形波チャンネル１制御レジスタ２
	
	lda #LOW(\1)	; 周波数(下位8ビット)
	sta $4002	; 矩形波チャンネル１周波数レジスタ１
	lda #%11111000 +(HIGH(\1) &7)	; 再生時間・周波数(上位3ビット)
	sta $4003	; 矩形波チャンネル１周波数レジスタ２

	ENDM



;-------------------------------
; Areg = セットするライン数
;-------------------------------
;/// @brief Reloads the scanline IRQ counter. @note Vestigial; no such hardware here.
;/// @ingroup bootrom
HIRQ_LATCH_RELOAD	MACRO
	sta  <HIRQ_CNT
	sta  EXS_HIRQ_REG		; H-IRQラインオフセット設定
;	lda  <EXA_MODE
;	bne  .iend\@
;	lda  #1
;	sta  <HIRQ_CNT
;.iend\@:
	ENDM


;/// @brief Acknowledges a scanline IRQ. @note Vestigial.
;/// @ingroup bootrom
HIRQ_END	MACRO
	lda  #$FF
	sta  EXS_HIRQ_REG		; H-IRQラインオフセット設定
	sta  <HIRQ_CNT
	ENDM


;-------------------------------
; ROMミラー設定マクロ
;-------------------------------
;/// @brief Emits the literal marker `nes_mirror` for a post-processing tool.
;/// @note The tool (`bin/nes_mirror.exe`) is never invoked by any build script
;///       in this project. @see @ref conventions
;/// @ingroup bootrom
ROM_MIRROR 	macro
	db "nes_mirror",\1
	dw \2
	ds \2 -13

	ENDM


;-------------------------------
; バンク切り替え
;-------------------------------
;/// @brief Selects a PRG bank from A. @note Inert on NROM; the write lands on the board's write-enable latch. @see @ref flashing
;/// @ingroup bootrom
CHG_BANK_A 	macro
	sta  $E000
	ENDM

;/// @brief Selects a PRG bank from X. @note Inert on NROM.
;/// @ingroup bootrom
CHG_BANK_X 	macro
	jsr  SYS_CHG_BANK_X
;	stx  $8000
	ENDM

;-------------------------------
; バンク切り替え
;-------------------------------
;/// @brief Selects a PRG bank from a literal. @note Inert on NROM.
;/// @ingroup bootrom
CHG_BANK_LB macro
	lda  #BANK (\1) /2
	sta  $E000
	ENDM



;-------------------------------
; 現在のバンクをスタックにPUSH
;-------------------------------
;/// @brief Saves the current bank number. @note Inert on NROM.
;/// @ingroup bootrom
PUSH_BANK macro
	lda  ROM_BANK_NO
	pha
	ENDM


;-------------------------------
; 現在のスタックからPOPしてバンク切り替え
;-------------------------------
;/// @brief Restores a saved bank number. @note Inert on NROM.
;/// @ingroup bootrom
POP_BANK macro
	pla
	jsr  SYS_CHG_BANK
	ENDM


;-------------------------------
; サウンドデータテーブルマクロ
;-------------------------------
;/// @brief Emits a data byte for the model/table description format.
;/// @ingroup bootrom
MDR_DT	MACRO
	DW \1
	DB \2
	DB \3
	ENDM


;-------------------------------
; サウンドデータテーブルマクロ
;-------------------------------
;/// @brief Stops dead with a repeating beep.
;/// @details Masks interrupts and loops forever. Intended as a visible, audible
;///          failure rather than a silent hang.
;/// @ingroup bootrom
DEBUG_HALT MACRO
	sei
	BEEP $104,%11110011
.halt\@
	jmp  .halt\@
	ENDM

;******************************************************************************
;	NoNMI関係
;******************************************************************************

;/// @brief Polls `$2002` until vertical blank begins. For use with NMI disabled.
;/// @ingroup bootrom
WAIT_VBLANK MACRO
.wait_loop\@
	bit	 $2002  ;ppu__status
	bpl	.wait_loop\@
	ENDM

;/// @brief Polls `$2002` until vertical blank ends.
;/// @ingroup bootrom
WAIT_VBLANK_END MACRO
.wait_loop\@
	bit	 $2002  ;ppu__status
	bmi	.wait_loop\@
	ENDM

;/// @brief Zeroes both scroll registers.
;/// @ingroup bootrom
RESET_SCR_XY MACRO
	lda  #0
	sta  $2005
	sta  $2005
	ENDM

;******************************************************************************
;	EXA関係
;******************************************************************************

;/// @brief Reads expansion adapter status from `$5000`. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
RSTAT_EXA MACRO
	lda  $5000
	ENDM


;******************************************************************************
;	TIMEOUT関連
;******************************************************************************
;-------------------------------------------------------------------------------
; TIMEOU値セット
;-------------------------------------------------------------------------------
;	\1 = タイムアウト秒数 (1-68秒)
;/// @brief Arms #DEMO_TIMER for the given number of seconds.
;/// @ingroup bootrom
SET_TIMEOUT macro
	lda  #( \1 *60/16)
	sta  <DEMO_TIMER
	ENDM

;/// @brief Decrements #DEMO_TIMER and branches when it reaches zero.
;/// @ingroup bootrom
JOB_TIMEOUT MACRO
	lda  <DEMO_TIMER
	beq  .lpx\@
	LDA  <SYS_TIMER
	AND  #$0f
	BNE	 .lpx\@
	dec  <DEMO_TIMER
.lpx\@
	ENDM

;/// @brief Branches when #DEMO_TIMER has expired.
;/// @ingroup bootrom
IS_TIMEOUT macro
	lda  <DEMO_TIMER
	ENDM


