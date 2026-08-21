;/// @file defDebug.h
;/// @brief Build-time switches for the erasable bank.
;/// @ingroup bootrom

;==========================================================
; デバッグコントロール定義
;==========================================================
; デバッグビルドコントロール
DEBUG_BUILD EQU  0   ;///< Non-zero enables the in-ROM debug controls in `AplGame.asm`. 

MAPPER_NO	EQU 0   ;///< iNES mapper number; 0 for the NROM cartridge this project targets. 


; NMI とゲーム処理だけですが処理の重さを表示する場合は 1 にします
PROCESSMETER = 0   ;///< Show a CPU-load meter for the vertical blank and application. 

; ゲーム処理の重さを表示する場合は 1 にします
GAME_PROCESSMETER = 	0   ;///< Show a CPU-load meter for the application only. 


; エンディングテストモード (即クリア確定)
ENDINGTEST = 0   ;///< Ending test mode: clear the game immediately. 

; デバッグ機能ON
DEBUG_MODE = 0   ;///< Master switch for the in-ROM debug features. 

; デバッグモードでテンポの速いステージ曲を再生可能にする場合は 1 にします。
; (「SOUND TST」で B を押しながら A を押す)
DEBUG_TEMPO_UP = 1   ;///< Allow the fast stage tune to be selected in the sound test. 

DEBUG_NO_GAME_OVER = 0   ;///< Disable game over. 

DEBUG_DISP_BOSS_HP = 0   ;///< Display boss hit points. 

DEBUG_DISP_PLY_DEBUG = 0   ;///< Display player debug state. 


AUTO_SHOT_OFF = 0   ;///< Allow autofire to be suppressed by holding A. ; =1 の時、通常弾のオートショットをＡボタン押しっぱなしで停止出来る


SINGLE_SHOT_TEST = 0   ;///< Restrict the player to a single shot, for testing. ; =1 自機の通常弾1発のみテスト
STAGE_TEST		 = 0   ;///< Non-zero starts the game on that stage. ; !=0  指定ステージからスタート

MMC_TYPE = 0   ;///< Mapper variant: 0 MMC3, 1 AX-A1, 2 INL-SWAP. @note Stale; this cartridge is NROM. ; =0 MMC3  =1 AX-A1 =2 INL-SWAP
NO_COPY_PROTECT =  0   ;///< Disable the copy protection check. ; =1 コピープロテクト無し
FLASH_DEV_CODE = $A4   ;///< Expected flash device ID, `$A4` for the AM29F040B. 
FLASH_MAN_CODE = $C2   ;///< Expected flash manufacturer ID. @note Disagrees with `rom/flashdevice.nut`; unused, so inert. @see @ref flashing 
WRAM_PROTECT_CODE = 0   ;///< Cartridge RAM write-protect code. 

ARDUINO_MODE = 0   ;///< Build for a cartridge carrying the FC-EXA expansion adapter. @note Not FC PICO. ; =1 ARDUINO 搭載モード

