
;==========================================================
; ミッション制御関連定義
;==========================================================
MT_HARA		EQU  0		; HARADIUSミッション
MT_FLET		EQU  1		; 艦隊ミッション
MT_BOSS		EQU  2		; ボス敵ミッション
MT_EVNT		EQU  3		; イベント

MT_MAX		EQU  4		; ミッションタイプMAX

;---- 特殊ミッション制御 ----
MT_ATK_NO	EQU  $FB	; ミッション攻撃番号セット
MT_ANM_NO	EQU  $FC	; ミッションアニメ番号セット
MT_BOSSJMP	EQU  $FD	; デモ時の開始ボスミッションにジャンプ
MT_DEMOJMP	EQU  $FE	; デモ時の開始ミッションにジャンプ
MT_END		EQU  $FF	; ミッション終了


;==========================================================
; ミッションサブ　バリエーションタイプ
;==========================================================
MTSV_0		EQU  $00
MTSV_1		EQU  $40
MTSV_2		EQU  $80
MTSV_3		EQU  $C0

;==========================================================
; ミッション　アニメ番号(下位２ビットはバリエーション番号）
;==========================================================
MTA_OFF			EQU  $00*4		; アニメオフ


MTA_ASTRO_F		EQU  $80+0*4	; 隕石　右から
MTA_ASTRO_B		EQU  $80+1*4	; 隕石　左から
MTA_ASTRO_U		EQU  $80+2*4	; 隕石　上から
MTA_ASTRO_D		EQU  $80+3*4	; 隕石　下から
MTA_ZAKO_F0		EQU  $80+4*4	; 敵　前から 弱ザコのみ
MTA_ZAKO_F1		EQU  $80+5*4	; 敵　前から 硬ザコMIX
MTA_ZAKO_F2		EQU  $80+6*4	; 敵　前から ブラックタイガー隊
MTA_WARPIN0		EQU  $80+7*4	; ワープIN 弱ザコ
MTA_WARPIN1		EQU  $80+8*4	; ワープIN ブラックタイガー
MTA_WARPIN2		EQU  $80+9*4	; ワープIN 隕石


;==========================================================
; ミッション　SP敵攻撃パターン(下位２ビットは攻撃頻度）
;==========================================================
MAA_OFF			EQU  $00*4		; 攻撃オフ
MAA_1SHOT		EQU  $01*4		; １発攻撃
MAA_3SHOT		EQU  $02*4		; ３発攻撃



;----------------------------------------------
; ミッションコントロールコード
;----------------------------------------------
_MC_END			EQU  $00
_MC_CALL	 	EQU  $FF	; MC_CALL, 呼び出し先アドレスラベル　　コールは先からコールは不可
_MC_RET			EQU  $FE
_MC_LOOP_CNT	EQU  $FD	; MC_LOOP_CNT, （ループ回数）
_MC_JMP			EQU  $FC	; MC_JMP, ジャンプ条件, ジャンプ先アドレス
_MC_ZAKO		EQU  $FB	; 雑魚敵セット
_MC_MEMCPYN		EQU  $FA	; メモリーコピー Nバイト版
_MC_MEMCPY2		EQU  $F9	; メモリーコピー 2バイト版
_MC_BOSS_NS		EQU  $F8	; ボス通常弾発射
_MC_PALSET		EQU  $F7	; パレット書き換え
_MC_MEMSET		EQU  $F6	; メモリーセット 1バイト版
_MC_MEMSET2		EQU  $F5	; メモリーセット 2バイト版
_MC_PGCALL		EQU  $F4	; プログラムコール プログラムを呼び出す
_MC_MEMCLR		EQU  $F3	; メモリークリアー

_MC_BG_ANIME	EQU  $F2	;  BGにアニメーションデータをセットする
_MC_PGCALL2		EQU  $F1	;  バンク付きプログラムコール プログラムを呼び出す
;_MC_VRAMSET		EQU  $F0	;  VRAMセット　アドレス、値
_MC_MEMADD		EQU  $EF	;  メモリー加算 1バイト版 アドレス、値
_MC_MEMCMP		EQU  $EE	;  メモリー比較 1バイト版 アドレス、値
_MC_MEMPUSH		EQU  $ED	;  メモリー 1バイト PUSH
_MC_MEMPOP		EQU  $EC	;  メモリー 1バイト POP


_MC_BASE	EQU  $EC		; コマンド予約の開始番号


;----------------------------------------------
; ミッション拡張関数コード
;----------------------------------------------



;----------------------------------------------
; ミッションコントロールコード　ジャンプ条件
;----------------------------------------------
MCJ_JMP			EQU  0		; 無条件ジャンプ
MCJ_LOOP_CNT	EQU  1		; ループカウンターをマイナス１してゼロでなければジャンプ
							; MC_LOOP_CNTであらかじめループ回数セット

;MCJ_BOSS_HP		EQU  2		; ボスＨＰがボスＨＰ比較データより大きければジャンプ
MCJ_ENEMY_Z		EQU  3		; BG敵の残りがゼロならジャンプ
MCJ_EBG_TAG_Z	EQU  4		; 指定タグのＢＧ敵がゼロならジャンプ
MCJ_ESP_TAG_Z	EQU  5		; 指定タグのＳＰ敵がゼロならジャンプ
MCJ_CMP_Z		EQU  6		; メモリー比較結果がZならジャンプ
MCJ_CMP_NZ		EQU  7		; メモリー比較結果がNZならジャンプ
MCJ_CMP_C		EQU  8		; メモリー比較結果がCならジャンプ
MCJ_CMP_NC		EQU  9		; メモリー比較結果がNCならジャンプ


;----------------
; ミッションコントロールマクロ
;----------------

MC_END MACRO
	DB	_MC_END
	ENDM

MC_CALL MACRO
	DB	_MC_CALL
	DW  \1			; 呼び出し先アドレスラベル
	ENDM

MC_RET MACRO
	DB	_MC_RET
	ENDM

MC_LOOP_CNT MACRO
	DB	_MC_LOOP_CNT
	DB  \1			; ループ回数
	ENDM

MC_JMP MACRO
	DB	_MC_JMP
	DW  \1			; ジャンプ先アドレスラベル
	DB  \2			; ジャンプ条件
	DB  0
	ENDM

MC_JMP2 MACRO
	DB	_MC_JMP
	DW  \1			; ジャンプ先アドレスラベル
	DB  \2			; ジャンプ条件
	DB  \3			; ジャンプ条件
	ENDM



MC_ZAKO MACRO
	DB	_MC_ZAKO
	DB  \1			; 種類
	DB  \2			; 移動パターン（最下位ビット=1 でX軸反転）
	DB  ((\3) /2)		; Ｘオフセット 0-511 (1/2してセットされる）
	DB  \4			; Ｙオフセット -128 から 127　まで
	ENDM


;-------------------------------
;  LBF_ZAKO 用弾発射
;-------------------------------
MC_BOSS_NS MACRO
	DB	_MC_BOSS_NS
	DB  \1			; LASTER_BG IDX
	DB  \2			; ターゲット
	DB  \3			; 弾の種類
	DB  \4			; 移動パターン
	ENDM

MC_BOSS_SS MACRO
	DB	_MC_BOSS_SS
	DB  \1			; Ｘオフセット -128 から 127　まで
	DB  \2			; Ｙオフセット -128 から 127　まで
	DB  \3			; パラメーター
	ENDM

MC_BOSS_HS MACRO
	DB	_MC_BOSS_HS
	DB  \1			; Ｘオフセット -128 から 127　まで
	DB  \2			; Ｙオフセット -128 から 127　まで
	DB  \3			; 弾の種類
	DB  \4			; 移動パターン
	ENDM

MC_PALSET MACRO
	DB	_MC_PALSET
	DB  \1			; セット位置 4xN +1 (N= 0～7)
	DB  \2,\3,\4	; パレットデータ
	ENDM

MC_MEMSET MACRO
	DB	_MC_MEMSET
	DW  \1			; 書き換えアドレス
	DB  \2			; 書き換えデータ
	ENDM

MC_MEMSET2 MACRO
	DB	_MC_MEMSET2
	DW  \1			; 書き換えアドレス
	DW  \2			; 書き換えデータ
	ENDM

MC_MEMSET3B MACRO
	DB	_MC_MEMSET2
	DW  \1			; 書き換えアドレス
	DB  \2,\3		; 書き換えデータ
	DB	_MC_MEMSET
	DW  \1+2		; 書き換えアドレス
	DB  \4			; 書き換えデータ
	ENDM

MC_MEMADD MACRO
	DB	_MC_MEMADD
	DW  \1			; アドレス
	DB  \2			; 加算データ
	ENDM

MC_MEMCMP MACRO
	DB	_MC_MEMCMP
	DW  \1			; アドレス
	DB  \2			; 比較データ
	ENDM

MC_MEMCPYN MACRO
	DB	_MC_MEMCPYN
	DW  \2			; アドレス SRC
	DW  \1			; アドレス DST
	DB  \3
	ENDM

MC_MEMCPY2 MACRO
	DB	_MC_MEMCPY2
	DW  \2			; アドレス SRC
	DW  \1			; アドレス DST
	ENDM


MC_MEMPUSH MACRO
	DB	_MC_MEMPUSH
	DW  \1			; PUSH変数アドレス
	ENDM

MC_MEMPOP MACRO
	DB	_MC_MEMPOP
	DW  \1			; POP変数アドレス
	ENDM


MC_PGCALL MACRO
	DB	_MC_PGCALL
	DB  #high( \1 -1)	; コールアドレス
	DB  #low( \1 -1)	; コールアドレス
	DB  0			; コール時にYreg にセットする値
	DB  \2			; コール時にAreg にセットする値
	ENDM

MC_PGCALL2 MACRO
	DB	_MC_PGCALL
	DB  #high( \1 -1)	; コールアドレス
	DB  #low( \1 -1)	; コールアドレス
	DB  \3			; コール時にYreg にセットする値
	DB  \2			; コール時にAreg にセットする値
	ENDM

MC_PGCALL_A MACRO
	DB	_MC_PGCALL2
	DW  \3			; TMP_ADR0にセットする値
	DB  #high( \1 -1)	; コールアドレス
	DB  #low( \1 -1)	; コールアドレス
	DB  0			; コール時にYreg にセットする値
	DB  \2			; コール時にAreg にセットする値
	ENDM




MC_MEMCLR MACRO
	DB	_MC_MEMCLR
	DW  \1			; アドレス
	DB  \2			; サイズ
	DB  \3			; クリア値
	ENDM

MC_BG_POS_CLR MACRO
	DB	_MC_BG_POS_CLR
	DB  \1			; Ｘ
	DB  \2			; Ｙ
	ENDM


MC_BG_TAG_SBG MACRO
	DB	_MC_BG_TAG_SBG
	DB  \1			; タグ番号
	DB  \2			; BG化する敵の種類番号
	ENDM

MC_BG_POS_HS MACRO
	DB	_MC_BG_POS_HS
	DB  \1			; Ｘ
	DB  \2			; Ｙ
	DB  \3			; 弾種類
	DB  \4			; 移動パターン
	ENDM






MC_WAIT MACRO
	DB  \1			; ウェイトフレーム数 1-200
	ENDM



;------------------------------------
; ミッション拡張関数
;  \1 ->拡張関数番号
;------------------------------------
MC_MISSION_FUNC MACRO
	DB	_MC_PGCALL
	DB  #high( MissionFunc -1)	; コールアドレス
	DB  #low( MissionFunc -1)	; コールアドレス
	DB  \1			; コール時にAreg にセットする値
	ENDM




