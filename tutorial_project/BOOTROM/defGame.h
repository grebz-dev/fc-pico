;/// @file defGame.h
;/// @brief Application step codes and sprite constants.
;/// @ingroup bootrom

;----------------
; 各種P定義
;----------------
SP_CLR_Y	EQU 240   ;///< Y coordinate that parks a sprite off screen. ; スプライトクリアーY

;----------------
; STEP定義
;----------------
ST_INIT		EQU	 0   ;///< Application step: initialisation. ; 初期化
;ST_EXA00	EQU	 1	; 拡張システム起動チェック
ST_MAIN		EQU	 2   ;///< Application step: main scene. 

ST_MAX		EQU	 7   ;///< Number of application steps. ; ステップの最大値


