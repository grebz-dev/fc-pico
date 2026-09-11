
;/// @file SysPico.asm
;/// @brief FC-PICO wire protocol: opcode definitions, command dispatch and bulk data mode.
;/// @ingroup bootrom
;///
;/// Two channels share one wire, the PPU data port at `$2007`:
;///
;///  - **PICO -> Famicom** arrives passively, as the 64-byte mailbox appended to
;///    the tail of every frame's pattern stream and read out by @ref NMI. The
;///    opcodes in it are executed here by @ref jobPICO.
;///  - **Famicom -> PICO** is a byte written to `$2007`, captured by the
;///    cartridge's receive state machine.
;///
;/// There is no ready line and no acknowledgement in either direction. After
;/// sending a request the 6502 simply spins in @ref PICO_COM_WAIT for roughly
;/// 1.3 ms and then reads the answer.
;///
;/// @warning These opcode values are duplicated in `sys/rp_system.h` (as
;///          `PF_COM_*` / `FP_COM_*`) and again in `BOOTROM_FIX/PG_main.asm`.
;///          Three copies, no version check -- change them together.
;/// @see @ref protocol
;***********************************************************************
;	FC-PICO関連システム
;***********************************************************************

;/// @brief Sentinel the RP2350 stamps into mailbox byte 1 so a torn frame can be detected.
;/// @details Known as #PF_MAGIC_NO on the C++ side.
PF_MAGIC_CODE = $FC		; 受け取ったコマンドの可否チェックコード
;----------------------------------------------
;
; PICOからファミコン　コマンド
;
;
;----------------------------------------------
PF_COM_NONE = 0			;///< End of the command list; stops @ref jobPICO. // コマンドなし
PF_COM_DMOD = 1			;///< Blank the display and enter bulk data mode. @see xPF_COM_DMOD
PF_COM_FDIN = 2			;///< Begin a palette fade-in. @see SET_FADE_IN_B
PF_COM_FDOT = 3			;///< Begin a palette fade-out. @see SET_FADE_OUT_B

PF_COM_SE   = $80		;///< Play sound effect `$80 | SE_NO`. @note Body is disabled in this build.
PF_COM_BGM  = $A0		;///< Play music `$A0 | BGM_NO`. @note Body is disabled in this build.
PF_COM_VRAM = $C0		;///< Poke one VRAM byte: `$C0|adrH`, `adrL`, `data`.



;----------------------------------------------
;
; PICOからファミコンへデータセット
;
; 画面OFF限定で大量のデータをPICOからファミコンへデータを送るコマンド
;
;----------------------------------------------
PF_DAT_VRAM = $80		;///< Bulk VRAM write: `adrH`, `adrL`, `size`, `data...`; size 0 means 256.
; --> size = 0 は256バイト 256バイト以上送りたい場合は分割して送る
PF_DAT_RAM  = $81		;///< Bulk CPU-RAM write; same payload layout as #PF_DAT_VRAM.

PF_DAT_STEP = $82		;///< Leave data mode and jump to the step code in the payload.

;----------------------------------------------
;
; ファミコンからPICO　コマンド
;
;----------------------------------------------
FP_COM_ACK	= $0F		;///< Success reply. @note Unused; the RP2350 side has it commented out.
FP_COM_NAK	= $1F		;///< Failure reply. @note Unused; the RP2350 side has it commented out.
FP_COM_VER	= $2F		;///< Request the 14-byte boot-ROM build stamp. @see CHK_ROMVER
FP_COM_ROM	= $3F		;///< Request a ROM page: `FP_COM_ROM`, `adrH`; yields `$100` bytes. @see ROM_UPDATE
FP_COM_LOG	= $BF		;///< Debug log; following bytes are printed on the cartridge's serial console.
FP_COM_DRQ	= $CF		;///< Data request, issued repeatedly while in bulk data mode.
FP_COM_DLD	= $DF		;///< Data load; next byte selects the 256-byte page to receive.
FP_COM_RST	= $EF		;///< Restart the cartridge firmware. @see BR_INIT
FP_COM_INI	= $FF		;///< Initialise the cartridge; next byte is the starting stage.


;------------------------------------------
;
;		PICO コマンド処理
;
;------------------------------------------
;/// @brief Executes the command list the RP2350 placed in this frame's mailbox.
;/// @ingroup bootrom
;///
;/// Walks mailbox bytes 2..15. Byte 0 is unused and byte 1 holds
;/// #PF_MAGIC_CODE, already validated by @ref NMI. Opcodes with bit 7 clear are
;/// dispatched through a jump table; the rest are split by range into the
;/// sound and VRAM handlers. A #PF_COM_NONE byte ends the list.
;///
;/// @note Called from the main loop, not from the interrupt: the original
;///       `jsr jobPICO` inside @ref NMI is commented out, because commands such
;///       as #PF_COM_DMOD block for far longer than vblank allows.
;/// @see xPF_COM_DMOD
jobPICO:
	ldx  #2
;/// @brief Loop head of @ref jobPICO; X indexes the current mailbox byte.
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

;/// @brief Terminates command processing and clears the first command slot.
;/// @ingroup bootrom
;/// @details Zeroing `PICO_BUF2` prevents the same list from being re-executed
;///          if the next frame's mailbox fails its magic-byte check.
jobPICI_end:
	lda  #0
	sta  <PICO_BUF2
	rts

;-------------------
; サウンド関連
;-------------------
;/// @brief Handles #PF_COM_BGM: starts or stops music.
;/// @ingroup bootrom
;/// @details The low 5 bits select the song; 0 means stop.
;/// @note The body is disabled in this build. Music is driven from the
;///       cartridge instead, via the APU replay path in @ref NMI.
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
	
;/// @brief Handles #PF_COM_SE: starts a sound effect.
;/// @ingroup bootrom
;/// @details The low 5 bits select the effect.
;/// @note The body is disabled in this build. @see xPF_COM_BGM
xPF_COM_SE:
	and  #$1F
;@	jsr  PLAY_SE
	inx
	jmp  jobPICO_NEXT


;-------------------
; VRAM書き換え
;-------------------
;/// @brief Handles #PF_COM_VRAM: writes a single byte to VRAM.
;/// @ingroup bootrom
;/// @details Payload is `adrH` (in the opcode's low bits), `adrL`, `data`.
;/// @note The body is disabled (`.if 0`) in this build; the tutorial pushes
;///       palette and attribute data through bulk data mode instead.
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
;/// @brief Handles #PF_COM_FDIN: runs a fade-in to black and blocks until it finishes.
;/// @ingroup bootrom
;/// @see SET_FADE_IN_B, WAIT_FADE_END
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
;/// @brief Handles #PF_COM_FDOT: runs a fade-out to black and blocks until it finishes.
;/// @ingroup bootrom
;/// @see SET_FADE_OUT_B, WAIT_FADE_END
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
;/// @brief Bulk data mode: pulls whole VRAM/RAM blocks from the cartridge with rendering off.
;/// @ingroup bootrom
;///
;/// Rendering must be disabled because the transfer needs far more `$2007`
;/// traffic than a vertical blank can hold, and because with the PPU idle the
;/// cartridge no longer has a fetch stream to piggyback on.
;///
;/// Each iteration sends #FP_COM_DRQ, waits, and reads a six-byte header
;/// (command, magic, `adrL`, `adrH`, `sizeL`, `sizeH`). The loop ends when the
;/// cartridge answers #PF_COM_NONE or #PF_DAT_STEP.
;///
;/// @warning Entering this mode blanks the screen and holds the CPU in a spin
;///          loop; it is for scene transitions, not per-frame updates.
;/// @see rp_system::startDataMode, PICO_COM_WAIT
xPF_COM_DMOD:
	DISP_OFF
	lda  #0
	sta	 $2000

;/// @brief Body of the bulk-transfer loop; one request/response per iteration.
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

;/// @brief Common exit from bulk data mode: restores `$2000`, re-enables the display, fades in.
;/// @ingroup bootrom
xPF_COM_DMOD_end:
;	jsr PICO_COM_WAIT
;	jsr setDebugLog
;	jsr setFP_COM_LOG
	lda #FLG_PPU2000
	sta	 $2000
	DISP_ON
	jsr SET_FADE_IN_B

	jmp  jobPICI_end


;/// @brief Handles #PF_DAT_STEP: leaves data mode and switches the application step.
;/// @ingroup bootrom
;/// @details Bumps `NMI_FLG` first so the in-flight vertical blank cannot run a
;///          handler belonging to the scene being torn down.
xPF_DAT_STEP:
	inc	<NMI_FLG	;ハング防止
	lda  <PICO_BUF2
	sta  <STG_COD

	jmp  xPF_COM_DMOD_end


;/// @brief Error exit from bulk data mode when the header fails its magic-byte check.
;/// @ingroup bootrom
;/// @details Logs error code 1 and falls through to the normal exit, so a
;///          desynchronised transfer restores the display rather than hanging.
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
	; Known quirk, preserved verbatim from the original: transfers targeting
	; the attribute table at $23C0 arrive one byte out of phase, so an extra
	; dummy read is required. The original comment states the cause was never
	; identified. Do not remove without re-testing an attribute upload.
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

;/// @brief Sends four bytes of #PICO_DATA_BUF to the cartridge as a #FP_COM_LOG record.
;/// @ingroup bootrom
;/// @details The cartridge prints them on its USB serial console, giving the
;///          6502 a debug channel it otherwise has no hardware for.
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

;/// @brief Reports a protocol error to the cartridge's serial console.
;/// @ingroup bootrom
;/// @details Emits #FP_COM_LOG, the marker byte `$EA`, then the error code in A.
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

;/// @brief The entire handshake: spin ~256 iterations to let the cartridge answer.
;/// @ingroup bootrom
;///
;/// Roughly 1.3 ms at 1.79 MHz. There is no ready flag, no acknowledgement and no
;/// timeout -- the protocol simply assumes the RP2350 has replied by the time the
;/// loop finishes.
;///
;/// @warning This is the tightest coupling in the whole system. Anything that
;///          lengthens the cartridge's response path (serial logging, a watchdog
;///          stall, a long interrupt) makes the 6502 read stale bytes with no
;///          indication that anything went wrong.
PICO_COM_WAIT:
	ldx  #0
.wait
	dex
	bne  .wait
	rts

;/// @brief Dumps the six-byte data-mode header to the cartridge's serial console.
;/// @ingroup bootrom
;/// @details Diagnostic aid for bulk-transfer desynchronisation; the call sites
;///          are left in place but commented out.
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

