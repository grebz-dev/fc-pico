
;==========================================================
; デバッグコントロール定義
;==========================================================
; デバッグビルドコントロール
DEBUG_BUILD EQU  1

; マッパー番号
MAPPER_NO	EQU 0

; デバッグ機能ON
DEBUG_MODE = 1


DEBUG_NO_GAME_OVER = 0


AUTO_SHOT_OFF = 0			; =1 の時、通常弾のオートショットをＡボタン押しっぱなしで停止出来る


SINGLE_SHOT_TEST = 0		; =1 自機の通常弾1発のみテスト
STAGE_TEST		 = 0		; !=0  指定ステージからスタート =8 テスト

MMC_TYPE = 0				; =0 MMC3  =1 AX-A1 =2 INL-SWAP
NO_COPY_PROTECT =  1		; =1 コピープロテクト無し
FLASH_DEV_CODE = $A4
FLASH_MAN_CODE = $C2
WRAM_PROTECT_CODE = 0

ARDUINO_MODE = 0			; =1 ARDUINO 搭載モード

