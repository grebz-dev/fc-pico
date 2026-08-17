;/// @file SysEqu.h
;/// @brief RAM map for the erasable bank: zero page, work RAM and cartridge RAM.
;/// @ingroup bootrom
;///
;/// The layout worth knowing is the mailbox at `$20`-`$5F`, which is where the
;/// cartridge's per-frame command block lands. Everything from #PICO_BUF0 to
;/// #PICO_BUF38 is one contiguous 64-byte structure, not eight separate
;/// variables. @see @ref protocol
;///
;/// @warning Zero page is fully allocated. Adding a variable means finding a
;///          genuinely unused slot, not appending to the end.
;========================================

;========================================

W_AR			EQU	$00   ;///< General 16-bit accumulator for arithmetic, 2 bytes. ; 16 bit 計算用  2 bytes
W_BR			EQU	$02   ;///< Second general 16-bit accumulator, 2 bytes. ; 16 bit 計算用  2 bytes

TMP_SVA			EQU	$04   ;///< Scratch save slot for the A register. ; 汎用 A レジスタ保存用アドレス
TMP_SVX			EQU	$05   ;///< Scratch save slot for the X register. ; 汎用 X レジスタ保存用アドレス
TMP_SVY			EQU	$06   ;///< Scratch save slot for the Y register. ; 汎用 Y レジスタ保存用アドレス
TMP_LOOP_CNT	EQU	$07   ;///< General-purpose loop counter. ; 汎用ループカウンタ

SRC_ADR			EQU	$08   ;///< General 16-bit source pointer used by the copy routines. ; 汎用ソースアドレス  2 bytes
DST_ADR			EQU	$0A   ;///< General 16-bit destination pointer used by the copy routines. ; 汎用デスティネーションアドレス  2 bytes

TMP_SYS			EQU	$0C   ;///< System scratch byte. ; システムで使うTMP
TMP_SYS2		EQU	$0D   ;///< Second system scratch byte. ; システムで使うTMP
TMP_COUNT		EQU $0E    ;///< System scratch counter.

TMP_SV0		EQU	$10   ;///< General register save slot 0. ; 汎用レジスタ保存用アドレス
TMP_SV1		EQU	$11   ;///< General register save slot 1. ; 汎用レジスタ保存用アドレス
TMP_SV2		EQU	$12   ;///< General register save slot 2. ; 汎用レジスタ保存用アドレス
TMP_SV3		EQU	$13   ;///< General register save slot 3. ; 汎用レジスタ保存用アドレス
TMP_SV4		EQU	$14   ;///< General register save slot 4. ; 汎用レジスタ保存用アドレス
TMP_SV5		EQU	$15   ;///< General register save slot 5. ; 汎用レジスタ保存用アドレス
TMP_SV6		EQU	$16   ;///< General register save slot 6. ; 汎用レジスタ保存用アドレス
TMP_SV7		EQU	$17   ;///< General register save slot 7. ; 汎用レジスタ保存用アドレス

GM_TMP0	 	EQU	$18    ;///< Application scratch byte 0.
GM_TMP1	 	EQU	$19    ;///< Application scratch byte 1.
GM_TMP2	 	EQU	$1A    ;///< Application scratch byte 2.
GM_TMP3	 	EQU	$1B    ;///< Application scratch byte 3.

NMI_SVA		EQU	$1C   ;///< A register saved on entry to @ref NMI. ; NMI割り込み A レジスタ保存用アドレス
NMI_SVX		EQU	$1D   ;///< X register saved on entry to @ref NMI. ; NMI割り込み X レジスタ保存用アドレス
NMI_SVY		EQU	$1E   ;///< Y register saved on entry to @ref NMI. ; NMI割り込み Y レジスタ保存用アドレス



; PICO通信用 64byte
PICO_BUF0	EQU  $20    ;///< Mailbox byte 0; unused. The mailbox spans `$20`-`$5F`. @see @ref protocol
PICO_BUF1	EQU  $21    ;///< Mailbox byte 1; holds #PF_MAGIC_CODE and is checked before any command runs.
PICO_BUF2	EQU  $22    ;///< Mailbox byte 2; first command slot, where @ref jobPICO starts.
PICO_BUF3	EQU  $23    ;///< Mailbox byte 3.
PICO_BUF4	EQU  $24    ;///< Mailbox byte 4.
PICO_BUF5	EQU  $25    ;///< Mailbox byte 5.
PICO_BUF6	EQU  $26    ;///< Mailbox byte 6.
PICO_BUF7	EQU  $27    ;///< Mailbox byte 7.
PICO_BUF8	EQU  $28    ;///< Mailbox byte 8; start of the second unrolled read block.

PICO_BUF10	EQU  $30    ;///< Mailbox offset `$10`; start of the APU register area.
PICO_BUF18	EQU  $38    ;///< Mailbox offset `$18`.
PICO_BUF20	EQU  $40    ;///< Mailbox offset `$20`.
PICO_BUF28	EQU  $48    ;///< Mailbox offset `$28`.
PICO_BUF30	EQU  $50    ;///< Mailbox offset `$30`.
PICO_BUF38	EQU  $58    ;///< Mailbox offset `$38`; last block of the 64-byte mailbox.

PICO_SNDREG	EQU  PICO_BUF10    ;///< APU (register, value) pairs, terminated by `$FF`. Mirrors #PICO_SNDREG on the C++ side.


PICO_COM	EQU  $60   ;///< One command byte queued for the cartridge; sent and cleared by @ref NMI. ; PICOへコマンド送信用
PICO_MODE	EQU  $61   ;///< Retry counter used while waiting for the cartridge during boot. ; PICOの動作モード

PICO_STAGE	EQU  $62   ;///< Stage number sent with #FP_COM_INI; 0 selects the title screen. ; PICOへ渡すステージ番号

; 空き


GM_WAIT		EQU	$72   ;///< Frame countdown used by the application wait helpers. ; 2 bytes  ゲーム待ち

;----------------
; 処理落ち対策
;----------------

;----------------
; 拡張アダプター モード
;----------------
EXA_MODE	EQU	$76   ;///< 0 when an FC-EXA adapter was detected, 1 for the normal FC PICO path. ; =0 拡張モード =1 スタンドアロンモード

;----------------
; ゲーム関連
;----------------
DEMO_TIMER	EQU	$78   ;///< Countdown for the attract-mode timeout. @see SET_TIMEOUT ; デモタイマー



;----------------
; キー関連
;----------------


KEY_REL		EQU	$82    ;///< Keys released this frame.
KEY_TRG		EQU	$83    ;///< Keys pressed this frame (rising edge).
KEY_OLD		EQU	$84    ;///< Previous frame's held keys, used to derive edges.
KEY_NEW		EQU	$85    ;///< Currently-held keys; also the byte sent to the cartridge each frame as the frame heartbeat.
KEY_CH1		EQU	TMP_SYS    ;///< Scratch slot for the first controller sample of the majority vote.
KEY_CH3		EQU	TMP_SYS2    ;///< Scratch slot for the third controller sample of the majority vote.

REP_KEY		EQU	$8A   ;///< Direction currently being auto-repeated. ; リピート用のキー
REP_NEW		EQU	$8B   ;///< Auto-repeat events generated this frame. ; リピートによる押下状態
REP_CNT		EQU	$8C   ;///< Frames remaining until the next auto-repeat event. ; ウェイト、インターバルのカウンタ


;----------------
; IRQ処理関連
;----------------
HIRQ_ENA	EQU		$8F   ;///< Scanline IRQ enable. @note Vestigial; the hardware is not present. ; IRQ フラグ制御 (未使用=0)
BG_SCR_X	EQU		$94   ;///< Horizontal scroll, written to `$2005` during vertical blank. ; size 4 bytes


;----------------
; システム関連
;----------------
FLG_2000	EQU	$B0    ;///< Shadow of PPU register `$2000`, restored every vertical blank.
FLG_2001	EQU	$B1    ;///< Shadow of PPU register `$2001`, restored every vertical blank.
BG_SCR_Y	EQU	$B2    ;///< Vertical scroll, clamped to 239 before being written to `$2005`.

NMI_FLG		EQU	$B3    ;///< Re-entrancy guard for @ref NMI; non-zero means a handler is already running.
PAL_CHG_FG	EQU	$B4   ;///< Set when #PAL_WRK differs from the hardware palette, so @ref transPALLET uploads it. ; パレット変更フラグ

SYS_TIMER	EQU	$B5   ;///< Free-running 16-bit frame counter, incremented by @ref NMI. @see WAIT_VSYNC ; 2 bytes
FLM_TIMER	EQU	$B7   ;///< Per-scene frame counter, reset on scene change. ; フレームタイマー

STG_COD		EQU	$B8    ;///< Current application step. @see SET_STG_COD
STG_COD_SUB	EQU	$B9    ;///< Sub-step within the current step; 0 means the step's init has not run.



;----------------
; NMIからコールするプログラムのアドレス
;----------------
NMI_CALL_BNK	EQU $BD   ;///< Bank for the optional user vertical-blank hook. ; 1byte ０ならコールしない
NMI_CALL_ADR	EQU $BE   ;///< Address of the optional user vertical-blank hook; a zero high byte disables it. ; 2byte コールするプログラムアドレス


TMP_WRK0	EQU	$C0    ;///< Application work byte 0.
TMP_WRK1	EQU	$C1    ;///< Application work byte 1.
TMP_WRK2	EQU	$C2    ;///< Application work byte 2.
TMP_WRK3	EQU	$C3    ;///< Application work byte 3.

TMP_DISP2	EQU	TMP_WRK3   ;///< Alias of #TMP_WRK3 used by the display helpers. ; 1 byte  表示汎用


SPRITE1		EQU	$200   ;///< OAM shadow, 256 bytes; transferred by writing 2 to `$4014`. ; 256 bytes


;----------------
; パレット関連
;----------------
PALFADE_TIME	EQU	$306   ;///< Frames between fade steps. ; パレットフェード速度
PALFADE_CNT		EQU	$307   ;///< Frames remaining until the next fade step. ; パレットフェードカウンタ
PALFADE_VAL		EQU	$308   ;///< Current fade level; non-zero suppresses sprite DMA in @ref NMI. ; 加算値、減算値
PALFADE_ADD		EQU	$309   ;///< Amount added to #PALFADE_VAL each step; sign selects in or out. ; 変化の加算値
PALFADE_MASK	EQU	$30A   ;///< Per-entry mask selecting which palette groups the fade affects. ; 変化させないパレットビット指定


MP3_VOL		EQU	$329    ;///< MP3 volume, mirrored from the cartridge's save data.
MP3_BANK	EQU	$32A    ;///< Selected MP3 bank.

HISCORES	EQU	$32C   ;///< High score table. @note Deliberately preserved across a reset. ; 4 bytes ハイスコア

PAL_WRK		EQU	$330   ;///< 32-byte palette shadow; the only source @ref transPALLET uploads from. ; size $20	転送用




PICO_DATA_BUF  EQU	$400   ;///< Staging buffer for bulk transfers, 256 bytes. @see xPF_COM_DMOD ; PICO のデータ読み込みバッファ






;----------------
; 拡張アダプター関連
;----------------
EXS_HIRQ_REG = $4800   ;///< Scanline IRQ latch on the expansion adapter, `$4800`. @note Vestigial; not present on FC PICO. 



;----------------
; WRAM
;----------------
W_TEST		EQU	$6000   ;///< Cartridge RAM probe address used by the boot self-test. ; 1 byte  bit7: WRAM 存在フラグ
				;(1 bytes)
W_HISCORES	EQU	$6002   ;///< High scores in cartridge RAM. ; 8 bytes LV1 のハイスコア,キャラ
				;         LV2 のハイスコア,キャラ
W_MAGIC		EQU	$600a   ;///< Magic value marking cartridge RAM as initialised. ; 6 bytes WRAM 初期化済み判別用マジックナンバー


WIFI_BUF EQU $7000    ;///< Wi-Fi receive buffer. @note Vestigial FC-EXA feature.

BURST_READ_BUF EQU $7100    ;///< Burst-read buffer. @note Vestigial FC-EXA feature.

HTTP_BUF	EQU	$7400   ;///< HTTP buffer. @note Vestigial FC-EXA feature. ; HTTPデータ読み込み


;----------------
; PPU
;----------------
; MMC3のIRQを使う場合は BG を$0000番地, Spr を$1000番地に配置しなければならない
FLG_PPU2000	EQU	%100_01_0_00    ;///< Default `$2000`: NMI on, 8x8 sprites, both pattern tables at `$0000`, +1 address increment.
				; NMI gen,master,SP8x8,BG$0000,SP$0000,+1,v0,h0


FLG_PPU2001	EQU	%000_11_11_0    ;///< Default `$2001`: background and sprites enabled, including in the leftmost 8 pixels.


;----------------
; KEY BIT CODE
;----------------
KEY_A		EQU	$80    ;///< A button.
KEY_B		EQU	$40    ;///< B button.
KEY_SEL		EQU	$20    ;///< Select button.
KEY_RUN		EQU	$10    ;///< Start button.
KEY_UP		EQU	$08    ;///< D-pad up.
KEY_DOWN	EQU	$04    ;///< D-pad down.
KEY_LEFT	EQU	$02    ;///< D-pad left.
KEY_RIGHT	EQU	$01    ;///< D-pad right.

KEY_AB		EQU	$C0    ;///< Mask matching either action button.
KEY_ABRS	EQU	$F0    ;///< Mask matching A, B, Select or Start.


;----------------
; キーリピート設定
;----------------
REP_WAIT	EQU	24   ;///< Frames a direction must be held before auto-repeat begins. ; リピート開始までの時間 (フレーム数)
REP_INTERVAL	EQU	 8   ;///< Frames between auto-repeat events. ; リピート間隔 (フレーム数)


