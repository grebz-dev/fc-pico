;/// @file PG_main.asm
;/// @brief Top-level assembly unit for the permanent boot ROM bank.
;/// @ingroup bootrom
;///
;/// Publishes the jump table at `$F000` and the 6502 reset/NMI/IRQ vectors. This
;/// bank is never erased, which is what makes a failed self-reflash recoverable.
;///
;/// @warning The `FP_COM_*` constants are redefined locally here rather than
;///          shared with `BOOTROM/SysPico.asm`. That is a third copy of the
;///          protocol values. @see @ref protocol
;/// @see @ref boot_reflash

	.list			; リスティングファイル出力
	.mlist			; リスティングファイル上でマクロを展開

	.INCLUDE	"defDebug.h"

        .inesprg 4	            ; プログラムバンク数
        .ineschr 0	            ; CHR バンク数
        .inesmir 1              ; 垂直ミラーリング
        .inesmap 0				; mapper #0


PG_MAIN EQU 1   ;///< Set to 1 when assembling the permanent bank, so shared sources can branch. 

ROM_NMI_ENTRY	EQU $ED00   ;///< NMI vector target in the erasable bank, `$ED00`. 
ROM_IRQ_ENTRY	EQU $EE80   ;///< IRQ vector target in the erasable bank, `$EE80`. 



;----------------------------------------------
;
; ファミコンからPICO　コマンド
;
;----------------------------------------------
FP_COM_ACK	= $0F   ;///< Success reply. @note Redefined locally; see the warning in @ref protocol. ; PICOからのコマンド正常終了応答
FP_COM_NAK	= $1F   ;///< Failure reply. @note Redefined locally. ; PICOからのコマンド失敗終了応答
FP_COM_VER	= $2F   ;///< Request the boot-ROM build stamp. @see CHK_ROMVER ; BIOS-ROMのバージョン取得：0x0FバイトのROMバージョン文字列
FP_COM_ROM	= $3F   ;///< Request a 256-byte page of the ROM image. @see ROM_UPDATE ; BIOS-ROMのROMデータ要求コマンド： FP_COM_ROM,アドレスH : 0x100分のROMデータ読み出す

FP_COM_LOG	= $BF   ;///< Debug log to the cartridge's serial console. ; ログ
FP_COM_DRQ	= $CF   ;///< Data request while in bulk data mode. ; データリクエスト
FP_COM_DLD	= $DF   ;///< Data load; next byte selects the page. ; データロード
FP_COM_RST	= $EF   ;///< Restart the cartridge firmware. ; PICOリスタート
FP_COM_INI	= $FF   ;///< Initialise the cartridge. ; PICO初期化



;----------------------------------------------
;
; その他
;
;----------------------------------------------
SP_CLR_Y	EQU 240   ;///< Y coordinate that parks a sprite off screen. ; スプライトクリアーY



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
;/// @brief Placeholder at `$EF00`; the real routine comes from the erasable bank.
;/// @ingroup bootrom
MAIN_SETUP:
	jmp  0		; dmy
;/// @brief Placeholder at `$EF03`; the real routine comes from the erasable bank.
;/// @ingroup bootrom
MAIN_LOOP:
	jmp  0		; dmy

	ORG     $EFF0
;/// @brief Build stamp placeholder; the erasable bank supplies the real one.
;/// @ingroup bootrom
DB_ROM_VER:
;	.INCLUDE	"dbdate.h"

	ORG     $EFFF
;/// @brief Erase marker placeholder; the erasable bank supplies the real one.
;/// @ingroup bootrom
IS_ROM_ERACE:
	db  0			; ROMが消去されていたら $FFが格納されている
;	db  0xff		; ROMが消去されていたら $FFが格納されている

       ORG     $F000
	;----------------------------------------
	; ジャンプベクター
	;----------------------------------------
;/// @brief Jump-table entry `$F000`: cold boot. @see BR_INIT
;/// @ingroup bootrom
INIT:
	jmp  BR_INIT
;/// @brief Jump-table entry `$F003`: expand the system font into both pattern tables.
;/// @ingroup bootrom
TRANS_SYS_FONT:
	jmp  BR_TRANS_SYS_FONT
;/// @brief Jump-table entry `$F006`: read the controller. @see BR_KEY_RTN
;/// @ingroup bootrom
KEY_RTN:
	jmp  BR_KEY_RTN
;/// @brief Jump-table entry `$F009`: high-pitched beep.
;/// @ingroup bootrom
BEEP_PI:
	jmp  BR_BEEP_PI
;/// @brief Jump-table entry `$F00C`: low-pitched beep.
;/// @ingroup bootrom
BEEP_PO:
	jmp  BR_BEEP_PO


	.INCLUDE	"SysKey.asm"
	.include	"SysBootRom.asm"

	ORG	$FFF9
	DB  $00		; バンク判定用
	DW	ROM_NMI_ENTRY
	DW	INIT
	DW	ROM_IRQ_ENTRY


