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

W_AR		EQU	$00	; 16 bit 計算用  2 bytes
W_BR		EQU	$02	; 16 bit 計算用  2 bytes

TMP_SVA		EQU	$04	; 汎用 A レジスタ保存用アドレス
TMP_SVX		EQU	$05	; 汎用 X レジスタ保存用アドレス
TMP_SVY		EQU	$06	; 汎用 Y レジスタ保存用アドレス
TMP_LOOP_CNT	EQU	$07	; 汎用ループカウンタ

SRC_ADR		EQU	$08	; 汎用ソースアドレス  2 bytes
DST_ADR		EQU	$0A	; 汎用デスティネーションアドレス  2 bytes

TMP_SYS		EQU	$0C		;システムで使うTMP
TMP_SYS2	EQU	$0D		;システムで使うTMP


; 起動時RAMチェック
RAM_PAGE	EQU  $25
MEM_DISP	EQU  $26


PICO_MODE	EQU  $51		; PICOの動作モード


;----------------
; キー関連
;----------------

KEY_REL		EQU	$82	;
KEY_TRG		EQU	$83	;
KEY_OLD		EQU	$84	;
KEY_NEW		EQU	$85	;
KEY_CH1		EQU	TMP_SYS
KEY_CH3		EQU	TMP_SYS2

REP_KEY		EQU	$8A	; リピート用のキー
REP_NEW		EQU	$8B	; リピートによる押下状態
REP_CNT		EQU	$8C	; ウェイト、インターバルのカウンタ




;----------------
; システム関連
;----------------
FLG_2000	EQU	$B0
FLG_2001	EQU	$B1
;BG_SCR_Y	EQU	$B2

NMI_FLG		EQU	$B3



PICO_DATA_BUF  EQU	$400	; PICO のデータ読み込みバッファ
FLASH_DEBUG_BUF  EQU	$400	; FLASHデバッグ用バッファ
FLASH_SAVE_BUF   EQU	$400	; FLASHセーブ用バッファ

FLASH_EXEC_BUF  EQU	$500	; FLASHアクセスコード実行バッファ



;----------------
; PPU
;----------------
; MMC3のIRQを使う場合は BG を$0000番地, Spr を$1000番地に配置しなければならない
FLG_PPU2000	EQU	%100_01_0_00
				; NMI gen,master,SP8x8,BG$0000,SP$0000,+1,v0,h0


FLG_PPU2001	EQU	%000_11_11_0


;----------------
; KEY BIT CODE
;----------------
KEY_A		EQU	$80
KEY_B		EQU	$40
KEY_SEL		EQU	$20
KEY_RUN		EQU	$10
KEY_UP		EQU	$08
KEY_DOWN	EQU	$04
KEY_LEFT	EQU	$02
KEY_RIGHT	EQU	$01

KEY_AB		EQU	$C0
KEY_ABRS	EQU	$F0


;----------------
; キーリピート設定
;----------------
REP_WAIT	EQU	24	; リピート開始までの時間 (フレーム数)
REP_INTERVAL	EQU	 8	; リピート間隔 (フレーム数)


