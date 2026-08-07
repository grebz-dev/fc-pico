
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
	lda  $2007	;dummy read

	RCV_PICO_BUF PICO_BUF0
	RCV_PICO_BUF PICO_BUF8
	RCV_PICO_BUF PICO_BUF10
	RCV_PICO_BUF PICO_BUF18
	RCV_PICO_BUF PICO_BUF20
	RCV_PICO_BUF PICO_BUF28
	RCV_PICO_BUF PICO_BUF30
	RCV_PICO_BUF PICO_BUF38


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



