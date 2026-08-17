;========================================

;========================================

W_AR			EQU	$00	; 16 bit 計算用  2 bytes
W_BR			EQU	$02	; 16 bit 計算用  2 bytes

TMP_SVA			EQU	$04	; 汎用 A レジスタ保存用アドレス
TMP_SVX			EQU	$05	; 汎用 X レジスタ保存用アドレス
TMP_SVY			EQU	$06	; 汎用 Y レジスタ保存用アドレス
TMP_LOOP_CNT	EQU	$07	; 汎用ループカウンタ

SRC_ADR			EQU	$08	; 汎用ソースアドレス  2 bytes
DST_ADR			EQU	$0A	; 汎用デスティネーションアドレス  2 bytes

TMP_SYS			EQU	$0C		;システムで使うTMP
TMP_SYS2		EQU	$0D		;システムで使うTMP
TMP_COUNT		EQU $0E

TMP_SV0		EQU	$10	; 汎用レジスタ保存用アドレス
TMP_SV1		EQU	$11	; 汎用レジスタ保存用アドレス
TMP_SV2		EQU	$12	; 汎用レジスタ保存用アドレス
TMP_SV3		EQU	$13	; 汎用レジスタ保存用アドレス
TMP_SV4		EQU	$14	; 汎用レジスタ保存用アドレス
TMP_SV5		EQU	$15	; 汎用レジスタ保存用アドレス
TMP_SV6		EQU	$16	; 汎用レジスタ保存用アドレス
TMP_SV7		EQU	$17	; 汎用レジスタ保存用アドレス

GM_TMP0	 	EQU	$18
GM_TMP1	 	EQU	$19
GM_TMP2	 	EQU	$1A
GM_TMP3	 	EQU	$1B

NMI_SVA		EQU	$1C	; NMI割り込み A レジスタ保存用アドレス
NMI_SVX		EQU	$1D	; NMI割り込み X レジスタ保存用アドレス
NMI_SVY		EQU	$1E	; NMI割り込み Y レジスタ保存用アドレス



; PICO通信用 64byte
PICO_BUF0	EQU  $20
PICO_BUF1	EQU  $21
PICO_BUF2	EQU  $22
PICO_BUF3	EQU  $23
PICO_BUF4	EQU  $24
PICO_BUF5	EQU  $25
PICO_BUF6	EQU  $26
PICO_BUF7	EQU  $27
PICO_BUF8	EQU  $28

PICO_BUF10	EQU  $30
PICO_BUF18	EQU  $38
PICO_BUF20	EQU  $40
PICO_BUF28	EQU  $48
PICO_BUF30	EQU  $50
PICO_BUF38	EQU  $58

PICO_SNDREG	EQU  PICO_BUF10


PICO_COM	EQU  $60		; PICOへコマンド送信用
PICO_MODE	EQU  $61		; PICOの動作モード

PICO_STAGE	EQU  $62		; PICOへ渡すステージ番号

; 空き


GM_WAIT		EQU	$72	; 2 bytes  ゲーム待ち

;----------------
; 処理落ち対策
;----------------

;----------------
; 拡張アダプター モード
;----------------
EXA_MODE	EQU	$76	; =0 拡張モード =1 スタンドアロンモード

;----------------
; ゲーム関連
;----------------
DEMO_TIMER	EQU	$78	; デモタイマー



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
; IRQ処理関連
;----------------
HIRQ_ENA	EQU		$8F ; IRQ フラグ制御 (未使用=0)
BG_SCR_X	EQU		$94 ; size 4 bytes


;----------------
; システム関連
;----------------
FLG_2000	EQU	$B0
FLG_2001	EQU	$B1
BG_SCR_Y	EQU	$B2

NMI_FLG		EQU	$B3
PAL_CHG_FG	EQU	$B4		; パレット変更フラグ

SYS_TIMER	EQU	$B5		; 2 bytes
FLM_TIMER	EQU	$B7		; フレームタイマー

STG_COD		EQU	$B8
STG_COD_SUB	EQU	$B9



;----------------
; NMIからコールするプログラムのアドレス
;----------------
NMI_CALL_BNK	EQU $BD ; 1byte ０ならコールしない
NMI_CALL_ADR	EQU $BE ; 2byte コールするプログラムアドレス


TMP_WRK0	EQU	$C0
TMP_WRK1	EQU	$C1
TMP_WRK2	EQU	$C2
TMP_WRK3	EQU	$C3

TMP_DISP2	EQU	TMP_WRK3	; 1 byte  表示汎用


SPRITE1		EQU	$200	; 256 bytes


;----------------
; パレット関連
;----------------
PALFADE_TIME	EQU	$306	; パレットフェード速度
PALFADE_CNT		EQU	$307	; パレットフェードカウンタ
PALFADE_VAL		EQU	$308	; 加算値、減算値
PALFADE_ADD		EQU	$309	; 変化の加算値
PALFADE_MASK	EQU	$30A	; 変化させないパレットビット指定


MP3_VOL		EQU	$329
MP3_BANK	EQU	$32A

HISCORES	EQU	$32C	; 4 bytes ハイスコア

PAL_WRK		EQU	$330	 ;size $20	転送用




PICO_DATA_BUF  EQU	$400	; PICO のデータ読み込みバッファ






;----------------
; 拡張アダプター関連
;----------------
EXS_HIRQ_REG = $4800



;----------------
; WRAM
;----------------
W_TEST		EQU	$6000	; 1 byte  bit7: WRAM 存在フラグ
				;(1 bytes)
W_HISCORES	EQU	$6002	; 8 bytes LV1 のハイスコア,キャラ
				;         LV2 のハイスコア,キャラ
W_MAGIC		EQU	$600a	; 6 bytes WRAM 初期化済み判別用マジックナンバー


WIFI_BUF EQU $7000

BURST_READ_BUF EQU $7100

HTTP_BUF	EQU	$7400		; HTTPデータ読み込み


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


