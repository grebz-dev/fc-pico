;===============================================================================================
;	ゲームバランス調整用
;===============================================================================================
EVENT_MODE	EQU  0		; =1 イベントモード

PLY_LIFE_INIT  EQU  10	; 自機の残数　初期値
;PLY_LIFE_INIT  EQU  1	; 自機の残数　初期値
COTI_MAX_LIFE  EQU  25  ; コンティニューによるLIFE上限アップ

DEMO_STAGE_MAX  EQU 3	; デモステージの最大数
STAGE_MAX  EQU 3		; ステージの最大数



PLY_HIT_ADD_X	EQU		4		; プレーヤー当たり判定位置調整Ｘ
PLY_HIT_ADD_Y	EQU		4		; プレーヤー当たり判定位置調整Ｙ
PLY_HIT_SIZ_W	EQU		8		; プレーヤー当たり判定サイズ調整Ｗ
PLY_HIT_SIZ_H	EQU		8		; プレーヤー当たり判定サイズ調整Ｈ



;PLY_LIM_XL		EQU		16			; プレイヤーＸ座標リミッター
;PLY_LIM_XH		EQU		256-16		; プレイヤーＸ座標リミッター
PLY_LIM_XL		EQU		32			; プレイヤーＸ座標リミッター
PLY_LIM_XH		EQU		256-32		; プレイヤーＸ座標リミッター
OPT_LIM_XH		EQU		256-32		; オプションＸ座標リミッター

PLY_LIM_YL		EQU		24			; プレイヤーＹ座標リミッター
PLY_LIM_YH		EQU		208			; プレイヤーＹ座標リミッター
OPT_LIM_YL		EQU		16			; オプションＹ座標リミッター
OPT_LIM_YH		EQU		200 -2		; オプションＹ座標リミッター


;IOSR_2			EQU		707/1000	; ルート２分の１ (0.707)
IOSR_2			EQU		100/100		; ルート２分の１ (0.707)
;IOSR_2			EQU		1			; ルート２分の１ (0.707)

MV_PLY_BASE0		EQU 	$200*3/2		; 自機の移送速度ベース
MV_PLY_BASE0_IR2	EQU 	$16a*3/2		; 自機の移送速度ベースのルート2分の1

;MV_PLY_BASE0		EQU 	$200*6/5		; 自機の移送速度ベース
;MV_PLY_BASE0_IR2	EQU 	$16a*6/5		; 自機の移送速度ベースのルート2分の1



MV_ENT_BASE0	EQU 120		; 敵の弾 スピード調整用 ※256ドットを指定フレームで移動する
MV_ENT_BASE1	EQU 100		; 敵の弾 スピード調整用 ※256ドットを指定フレームで移動する
MV_ENT_BASE2	EQU  80		; 敵の弾 スピード調整用 ※256ドットを指定フレームで移動する
MV_ENT_BASE3	EQU  60		; 敵の弾 スピード調整用 ※256ドットを指定フレームで移動する


POS_PLY_X_INIT	EQU		128			; プレーヤー初期位置Ｘ
POS_PLY_Y_INIT	EQU		192			; プレーヤー初期位置Ｙ


PSHOTA_SPD    EQU 8		; 自機通常弾速度



ENEMY_LINE_SUU	EQU		240-24		; 敵BG表示エリアライン数

BAKU_EFC_CHR equ $01

MUTEKI_TIME		equ 60	;ダメージ時の無敵期間
DAM_BG_FLASH_INIT equ 4 ; ダメージフラッシュタイム

PS_NOMAL_POW		equ -1	;通常弾のヒット時の敵ダメージ	(静止目標に、２回当たる）

; シークレット取得時のライフ回復量
ADD_SC_LIFE_ST1	EQU 1
ADD_SC_LIFE_ST2	EQU 2
ADD_SC_LIFE_ST3	EQU 3
ADD_SC_LIFE_ST4	EQU 3
ADD_SC_LIFE_ST5	EQU 3




