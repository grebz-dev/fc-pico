;/// @file AplGame.asm
;/// @brief The tutorial application running on the console.
;/// @ingroup bootrom
;///
;/// Deliberately minimal. It clears the screen, performs the #FP_COM_INI
;/// handshake that tells the cartridge which stage to start on, and then does
;/// nothing but run @ref jobPICO every frame. All graphics come from the
;/// cartridge. @see @ref architecture

;/// @brief The application's 32-byte palette: 16 background entries then 16 sprite entries.
;/// @ingroup bootrom
PAL_GAME_ADR:
	DB	$0F,$1A,$14,$30 ;(緑) 
	DB	$0F,$2A,$2A,$2A ;(赤) 
	DB	$0F,$15,$27,$30 ;(青) 
	DB	$0F,$1A,$1A,$1A ; 
	;	スプライト用パレット
	DB	$0F,$0F,$20,$20 ;スプライト0
	DB	$0F,$1A,$17,$29
	DB	$0F,$15,$19,$20
	DB	$0F,$21,$10,$20



;=====================================
;プレイ画面
;=====================================
;/// @brief Application step handler; runs the scene then clears unused sprites.
;/// @ingroup bootrom
APL_GAME:
	jsr PLY_STG_MAIN
	
;@	jsr updateGameSync
	
;	jsr updateMission
;	jsr moveGameObj

;	BNK_CALL updateGameDisp

	ldy  #0
	
;@	jsr  .disp_debug_obj
	
	
	; 余ったスプライトを非表示にする
	jsr clearObj

;	lda  <STG_COD	;
;	cmp  #ST_MAIN
;	beq  .skip
;	lda  <FLG_2000
;	sta	 $2000				; このタイミングでNMI発生
;.skip
	rts

.disp_debug_obj
	ldx  #0
	
.ddo_loop
	
	lda  #8
	sta  $200 +3,y	; x
	clc
	adc  #8
	sta  $204 +3,y	; x

	txa
	asl  a
	asl  a
	asl  a
	clc
	adc  #16
	sta  $200,y		; y
	sta  $204,y		; y

	lda  PICO_BUF10,x
	lsr  a
	lsr  a
	lsr  a
	lsr  a
	jsr  convHEX2
	sta $200 +1,y	; pat
	lda  PICO_BUF10,x
	jsr  convHEX2
	sta $204 +1,y	; pat

	lda #$03
	sta $200 +2,y	; pal
	sta $204 +2,y	; pal

	tya
	clc
	adc  #8
	tay
	inx
	cpx  #24
	bne  .ddo_loop

	rts



;/// @brief Dispatches on #STG_COD_SUB to the scene's init or main body.
;/// @ingroup bootrom
PLY_STG_MAIN:
	LDA	<STG_COD_SUB
	TBL_JUMP
	JPTBL	PLY_STG_0	; 0
	JPTBL	PLY_STG_1	; 1

;****** INIT ************
;/// @brief Scene init: clears the screen and sends #FP_COM_INI with the starting stage.
;/// @ingroup bootrom
PLY_STG_0:
	DISP_OFF
;	INC	<NMI_FLG


	lda #$80
	JSR CLEAR_BG
	lda #$00
	JSR CLEAR_BG_24

	jsr  SPT_CLR_RTN
	lda  #2
	sta  $4014		; SP DMA

 
	SET_VRAM_ADD2 #$2000+32*30
	lda  #0
	ldy	#64
.loop2
	sta	$2007
	dey
	bne	.loop2


;	SET_DATA_SRC  PAL_GAME_ADR
;	jsr  setPalData

	SET_VRAM_ADD2 #$3F00
	ldy  #32
	lda  #$1F		; 黒
	jsr  SYS_VRAM_WLP

;	CHK_BIT	<KEY_NEW, #KEY_A

	


	SET_VRAM_ADD2	#$0800
	lda  #FP_COM_INI
	sta  $2007
	lda  <PICO_STAGE
	sta  $2007		; 0だったらタイトル画面


	lda  #0
	sta  <BG_SCR_X
	sta  <BG_SCR_Y


	lda #FLG_PPU2000
	sta <FLG_2000

    INC	<STG_COD_SUB

	DISP_ON
	RTS

;	jmp  xPF_COM_DMOD


;****** MAIN ***********
;/// @brief Scene main: runs @ref jobPICO every frame.
;/// @ingroup bootrom
PLY_STG_1:
 .if DEBUG_BUILD
	CHK_BIT	<KEY_TRG, #KEY_A
	beq  .aaa_00
	lda  #PF_COM_FDIN
	sta  <PICO_BUF2
;	lda  #PF_COM_SE
;	sta  <PICO_BUF2
;	lda  #1
;	sta  <PICO_BUF3
	
.aaa_00

	CHK_BIT	<KEY_TRG, #KEY_B
	beq  .aaa_01
	lda  #PF_COM_FDOT
	sta  <PICO_BUF2
.aaa_01

 .endif

	jsr  jobPICO

	lda  <STG_COD
	cmp  #ST_MAIN
	beq  .end
	jsr  SET_STG_COD
.end
	rts


