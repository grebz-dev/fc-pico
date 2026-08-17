
	.list			; リスティングファイル出力
	.mlist			; リスティングファイル上でマクロを展開

	.INCLUDE	"defDebug.h"

        .inesprg 4	            ; プログラムバンク数
        .ineschr 0	            ; CHR バンク数
        .inesmir 1              ; 垂直ミラーリング
        .inesmap 0				; mapper #0


PG_MAIN EQU 1

ROM_NMI_ENTRY	EQU $ED00
ROM_IRQ_ENTRY	EQU $EE80



;----------------------------------------------
;
; ファミコンからPICO　コマンド
;
;----------------------------------------------
FP_COM_ACK	= $0F		; PICOからのコマンド正常終了応答
FP_COM_NAK	= $1F		; PICOからのコマンド失敗終了応答
FP_COM_VER	= $2F		; BIOS-ROMのバージョン取得：0x0FバイトのROMバージョン文字列
FP_COM_ROM	= $3F		; BIOS-ROMのROMデータ要求コマンド： FP_COM_ROM,アドレスH : 0x100分のROMデータ読み出す

FP_COM_LOG	= $BF		; ログ
FP_COM_DRQ	= $CF		; データリクエスト
FP_COM_DLD	= $DF		; データロード
FP_COM_RST	= $EF		; PICOリスタート
FP_COM_INI	= $FF		; PICO初期化



;----------------------------------------------
;
; その他
;
;----------------------------------------------
SP_CLR_Y	EQU 240		; スプライトクリアーY



	.INCLUDE	"SysEqu.h"
	.INCLUDE	"macro.h"

	.code


	;========================================
	; ゲームバンク $00
	;========================================
	.BANK		0
       ORG     $8000

	.BANK		1
       ORG     $A000


	;========================================
	; ゲームバンク $10
	;========================================
	.BANK		2
		ORG     $C000


	.BANK		3
       ORG     $E000

	ORG     $EF00
MAIN_SETUP:
	jmp  0		; dmy
MAIN_LOOP:
	jmp  0		; dmy

	ORG     $EFF0
DB_ROM_VER:
;	.INCLUDE	"dbdate.h"

	ORG     $EFFF
IS_ROM_ERACE:
	db  0			; ROMが消去されていたら $FFが格納されている
;	db  0xff		; ROMが消去されていたら $FFが格納されている

       ORG     $F000
	;----------------------------------------
	; ジャンプベクター
	;----------------------------------------
INIT:
	jmp  BR_INIT
TRANS_SYS_FONT:
	jmp  BR_TRANS_SYS_FONT
KEY_RTN:
	jmp  BR_KEY_RTN
BEEP_PI:
	jmp  BR_BEEP_PI
BEEP_PO:
	jmp  BR_BEEP_PO


	.INCLUDE	"SysKey.asm"
	.include	"SysBootRom.asm"

	ORG	$FFF9
	DB  $00		; バンク判定用
	DW	ROM_NMI_ENTRY
	DW	INIT
	DW	ROM_IRQ_ENTRY


