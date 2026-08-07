
;==========================================================
; デバッグコントロール定義
;==========================================================
; デバッグビルドコントロール
DEBUG_BUILD EQU  0

MAPPER_NO	EQU 0


; NMI とゲーム処理だけですが処理の重さを表示する場合は 1 にします
PROCESSMETER = 0

; ゲーム処理の重さを表示する場合は 1 にします
GAME_PROCESSMETER = 	0


; エンディングテストモード (即クリア確定)
ENDINGTEST = 0

; デバッグ機能ON
DEBUG_MODE = 0

; デバッグモードでテンポの速いステージ曲を再生可能にする場合は 1 にします。
; (「SOUND TST」で B を押しながら A を押す)
DEBUG_TEMPO_UP = 1

DEBUG_NO_GAME_OVER = 0

DEBUG_DISP_BOSS_HP = 0

DEBUG_DISP_PLY_DEBUG = 0


AUTO_SHOT_OFF = 0			; =1 の時、通常弾のオートショットをＡボタン押しっぱなしで停止出来る


SINGLE_SHOT_TEST = 0		; =1 自機の通常弾1発のみテスト
STAGE_TEST		 = 0		; !=0  指定ステージからスタート

MMC_TYPE = 0				; =0 MMC3  =1 AX-A1 =2 INL-SWAP
NO_COPY_PROTECT =  0		; =1 コピープロテクト無し
FLASH_DEV_CODE = $A4
FLASH_MAN_CODE = $C2
WRAM_PROTECT_CODE = 0

ARDUINO_MODE = 0			; =1 ARDUINO 搭載モード

