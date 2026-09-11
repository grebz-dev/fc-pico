;/// @file SysNMI.asm
;/// @brief Vertical-blank handler: the one place the console and the RP2350 exchange data.
;/// @ingroup bootrom
;///
;/// Everything the cartridge link does happens inside this interrupt, in this
;/// order:
;///
;///  1. flush any pending palette change (must be inside vblank);
;///  2. read the 64-byte mailbox out of `$2007` into zero page `$20`-`$5F`;
;///  3. write back one pending command byte plus the controller state;
;///  4. run the optional user NMI hook;
;///  5. restore `$2000`/`$2001` and the scroll registers;
;///  6. sprite DMA;
;///  7. replay the APU register writes the RP2350 queued for this frame.
;///
;/// Step 3 is the frame heartbeat. Writing #KEY_NEW is what tells the RP2350 that
;/// a frame boundary has passed, so it restarts its DMA and releases core 0 to
;/// render the next frame. Skip it and the display freezes.
;///
;/// @note `$0800` here is a *port*, not video memory. `SET_VRAM_ADD2 #$0800`
;///       parks the PPU address in pattern-table space, which is decoded by the
;///       cartridge; the subsequent `$2007` accesses are reads from and writes to
;///       the RP2350, not VRAM.
;/// @see @ref protocol
;/// @see jobPICO

;/// @brief Reads eight consecutive mailbox bytes from `$2007` into a zero-page block.
;/// @details Fully unrolled because the vblank budget is roughly 2200 cycles and
;///          a loop's overhead would not fit alongside sprite DMA.
RCV_PICO_BUF	MACRO
	lda  $2007
	sta  <\1 +0
	lda  $2007
	sta  <\1 +1
	lda  $2007
	sta  <\1 +2
	lda  $2007
	sta  <\1 +3
	lda  $2007
	sta  <\1 +4
	lda  $2007
	sta  <\1 +5
	lda  $2007
	sta  <\1 +6
	lda  $2007
	sta  <\1 +7
	ENDM



;***************************************
; NMI割り込み
;***************************************

;/// @brief Vertical-blank interrupt handler; entered ~60 times per second.
;/// @ingroup bootrom
;///
;/// Reached through the fixed trampoline at `ROM_NMI_ENTRY` (`$ED00`) so that the
;/// permanent fix bank's vector table never has to change when the main bank is
;/// reflashed.
;///
;/// @warning Re-entrant calls are rejected via `NMI_FLG`: if the previous frame's
;///          handler has not finished, this one returns immediately. A handler
;///          that overruns vblank therefore drops frames rather than corrupting
;///          VRAM.
;/// @note Sprite DMA is skipped while a palette fade is in progress, freeing the
;///       ~514 cycles it costs for the fade computation.
;/// @see RCV_PICO_BUF, transPALLET, jobPICO
NMI:
	bit	 $2002
	sta  <NMI_SVA
	incw  <SYS_TIMER
	lda  <NMI_FLG	;NMI処理中か?
	beq  .nmi_ok
	lda  <NMI_SVA
	rti

.nmi_ok
	inc  <NMI_FLG	;NMI処理中フラグオン
	stx  <NMI_SVX
	sty  <NMI_SVY


; --- NMI メイン処理 --------------


;****************************
; ＰＰＵ制御（※ＰＰＵは、垂直帰線期間中に処理を終わらせる）
;****************************
	jsr  transPALLET

	SET_VRAM_ADD2 #$0800
	; PICOからデータ受診
	; The first read after setting the PPU address returns the stale read
	; buffer, never the addressed byte. Discarding it is mandatory, not
	; defensive -- without it every mailbox byte lands one slot early.
	lda  $2007	;dummy read

	RCV_PICO_BUF PICO_BUF0
	RCV_PICO_BUF PICO_BUF8
	RCV_PICO_BUF PICO_BUF10
	RCV_PICO_BUF PICO_BUF18
	RCV_PICO_BUF PICO_BUF20
	RCV_PICO_BUF PICO_BUF28
	RCV_PICO_BUF PICO_BUF30
	RCV_PICO_BUF PICO_BUF38


	; Reply channel. At most one queued command byte goes out per frame,
	; followed unconditionally by the controller state. That second write is
	; the frame heartbeat: on the cartridge it falls through to the default
	; case of rp_system::jobRcvCom(), which latches the keys, re-arms the frame
	; DMA and clears rp_system::frame_draw to release the renderer.
	SET_VRAM_ADD2 #$0800
	lda  <PICO_COM
	beq  .pico00
	sta  $2007
	lda  #0
	sta  <PICO_COM
.pico00
	lda  <KEY_NEW
	sta  $2007



;--- ユーザーVRAM書き換え処理 -----
	lda <NMI_CALL_ADR+1
	beq  .no_usr_nmi
	jsr .usr_nmi_sub
.no_usr_nmi



	lda	<FLG_2000
	sta	 $2000
	lda	<FLG_2001
	sta	 $2001


; ＩＲＱ割り込み値設定=============
;--- スクロールレジスタ設定 -----
	lda  <BG_SCR_X
	sta  $2005
	lda  <BG_SCR_Y
	cmp  #240 -1
	bcc  .y_set
	lda  #239
.y_set
	sta  $2005


	lda  <HIRQ_ENA
	beq  .no_irq
;@	cli			; 割り込み解除

.no_irq

	lda  PALFADE_VAL
	bne  .skip_sp	; フェード中はスプライト更新停止

;--- スプライトDMA転送 ----- （※512 clock消費）
	lda  #2
	sta  $4014		;ここに必要
.skip_sp



	; Reject a torn mailbox. If the frame stream slipped out of phase the 64
	; bytes just read are arbitrary pattern data, so the magic byte is checked
	; before anything acts on them.
	lda  <PICO_BUF1
	cmp  #PF_MAGIC_CODE
	beq  .PF_COM_OK
	lda  #0
	sta  <PICO_BUF2
	beq  .PF_COM_NG
.PF_COM_OK
;	lda  <PICO_BUF0
;	beq  .PF_COM_NG

	; サウンドレジスタ書き込み
 .if 0
	ldx  #0
.sr_loop
	cpx  #$14
	beq  .sr_skip
	lda  PICO_SNDREG,x
	cmp  SV_SNDREG,x
	beq  .sr_skip
	sta  $4000,x
	sta  SV_SNDREG,x
.sr_skip
	inx
	cpx  #$18
	bne  .sr_loop

.sr_end

 .endif

	; APU register replay. The RP2350 runs the music driver on an emulated
	; 6502 and ships the resulting register writes here as (index, value)
	; pairs; the console's own APU is what actually makes the sound. The list
	; is terminated by any value with bit 7 set (rp_system::setPF_APU writes
	; $FF), and is hard-capped at $30 bytes = 24 pairs per frame.
 .if 1
	ldx  #0
.sr_loop
	ldy  PICO_SNDREG,x
	bmi  .sr_end
	inx
	lda  PICO_SNDREG,x
	inx
	sta  $4000,y
	cpx  #$30
	bne  .sr_loop
.sr_end

 .endif

;	jsr  jobPICO


.PF_COM_NG

;****************************
; ＡＰＵ制御（※ＰＰＵ後に処理する）
;****************************
;	jsr  SOUND_SYSTEM


;****************************
; ＮＭＩの最後の最後
;****************************
	ldy  <NMI_SVY
	ldx  <NMI_SVX

	lda  #0		;NMI処理中のフラグオフ
	sta  <NMI_FLG

	lda  <NMI_SVA
	rti

.usr_nmi_sub
	jmp [NMI_CALL_ADR]



