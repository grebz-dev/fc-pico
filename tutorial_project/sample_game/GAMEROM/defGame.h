
;----------------
; PPU
;----------------
; MMC3のIRQを使う場合は BG を$0000番地, Spr を$1000番地に配置しなければならない
FLG_PPU2000	EQU	%101_01_0_00
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
; KEY DIR
;----------------
KDIR_N		EQU $FF
KDIR_U		EQU $00
KDIR_UR		EQU $01
KDIR_R		EQU $02
KDIR_DR		EQU $03
KDIR_D		EQU $04
KDIR_DL		EQU $05
KDIR_L		EQU $06
KDIR_UL		EQU $07


;----------------
; キーリピート設定
;----------------
REP_WAIT	EQU	24	; リピート開始までの時間 (フレーム数)
REP_INTERVAL	EQU	 8	; リピート間隔 (フレーム数)


;----------------
; バンク定義
;----------------
PBNK_SYS	EQU  $00



;----------------
; 各種P定義
;----------------
SP_CLR_Y	EQU 240		; スプライトクリアーY

;----------------
; STEP定義
;----------------
ST_INIT		EQU	 0	; 初期化
ST_EXA00	EQU	 1	; 拡張システム起動チェック
ST_TITLE	EQU	 2
ST_OPTION	EQU	 3
ST_DEBUG	EQU	 4
ST_MAIN		EQU	 5
ST_CLEAR	EQU	 6
ST_OVER		EQU	 7
ST_LICENSE	EQU	 8	; ライセンス

ST_MAX		EQU	 9	; ステップの最大値



;==========================================================
; デモタイマー関連定義
;==========================================================
TITLE_DEMO_TM	EQU	(7*60/16)	; 約7秒
GAME_DEMO_TM	EQU	(15*60/16)	; 約15秒
CREDIT_DEMO_TM	EQU	(5*60/16)	; 約5秒



;==========================================================
; プレーヤーアニメ制御関連定義
;==========================================================
PLY_AN_WAIT		EQU  0		; 待機
PLY_AN_CHARGE	EQU  1		; チャージ
PLY_AN_SHOTA	EQU  2		; ショットA
PLY_AN_SHOTB	EQU  3		; ショットB
PLY_AN_DEAD		EQU  4		; 死亡アニメ



;----------------
; サウンド定義
;----------------
BGM_BOSS	EQU  1	;
BGM_STAGE	EQU  2	;
BGM_CLEAR	EQU  3	;
BGM_OVER	EQU  4	;

SE_TOP_NO	EQU  5

SE_CUR_SEL		EQU  (SE_TOP_NO+0)	; 00 カーソル 移動
SE_CUR_ENT		EQU  (SE_TOP_NO+1)	; 01 カーソル 決定
SE_CUR_CAN		EQU  (SE_TOP_NO+2)	; 02 カーソル キャンセル　(オプション 使用)
SE_SHOT_A		EQU  (SE_TOP_NO+3)	; 03 自機ショット音
SE_PLY_DAME		EQU  (SE_TOP_NO+4)	; 04 自機ダメージ音
SE_PLY_DEAD		EQU  (SE_TOP_NO+5)	; 05 自機死亡
SE_PLY_FORM		EQU  (SE_TOP_NO+6)	; 06 自機フォーメーションチェンジ

SE_BAKU_S		EQU  (SE_TOP_NO+7)	; 07 敵 撃破 敵サイズ小 ザコ
SE_BAKU_M		EQU  (SE_TOP_NO+8)	; 08 敵 撃破 敵サイズ中 ザコ
SE_BAKU_L		EQU  (SE_TOP_NO+9)	; 09 敵 撃破 敵サイズ大 ボス

SE_NO_DAME		EQU  (SE_TOP_NO+10)	; 10 敵無敵音
SE_DAME			EQU  (SE_TOP_NO+11)	; 11 ダメージ受け音

SE_BOSS_MOVE1	EQU  (SE_TOP_NO+12)	; 12 ボス移動1
SE_BOSS_MOVE2	EQU  (SE_TOP_NO+13)	; 13 ボス移動2

SE_BOSS_ATK1	EQU  (SE_TOP_NO+14)	; 14 ボス攻撃
SE_TITLE		EQU  (SE_TOP_NO+15)	; 15 タイトルＳＥ
SE_START_JET	EQU  (SE_TOP_NO+16)	; 16 スタートジェット
SE_HADOU_CHG	EQU  (SE_TOP_NO+17)	; 17 波動砲　チャージ
SE_HADOU_SHT	EQU  (SE_TOP_NO+18)	; 18 波動砲　発射
SE_DM_DIVE		EQU  (SE_TOP_NO+19)	; 19 次元潜航
SE_YAMATO_S		EQU  (SE_TOP_NO+20)	; 20 ヤマト発進

SNDTST_MAX   EQU (SE_TOP_NO+21)


SE_TITLE_START	EQU  SE_CUR_ENT
SE_POWUP		EQU  SE_CUR_ENT
SE_BAKU_EFC		EQU  SE_BAKU_S
SE_BAKU_BG		EQU  SE_BAKU_M

SE_SPECIAL	EQU	 SE_PLY_FORM	;

BGM_GAME_CLEAR  EQU  BGM_CLEAR




