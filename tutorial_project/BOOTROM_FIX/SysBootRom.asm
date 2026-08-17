;=====================================================
;
;	起動時初期化処理
;
;=====================================================



;===================================================================
;	ファミコン初期化
;===================================================================
BR_INIT:
	sei
	cld
	ldx	#$ff
	txs

	lda  #0
	sta	 $2000
	sta  $8000

	ldx  #0		; =ldx #0
	stx  $2000
	stx  $2001
	stx  $4015
	stx	 $4010  ;apu__dmc_control
	lda	#(%1<<6)
	sta	 $4017  ;apu__frame


	;===============================
	;	グラフィック関連初期化
	;===============================
.v2:
	bit	 $2002  ;ppu__status
	bpl	.v2


	lda	 $2002  ;ppu__status		; 1st-write
	lda	#$10
	tax
.toggle_ppu_a12:
	sta	 $2006  ;ppu__address
	sta	 $2006  ;ppu__address
	eor	#$10
	dex
	bne	.toggle_ppu_a12
	stx	 $2001  ;ppu__control2

	bit	 $2002  ;ppu__status
.v:	bit	 $2002  ;ppu__status
	bpl	.v


	;===============================
	;	スクロールレジスタ初期化
	;===============================
	lda  #0
	sta  $2005
	sta  $2005

	;===============================
	;	PICOリセット
	;===============================
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_RST
	sta  $2007

	;===============================
	; スプライト初期化
	;===============================
	lda  #0
	sta  $2003
	ldx  #$F1
	ldy  #64
.loop_spclr
	sta  $2004
	stx  $2004
	sta  $2004
	sta  $2004
	dey
	bne  .loop_spclr

	;===============================
	;	パレットクリア
	;===============================
	; 全て白にしてリセット時のゴミを隠す
	SET_VRAM_ADD2 #$3F00
	ldy	#32
	lda	#$30			; 白
	jsr  .SYS_VRAM_WLP

	SET_VRAM_ADD2 #$3F00
	ldy  #1
	lda  #$1F		; 黒
	jsr  .SYS_VRAM_WLP

	;===============================
	; ネームテーブル初期化
	;===============================
	jsr  .SYS_CLEAR_BG
	SET_VRAM_ADD2 #$2C00
	jsr  .SYS_CLEAR_BG2


	;===============================
	; 初期キャラデータ転送
	;===============================

	jsr  TRANS_SYS_FONT



	ldx  #0
.chk_ram100:
	lda  #$00
	sta  $100,x
	inx
	cpx  #-3
	bne  .chk_ram100

	;=======================
	; メモリークリア       *
	;=======================
.ram_clear:
	lda  #0
	tax				; =lda #0
.CLR_LOP:
	sta	<$00 ,x

; この領域に置いたハイスコアをリセット時にも保持させるため、
;        STA     $0100,X
	sta  $0200,X
	sta  $0300,X
	sta  $0400,X
	sta  $0500,X
	sta  $0600,X
	sta  $0700,X
	inx
	bne  .CLR_LOP


	jsr  .SYS_CLEAR_SP	; スプライトを画面外に初期化


	;===============================
	; 初回キー入力チェック
	;===============================
	; WRAM 強制初期化操作に必要
	; 連続 2 フレームで同じキーが押されていないと
	; 「押された」と判定されないため、2 回の読込みを行わせる。
	jsr  KEY_RTN
	jsr  KEY_RTN

	lda  <KEY_NEW
	cmp  #KEY_RUN
;	cmp  #KEY_RUN | KEY_SEL
	beq  .rom_erace 

 .if !DEBUG_BUILD

	lda  IS_ROM_ERACE
	bne  .rom_update

	;===============================
	; メモリーテスト
	;===============================
	jsr  BOOT_MEMTEST

	;===============================
	; ROMバージョンチェック
	;===============================
	jsr  CHK_ROMVER
	bcs  .rom_erace 

 .endif



	jsr  MAIN_SETUP


	; PPU 制御フラグ 1 初期化
	lda	#FLG_PPU2000
	sta	<FLG_2000
	sta	 $2000				; このタイミングでNMI発生

	sei

	jmp  MAIN_LOOP

.rom_update
	jmp  ROM_UPDATE

.rom_erace
	jmp  ROM_ERACE



;==============================================================================
;
;					起動ROM用サブルーチン
;
;==============================================================================


.SYS_CLEAR_SP:
	ldy  #0
	lda #SP_CLR_Y
.spclr_loop
	sta $200,y
	iny
	iny
	iny
	iny
	bne .spclr_loop
.end
	rts


;=======================
; VRAM CLEAR
;=======================
.SYS_CLEAR_BG:
	SET_VRAM_ADD2 #$2000
.SYS_CLEAR_BG2:
	lda	#$00
	ldy	#0
	jsr .SYS_VRAM_WLP
	jsr .SYS_VRAM_WLP
	jsr .SYS_VRAM_WLP

.SYS_VRAM_WLP:
	sta  $2007
	dey
	bne  .SYS_VRAM_WLP
	rts


;------------------------------------------------------------------------------
;				文字列描画
;				\1 = 文字列
;------------------------------------------------------------------------------
BR_DRAW_STRING2 MACRO
	LDA  #HIGH (.end\@ -1)
	PHA
	LDA  #LOW (.end\@ -1)
	PHA
	LDA  #LOW (.tbl\@ )
	STA  <SRC_ADR+0
	LDA  #HIGH (.tbl\@ )
	sta  <SRC_ADR+1
	jmp  BR_DRAW_STRING_SUB
.tbl\@:
    db  \1,0
.end\@:
	ENDM

BR_DRAW_STRING_SUB:
	ldy  #0
.drst00:
	lda  [SRC_ADR],Y
	beq  .drst01
	cmp  #' '
	bne  .set
	lda  #0
.set
	sta  $2007
	iny
	bne  .drst00
.drst01:
	rts

;=======================
; 16進数　数字描画
;   SET_VRAM で転送先VRAMアドレスを指定
;   A reg 描画する数値
;=======================
BR_DRAW_HEX_BYTE:
	TAY
	LSR A
	LSR A
	LSR A
	LSR A
	JSR	.DRAW_HEX_BYTE2
	TYA
.DRAW_HEX_BYTE2:
	jsr  .convHEX2
	sta  $2007
	rts

.convHEX2:
	and  #$0f
	cmp  #10
	bcs  .drhx00
	clc
	adc  #'0'
	bne  .drhx01
.drhx00:
	clc
	adc #'A' -10
.drhx01:
	rts


BR_PICO_COM_WAIT:
	ldx  #0
.wait
	dex
	bne  .wait
	rts


BR_ERROR_EMD:
	DEBUG_HALT
	rts


;==============================================================================
;
;					ROMバージョンチェック
; CFlag = ON：ROMバージョンアップ
;==============================================================================
CHK_ROMVER:

	lda  #0
	sta	 $2000
	sta	 $2001

	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_VER
	sta  $2007

	jsr  BR_PICO_COM_WAIT

	; PICOからデータ取得
	SET_VRAM_ADD2	#$0800
	lda  $2007	; ダミーリード
.head
	lda  $2007	; ヘッダーチェック
	cmp  #'C'
	bne  .head

	ldx  #0
.loop
	lda  $2007
	sta  PICO_DATA_BUF,x
	inx
	cpx  #$10
	bne  .loop

	jsr  BR_VBLANK_END


	lda  #%000_01_0_00		; NO-NMI
	sta	 $2000
	lda  #%000_01_110
	sta	 $2001

 .if 0
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*4 + 1
	SET_DATA_SRC  DB_ROM_VER
	jsr  BR_DRAW_STRING_SUB

	SET_VRAM_ADD2	#$2000 + 32*6 + 1
	SET_DATA_SRC  PICO_DATA_BUF
	jsr  BR_DRAW_STRING_SUB

	jsr  BR_VBLANK_END

	ldx  #60*2
	jsr  BR_WAIT_VBLANK_X		; 表示ウェイト
 .endif


	; ROMバージョン一致チェック
	lda  #0
	tax
	tay

.loop2
	lda  DB_ROM_VER,x
	cmp  PICO_DATA_BUF,y
	bne  .romverup
	iny
	inx
	cpx  #$0E
	bne  .loop2

	clc
	rts

.romverup
 .if 1
	inc  <PICO_MODE
	lda  <PICO_MODE
	cmp  #5				; retry
	beq  .romv00
 .endif

 .if 1
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*4 + 1
	SET_DATA_SRC  DB_ROM_VER
	jsr  BR_DRAW_STRING_SUB

	SET_VRAM_ADD2	#$2000 + 32*6 + 1
	SET_DATA_SRC  PICO_DATA_BUF
	jsr  BR_DRAW_STRING_SUB

	jsr  BR_VBLANK_END

	ldx  #30*1
	jsr  BR_WAIT_VBLANK_X		; 表示ウェイト
 .endif
	jmp CHK_ROMVER

.romv00

	lda  PICO_DATA_BUF+0
	cmp  #'2'
	bne  .no_pico
	lda  PICO_DATA_BUF+1
	cmp  #'0'
	bne  .no_pico

	lda  #0
	sta	 $2000
	sta	 $2001

;	clc
	sec
	rts


.no_pico
	jsr  BR_VBLANK_START

	SET_VRAM_ADD2	#$2000 + 32*8 + 1

	BR_DRAW_STRING2 "PICO NOT FOUND"

	jsr  BR_VBLANK_END
	jmp  BR_ERROR_EMD




;==============================================================================
;
;					起動時　メモリーテスト
;
;==============================================================================
BOOT_MEMTEST:

	lda  #%000_01_0_00		; NO-NMI
	sta	 $2000
	lda  #%000_01_110
	sta	 $2001

	ldx  #30
	jsr  BR_WAIT_VBLANK_X		; 拡張RAMが安定するまで0.5秒ウェイト


	jsr  BEEP_PI
	ldx  #5
	jsr  BR_WAIT_VBLANK_X
	jsr  BEEP_PO

	SET_VRAM_ADD2	#$2000 + 32*2 + 1
	BR_DRAW_STRING2 "MEMORY"

	jsr  .chkExRAM	; 拡張RAMチェック
	bcs  .skip_01

	SET_VRAM_ADD2	#$2000 + 32*2 + 14
	BR_DRAW_STRING2 "OK"

.skip_01

	ldy  #0

.memchk_loop
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*2 + 7
	jsr  .MEMCHK_STR
	jsr  BR_VBLANK_END

	cpy  #8
	beq  .memchk_loop_end

	jsr  MEMCHK_SUB
	jsr  .add_memchk_str
	iny
	bne  .memchk_loop
.memchk_loop_end

	jsr  .chkExRAM	; 拡張RAMチェック
	bcs  .skip_exram00
	jmp  .skip_exram
.skip_exram00
	;------------------------
	; EXRAMチェック
	;------------------------

;	ldx  #60
;	jsr  WAIT_VBLANK_X

	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*2 + 14
	lda  #'+'
	sta  $2007
	lda  #0
	sta  $2007
	
	SET_VRAM_ADD2	#$2000 + 32*2 + 23-1
	BR_DRAW_STRING2 "OK"

	jsr  BR_VBLANK_END

	ldy  #0
	sty  <MEM_DISP+0
	sty  <MEM_DISP+1
	sty  <MEM_DISP+2
	sty  <MEM_DISP+3
	sty  <MEM_DISP+4

	sty  <RAM_PAGE

.memchk_loop3
;@	lda  <RAM_PAGE
;@	jsr  setWRAM_BANK

	ldy  #$60
.memchk_loop2
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*2 + 16-1
	jsr  .MEMCHK_STR

	jsr  BR_VBLANK_END

	cpy  #$80
;	cpy  #$81	; NG動作チェック用
	beq  .memchk_loop2_end


	jsr  MEMCHK_SUB
	jsr  .add_memchk_str
	jsr  MEMCHK_SUB
	jsr  .add_memchk_str
	jsr  MEMCHK_SUB
	jsr  .add_memchk_str
	jsr  MEMCHK_SUB
	jsr  .add_memchk_str
	
	iny
	iny
	iny
	iny
	bne  .memchk_loop2

.memchk_loop2_end
;@	inc  <RAM_PAGE
;@	lda  #4
;@	cmp  <RAM_PAGE
;@	bne  .memchk_loop3

;@	lda  #0
;@	jsr  setWRAM_BANK

.skip_exram
	
	ldx  #60
	jsr  BR_WAIT_VBLANK_X


	rts

;===============================
;	拡張RAMチェック ExRAMあればCFlag =1
;===============================
.chkExRAM:
	; 拡張RAMチェック
	lda  $6000
	tay
	eor  #$FF
	sta  $6000
	cmp  $6000
	sty  $6000
	beq  .skip_00
	clc
	rts

.skip_00
	sec
	rts


;--------------------------------
; メモリーチェック文字列セット
; Y reg = 表示文字列インデックス
;--------------------------------
.MEMCHK_STR:
	sty  <TMP_SVY
	lda  #0
	sta  <TMP_SVA
	ldy  #'0'
	clc


	lda  <MEM_DISP+4
	beq  .ms03
	sty  <TMP_SVA
.ms03
	adc  <TMP_SVA
	sta  $2007
	
	lda  <MEM_DISP+3
	beq  .ms00
	sty  <TMP_SVA
.ms00
	adc  <TMP_SVA
	sta  $2007
	
	lda  <MEM_DISP+2
	beq  .ms01
	sty  <TMP_SVA
.ms01
	adc  <TMP_SVA
	sta  $2007
	
	lda  <MEM_DISP+1
	beq  .ms02
	sty  <TMP_SVA
.ms02
	adc  <TMP_SVA
	sta  $2007

	lda  <MEM_DISP+0
	adc  #'0'
	sta  $2007

	lda  #'B'
	sta  $2007
	ldy  <TMP_SVY
	rts


.add_memchk_str
	stx  <TMP_SVX

	; MEM_DISPに256加算
	clc
	lda  <MEM_DISP+0
	adc  #6
	sta  <MEM_DISP+0
	
	lda  <MEM_DISP+1
	adc  #5
	sta  <MEM_DISP+1
	
	lda  <MEM_DISP+2
	adc  #2
	sta  <MEM_DISP+2
	
	; 10進数補正
	ldx  #0
.loop_ms00
	lda  MEM_DISP,x
	cmp  #10
	bcc  .next_ms00
	sbc  #10
	sta  MEM_DISP,x
	inc  MEM_DISP+1,x
.next_ms00
	inx
	cpx  #5
	bne  .loop_ms00

	ldx  <TMP_SVX
	rts


;--------------------------------
; メモリーチェック文字列セット
; Y reg = チェックアドレスインデックス
;--------------------------------
MEMCHK_SUB:
	cpy  #0
	bne  .ram_nozp

.loop_zp
	lda  $0000,y
	eor  #$FF
	sta  $0000,y
	cmp  $0000,y
	bne  .MEMCHK_ERROR
	eor  #$FF
	sta  $0000,y
	cmp  $0000,y
	bne  .MEMCHK_ERROR

	iny
	bne  .loop_zp
	rts

.ram_nozp
	tya
	pha
;	cmp  #8
;	beq  .skip
	sta  <DST_ADR+1
	ldy  #0
	sty  <DST_ADR+0
.loop_nozp
	lda  [DST_ADR],y
	eor  #$FF
	sta  [DST_ADR],y
	cmp  [DST_ADR],y
	bne  .MEMCHK_ERROR
	eor  #$FF
	sta  [DST_ADR],y
	cmp  [DST_ADR],y
	bne  .MEMCHK_ERROR

	iny
	bne  .loop_nozp
;.skip
	pla
	tay
	rts


.MEMCHK_ERROR:
	SET_VRAM_ADD2	#$2000 + 32*12 + 14
	lda  <DST_ADR+1
	cmp  #$40
	bcc  .mce_00

	SET_VRAM_ADD2	#$2000 + 32*12 + 23-1
.mce_00
	
	lda  #'N'
	sta  $2007
	lda  #'G'
	sta  $2007
	BEEP $104,%11110011
.loop_me
	lda  #0
	jsr  BR_WAIT_VBLANK_X

	jmp  .loop_me



;==============================================================================
;
;					ROM消去システム
;
;==============================================================================
ROM_ERACE:
	jsr  BEEP_PI
	lda  #%000_01_0_00		; NO-NMI
	sta	 $2000
	lda  #%000_01_110
	sta	 $2001
	SET_DATA_DST  $8000
	
.loop
	ldy  #0
	lda  [DST_ADR],y
	cmp  #$FF
	bne  .erace

	inc  <DST_ADR +0
	bne  .loop
	inc  <DST_ADR +1
	lda  <DST_ADR +1
	cmp  #$F0			; 消去終了アドレス
	beq  .loop_end
	bne  .loop

.erace
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*4 + 1
	BR_DRAW_STRING2 "ROM ERACE "
	lda  <DST_ADR+1
	and  #$F0
	jsr  BR_DRAW_HEX_BYTE
	lda  #0
	jsr  BR_DRAW_HEX_BYTE

	lda  #0
	sta  $2007
	sta  $2007
	
	jsr  BR_VBLANK_END

	; 消去コマンド実行
	jsr  CPU_FlashSectorElase
	jmp  .loop

.loop_end
	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*4 + 1
	BR_DRAW_STRING2 "ROM ERACE END "
	jsr  BR_VBLANK_END

	ldx  #60*1
	jsr  BR_WAIT_VBLANK_X		; 拡張RAMが安定するまで0.5秒ウェイト

	jmp  INIT


;-----------------------------------
; CPU FLASHセクターイレースコマンド
;-----------------------------------
 .if 0
このシーケンスは同時に消去するセクタのアドレスにセクタ消去コマンド（30h）を
引き続きライトさせることで行います。最後のセクタ消去コマンドのWE 立上りから
50 ms のタイムアウト期間終了によりセクタ消去が開始されます。つまり，複数のセク
タを同時に消去する場合は，次の消去セクタをそれぞれ50 ms 以内に入力する必要があり，
それ以後ではコマンドは受け付けられないことがあります。引き続くセクタ消去コマンドが
有効かどうかはDQ3 にてモニタ可能です（「3．ライト動作状態（5）DQ 3・セクタ消去タイマ」参照）

 .endif

;-----------------------------------
CPU_FlashSectorElase:
	ldy  #0
.loop_cpy
	lda  .mem_exec_st,y
	sta  FLASH_EXEC_BUF,y
	iny
	cpy  #(.mem_exec_end - .mem_exec_st)
	bne  .loop_cpy
	php
	sei
	jsr  FLASH_EXEC_BUF

	plp
	rts
	
.mem_exec_st
	lda  #$AA
	sta  $D555

	lda  #$55
	sta  $AAAA

	lda  #$80
	sta  $D555
	
	lda  #$AA
	sta  $D555
	
	lda  #$55
	sta  $AAAA

	ldy  #0
	lda  #$30
	sta  [DST_ADR],y

	; Q3 タイムアウトビットチェック
.loop_to
	lda  $8000 + $000
	and  #$08
	bne  .loop_to

	; Q6 トグルビットチェック

.loop_tc
	lda  $8000 + $000
	bpl  .loop_tc

.write_end
.time_limit_over

	; セクター消去中断
	rts
.mem_exec_end



;==============================================================================
;
;					ROM更新システム
;
;==============================================================================
ROM_UPDATE:
	jsr  BEEP_PO
	lda  #%000_01_0_00		; NO-NMI
	sta	 $2000
	lda  #%000_01_110
	sta	 $2001


	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*2 + 1
	BR_DRAW_STRING2 "ROM UPDATE"
	jsr  BR_VBLANK_END

	ldx  #60*2
	jsr  BR_WAIT_VBLANK_X

	jsr  BR_VBLANK_START

	
	; 表示OFF
	lda  #%000_00_110
	sta	 $2001


	SET_DATA_SRC  FLASH_SAVE_BUF
	SET_DATA_DST  $8000

	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_ROM
	sta  $2007
	jsr  BR_PICO_COM_WAIT
	lda  <DST_ADR+1
	sta  $2007


.loop

	;----------------------------
	; ROM データリクエスト
	;----------------------------
	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_ROM
	sta  $2007
	lda  <DST_ADR+1
	sta  $2007

	jsr  BR_PICO_COM_WAIT

	; PICOからデータ取得
	SET_VRAM_ADD2	#$0800
	lda  $2007	; ダミーリード
;.head
;	lda  $2007	; ヘッダーチェック
;	cmp  #'C'
;	bne  .head


	; 書き込みコマンド実行
	lda  #0
	sta  <W_AR+0
	jsr  CPU_FlashPorgram

	
	inc  <DST_ADR +1
	lda  <DST_ADR +1
	cmp  #$F0			; 消去終了アドレス
	bne  .loop


.loop_end
	; 表示ON
	lda  #%000_01_110
	sta	 $2001

	jsr  BR_VBLANK_START
	SET_VRAM_ADD2	#$2000 + 32*2 + 1
	BR_DRAW_STRING2 "ROM UPDATE END "
	jsr  BR_VBLANK_END

	ldx  #60*1
	jsr  BR_WAIT_VBLANK_X

	jmp  INIT
;	DEBUG_HALT



;-----------------------------------
; CPU FLASHセクタープログラム
;-----------------------------------
; in:  SRC_ADR データアドレス
; in:  DST_ADR 書き込むアドレス
; in:  W_AR+0  書き込むサイズ
;
; zフラグがNZならエラー終了

CPU_FlashPorgram:
	ldy  #0
.loop_cpy
	lda  .mem_exec_st,y
	sta  FLASH_EXEC_BUF,y
	iny
	cpy  #(.mem_exec_end - .mem_exec_st)
	bne  .loop_cpy
	php
	sei
	jsr  FLASH_EXEC_BUF
	plp
	rts
	
.mem_exec_st

	ldy  #0
.loop_write
	lda  [DST_ADR],y
	cmp  #$FF
	bne  .error_end

	lda  #$AA
	sta  $D555

	lda  #$55
	sta  $AAAA

	lda  #$A0
	sta  $D555
	
	lda  $2007	; ダミーリード
	sta  [SRC_ADR],y
	sta  [DST_ADR],y

;	BEEP $104,%11110011

	; Q6 トグルビットチェック
.loop_tc
	lda  $8000 + $000
	cmp  $8000 + $000
	bne  .loop_tc

	; 強制リードモード

	lda  [SRC_ADR],y
	cmp  [DST_ADR],y
	bne  .error_end

	iny
	cpy  <W_AR+0
	bne  .loop_write
	rts

.error_end
	jmp  BR_ERROR_EMD

.mem_exec_end


;==============================================================================
;
;					システムFONT
;
;==============================================================================
BR_TRANS_SYS_FONT:
	inc  <NMI_FLG

	SET_DATA_SRC CHR_SYS_FONT
	SET_VRAM_ADD2 $0000

	ldx  #128
.loop
	jsr  .SYS_FONT_TRANS
	dex
	bne  .loop

	SET_DATA_SRC CHR_SYS_FONT
	SET_VRAM_ADD2 $1000
	
	ldx  #128
.loop2
	jsr  .SYS_FONT_TRANS
	dex
	bne  .loop2
	
	rts

;=======================
; FONT DATA TRANS
;=======================

.SYS_FONT_TRANS:
	jsr  .sft_sub
	jsr  .sft_sub
	lda  <SRC_ADR+0
	clc
	adc  #8
	sta  <SRC_ADR+0
	lda  <SRC_ADR+1
	adc  #0
	sta  <SRC_ADR+1
	rts

.sft_sub
	ldy  #0
.sft_loop
	lda  [SRC_ADR],y
	sta  $2007
	iny
	cpy  #8
	bne  .sft_loop
	rts



CHR_SYS_FONT:
	.INCBIN		"font1b_0.chr"

;==============================================================================
;
;					NMI禁止型　画面表示システム　BOOTROM用
;
;==============================================================================

;--------------------------------
; Xレジで指定フレームウェイト
;--------------------------------
BR_WAIT_VBLANK_X:
	jsr  BR_VBLANK_START

	jsr  BR_VBLANK_END

	dex
	bne  BR_WAIT_VBLANK_X
	rts


BR_VBLANK_END:
	RESET_SCR_XY
	WAIT_VBLANK_END
	rts


BR_VBLANK_START:
;	jsr   KEY_RTN		;--- キー入力チェック -----
	WAIT_VBLANK

;--- スプライトDMA転送 ----- （※512 clock消費）
;	lda  #2			;ここに必要
;	sta  $4014		;ここに必要
	rts



;===============================
;	起動音「ピ」
;===============================
BR_BEEP_PI:
	lda #0
	sta $4015

	lda #%00000001	; 矩形波チャンネル１を有効にする
	sta $4015
	
	lda #%10_01_1111	; Duty比・長さ無効・減衰無効・減衰率
	sta $4000	; 矩形波チャンネル１制御レジスタ１
	lda #%0_000_0_000	; スイープ有効・変化率・方向・変化量
	sta $4001	; 矩形波チャンネル１制御レジスタ２
	lda #56			; 周波数(下位8ビット)
;	lda #112		; 周波数(下位8ビット)
	sta $4002	; 矩形波チャンネル１周波数レジスタ１
	lda #%00000_000 +(HIGH(0) &7)	; 再生時間・周波数(上位3ビット)
	sta $4003	; 矩形波チャンネル１周波数レジスタ２
	rts


;===============================
;	起動音「ポ」
;===============================
BR_BEEP_PO:
	lda #0
	sta $4015

	lda #%00000001	; 矩形波チャンネル１を有効にする
	sta $4015
	
	lda #%10_01_1111	; Duty比・長さ無効・減衰無効・減衰率
	sta $4000	; 矩形波チャンネル１制御レジスタ１
	lda #%0_000_0_000	; スイープ有効・変化率・方向・変化量
	sta $4001	; 矩形波チャンネル１制御レジスタ２
	lda #112		; 周波数(下位8ビット)
	sta $4002	; 矩形波チャンネル１周波数レジスタ１
	lda #%00000_000 +(HIGH(0) &7)	; 再生時間・周波数(上位3ビット)
	sta $4003	; 矩形波チャンネル１周波数レジスタ２
	rts


