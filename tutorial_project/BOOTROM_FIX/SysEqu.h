;/// @file SysEqu.h
;/// @brief RAM map for the permanent bank.
;/// @ingroup bootrom
;///
;/// Deliberately reduced compared with the erasable bank's version. The original
;/// header notes that everything except the key-input state may be destroyed once
;/// boot is complete, because the permanent bank does not run again afterwards.
;///
;/// @see @ref boot_reflash
;========================================
;
; BOOTROM固定領域用ワーク
;
; ・キー入力関連以外は起動後は破壊してよい
;
; ・BOOTROM固定領域内のキー入力処理を使わずに
;   別途キー入力チェック処理を用意する場合はすべて破壊しても良い
; 
;========================================

W_AR		EQU	$00   ;///< General 16-bit accumulator for arithmetic, 2 bytes. ; 16 bit 計算用  2 bytes
W_BR		EQU	$02   ;///< Second general 16-bit accumulator, 2 bytes. ; 16 bit 計算用  2 bytes

TMP_SVA		EQU	$04   ;///< Scratch save slot for the A register. ; 汎用 A レジスタ保存用アドレス
TMP_SVX		EQU	$05   ;///< Scratch save slot for the X register. ; 汎用 X レジスタ保存用アドレス
TMP_SVY		EQU	$06   ;///< Scratch save slot for the Y register. ; 汎用 Y レジスタ保存用アドレス
TMP_LOOP_CNT	EQU	$07   ;///< General-purpose loop counter. ; 汎用ループカウンタ

SRC_ADR		EQU	$08   ;///< General 16-bit source pointer. ; 汎用ソースアドレス  2 bytes
DST_ADR		EQU	$0A   ;///< General 16-bit destination pointer; also the flash programming cursor. ; 汎用デスティネーションアドレス  2 bytes

TMP_SYS		EQU	$0C   ;///< System scratch byte. ; システムで使うTMP
TMP_SYS2	EQU	$0D   ;///< Second system scratch byte. ; システムで使うTMP


; 起動時RAMチェック
RAM_PAGE	EQU  $25    ;///< Page currently under test by the boot memory check.
MEM_DISP	EQU  $26    ;///< Five-digit decimal counter shown during the memory test.


PICO_MODE	EQU  $51   ;///< Retry counter for the cartridge version handshake. @see CHK_ROMVER ; PICOの動作モード


;----------------
; キー関連
;----------------

KEY_REL		EQU	$82   ;///< Keys released this frame. 
KEY_TRG		EQU	$83   ;///< Keys pressed this frame. 
KEY_OLD		EQU	$84   ;///< Previous frame's held keys. 
KEY_NEW		EQU	$85   ;///< Currently-held keys. @details Checked at boot: holding Start forces a ROM erase. 
KEY_CH1		EQU	TMP_SYS   ;///< Scratch slot for the first controller sample of the majority vote. 
KEY_CH3		EQU	TMP_SYS2   ;///< Scratch slot for the third controller sample of the majority vote. 

REP_KEY		EQU	$8A   ;///< Direction currently being auto-repeated. ; リピート用のキー
REP_NEW		EQU	$8B   ;///< Auto-repeat events generated this frame. ; リピートによる押下状態
REP_CNT		EQU	$8C   ;///< Frames remaining until the next auto-repeat event. ; ウェイト、インターバルのカウンタ




;----------------
; システム関連
;----------------
FLG_2000	EQU	$B0   ;///< Shadow of PPU register `$2000`. 
FLG_2001	EQU	$B1   ;///< Shadow of PPU register `$2001`. 
;BG_SCR_Y	EQU	$B2

NMI_FLG		EQU	$B3   ;///< Re-entrancy guard for the vertical-blank handler. 



PICO_DATA_BUF  EQU	$400   ;///< Staging buffer for data received from the cartridge, 256 bytes. ; PICO のデータ読み込みバッファ
FLASH_DEBUG_BUF  EQU	$400   ;///< Scratch buffer for flash diagnostics. ; FLASHデバッグ用バッファ
FLASH_SAVE_BUF   EQU	$400   ;///< Copy of the page being programmed, used to verify the write. ; FLASHセーブ用バッファ

FLASH_EXEC_BUF  EQU	$500   ;///< RAM the flash routines copy themselves into before running. @warning Mandatory: flash cannot be read while it is busy. @see @ref boot_reflash ; FLASHアクセスコード実行バッファ



;----------------
; PPU
;----------------
; MMC3のIRQを使う場合は BG を$0000番地, Spr を$1000番地に配置しなければならない
FLG_PPU2000	EQU	%100_01_0_00   ;///< Default `$2000`: NMI on, 8x8 sprites, both pattern tables at `$0000`, +1 increment. 
				; NMI gen,master,SP8x8,BG$0000,SP$0000,+1,v0,h0


FLG_PPU2001	EQU	%000_11_11_0   ;///< Default `$2001`: background and sprites enabled, including the leftmost 8 pixels. 


;----------------
; KEY BIT CODE
;----------------
KEY_A		EQU	$80   ;///< A button. 
KEY_B		EQU	$40   ;///< B button. 
KEY_SEL		EQU	$20   ;///< Select button. 
KEY_RUN		EQU	$10   ;///< Start button. @note Held at power-on, forces a ROM erase. @see BR_INIT 
KEY_UP		EQU	$08   ;///< D-pad up. 
KEY_DOWN	EQU	$04   ;///< D-pad down. 
KEY_LEFT	EQU	$02   ;///< D-pad left. 
KEY_RIGHT	EQU	$01   ;///< D-pad right. 

KEY_AB		EQU	$C0   ;///< Mask matching either action button. 
KEY_ABRS	EQU	$F0   ;///< Mask matching A, B, Select or Start. 


;----------------
; キーリピート設定
;----------------
REP_WAIT	EQU	24   ;///< Frames a direction must be held before auto-repeat begins. ; リピート開始までの時間 (フレーム数)
REP_INTERVAL	EQU	 8   ;///< Frames between auto-repeat events. ; リピート間隔 (フレーム数)


