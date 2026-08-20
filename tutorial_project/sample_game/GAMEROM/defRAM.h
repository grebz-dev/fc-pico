;========================================


;----------------
; サウンドドライバーのワーク
;----------------
SND_WK0		EQU	$00	; size $32

;========================================
;  NSF再生用
;========================================
__ptr		EQU	$05	; 汎用ポインタ 2byte
__tmp		EQU	$07

__flag		EQU $0A
 .if 0
	lda	#nsd_flag::BGM + nsd_flag::SE
	sta	__flag		;BGM, SE処理を禁止（RAM未初期化対策）

	__flag
		D... .... : 再生制御無効
		...J .... : 早送り中
		.... PP.. : 効果音の優先度
		.... ..S. : ＳＥ再生中
		.... ...B : ＢＧＭが再生中 
 .endif

;========================================
W_AR		EQU	$40	; 16 bit 計算用  2 bytes
W_BR		EQU	$42	; 16 bit 計算用  2 bytes

TMP_SVA		EQU	$44	; 汎用 A レジスタ保存用アドレス
TMP_SVX		EQU	$45	; 汎用 X レジスタ保存用アドレス
TMP_SVY		EQU	$46	; 汎用 Y レジスタ保存用アドレス
TMP_LOOP_CNT	EQU	$47	; 汎用ループカウンタ

TMP_SV0		EQU	$48	; 汎用レジスタ保存用アドレス
TMP_SV1		EQU	$49	; 汎用レジスタ保存用アドレス
TMP_SV2		EQU	$4A	; 汎用レジスタ保存用アドレス
TMP_SV3		EQU	$4B	; 汎用レジスタ保存用アドレス
TMP_SV4		EQU	$4C	; 汎用レジスタ保存用アドレス
TMP_SV5		EQU	$4D	; 汎用レジスタ保存用アドレス
TMP_SV6		EQU	$4E	; 汎用レジスタ保存用アドレス
TMP_SV7		EQU	$4F	; 汎用レジスタ保存用アドレス


TMP_WRK0	EQU	$50
TMP_WRK1	EQU	$51
TMP_WRK2	EQU	$52
TMP_WRK3	EQU	$53

TMP_DISP2	EQU	TMP_WRK3	; 1 byte  表示汎用


TMP_COUNT	EQU $54		; 2バイト


SRC_ADR		EQU	$56		; 汎用ソースアドレス  2 bytes
DST_ADR		EQU	$58		; 汎用デスティネーションアドレス  2 bytes

TMP_ADR0		EQU	$5A		; 汎用アドレス  2 bytes
TMP_ADR0_IDX	EQU	$5C		; 汎用アドレス  1 bytes
TMP_ADR1		EQU	$5D		; 汎用アドレス  2 bytes
TMP_ADR1_IDX	EQU	$5F		; 汎用アドレス  1 bytes

;----------------
; サブル－チン呼び出しパラメーター
;----------------
PRM_0		EQU $60
PRM_1		EQU $61
PRM_2		EQU $62
PRM_3		EQU $63
PRM_4		EQU $64
PRM_5		EQU $65
PRM_6		EQU $66
PRM_7		EQU $67

PRM_X_POS	EQU $68
PRM_Y_POS	EQU $69
PRM_W_POS	EQU $6A
PRM_H_POS	EQU $6B
PRM_WT_POS	EQU $6C
PRM_HT_POS	EQU $6D


;----------------
; 各画面毎に初期化して利用するワーク
;----------------
GM_TMP0	 	EQU	$70
GM_TMP1	 	EQU	$71
GM_TMP2	 	EQU	$72
GM_TMP3	 	EQU	$73
GM_TMP4	 	EQU	$74
GM_TMP5	 	EQU	$75
GM_TMP6	 	EQU	$76
GM_TMP7	 	EQU	$77
GM_TMP8	 	EQU	$78
GM_TMP9	 	EQU	$79
GM_TMP10 	EQU	$7A

; 選択画面系
DEBUG_KEY_CNT	EQU GM_TMP5	; デバッグ突入チェック用
PUSH_CTR	EQU	GM_TMP7	; 文字＆カーソル点滅用


DEBUG_0		EQU  GM_TMP7
DEBUG_1		EQU  GM_TMP8
DEBUG_2		EQU  GM_TMP9
DEBUG_3		EQU  GM_TMP10

; 空き
DEBUG_COM	EQU	$80



USR_PROG	EQU  $8A	; USRプログラム用
SP_LOCK		  EQU $8D	; スプライト更新制御用 （=1 更新しない）

CACHE_GET_NENMY_NT_FG  EQU $8E


PAL_WRK		EQU	$90 		 ;size $20	転送用

;----------------
; デモ用
;----------------
DEMOMODE_NAM	EQU	$B0

DBD_BGTEST_FLG	EQU	$B1	; 非 0:BG テスト中 (3,4 面の水面制御抑制に使う)


GM_WAIT		EQU	$B2	; 2 bytes  ゲーム待ち

;----------------
; 処理落ち対策
;----------------
ENEMY_FLFG		EQU  $B4	;
ENEMY_NT_FLFG	EQU  $B5	;

;----------------
; 拡張アダプター モード
;----------------
EXA_MODE	EQU	$B6	; =0 拡張モード =1 スタンドアロンモード

;----------------
; ゲーム関連
;----------------
DEMO_FG		EQU	$B7	; デモフラグ
DEMO_TIMER	EQU	$B8	; デモタイマー




;----------------
; キー関連
;----------------
KEY_CH0D	EQU	$C0	; ΔPCM ノイズ除去用に増設 (新設)
KEY_CH2D	EQU	$C1	; ΔPCM ノイズ除去用に増設 (新設)

KEY_REL		EQU	$C2	;
KEY_TRG		EQU	$C3	;
KEY_OLD		EQU	$C4	;
KEY_NEW		EQU	$C5	;
KEY_CH0		EQU	$C6	;
KEY_CH1		EQU	$C7	;
KEY_CH2		EQU	$C8	; 拡張パッド用に増設 (新設)
KEY_CH3		EQU	$C9	; 拡張パッド用に増設 (新設)

REP_KEY		EQU	$CA	; リピート用のキー
REP_NEW		EQU	$CB	; リピートによる押下状態
REP_CNT		EQU	$CC	; ウェイト、インターバルのカウンタ

BG_STAR_DISP	EQU	$CD	; ゲーム中BG_STAR最大表示数
BG_STAR_MODE	EQU	$CE	; ゲーム中BG_STARスクロールモード

;----------------
; IRQ処理関連
;----------------

;HIRQ_ENA	EQU  $CF ; IRQ フラグ制御 (未使用=0)

SCR_LINE	EQU	$D0	; size 4 bytes	多重スクロール用 (各段の開始位置)
BG_SCR_X	EQU	$D4 ; size 4 bytes

BG_BNK		EQU	$DA	; size 6 bytes 
BG0_BNK		EQU	BG_BNK
BG1_BNK		EQU	BG_BNK+1



;----------------
; システム関連
;----------------
FLG_2000	EQU	$E0
FLG_2001	EQU	$E1
BG_SCR_Y	EQU	$E2

NMI_FLG		EQU	$E3
PAL_CHG_FG	EQU	$E4		; パレット変更フラグ

SYS_TIMER	EQU	$E5		; 2 bytes
FLM_TIMER	EQU	$E7		; フレームタイマー

STG_COD		EQU	$E8
STG_COD_SUB	EQU	$E9

;----------------
; バンク関連
;----------------
SPT_BNK		EQU	$EA
SPT_BNK2	EQU	$EB
A0_BNK		EQU	$EC		; バンク切り替えリクエスト用　実際にはVBankで切り替わる


;----------------
; NMIからコールするプログラムのアドレス
;----------------
NMI_CALL_BNK	EQU $ED ; 1byte ０ならコールしない
NMI_CALL_ADR	EQU $EE ; 2byte コールするプログラムアドレス


TMP_SYS		EQU	$F0		;システムで使うTMP
TMP_SYS2	EQU	$F1		;システムで使うTMP
TMP_SYS3	EQU	$F2		;システムで使うTMP
TMP_SYS4	EQU	$F3		;システムで使うTMP


;----------------
; サウンド関連
;----------------
REQ_TMPUP		EQU	$F4	; テンポアップ
REQ_SE_NO		EQU	$F5
REQ_SE_NO2		EQU	$F6	; 
REQ_SE_NO3		EQU	$F7	; 

;LAST_SE_LOCK	EQU	$F8	; 同一効果音の最低再生フレーム数
;LAST_SE_NO		EQU	$F9	; 最後に再生したSE
REQ_BGM_NO		EQU	$FA
;REQ_SE_NO		EQU	$FB
SEQ_CTR			EQU	$FC	; カウンタ
;SND_FLG			EQU	$FD
__MusBank		EQU $FD


MASTER_VOL		EQU $FF

WRAM_EXIST	EQU	$100	; 1 byte  非 0: WRAM が存在
				; 1 byte
HISCORES	EQU	$102	; 8 bytes LV1 のハイスコア,キャラ,
				;         LV2 のハイスコア,キャラ
MAGIC		EQU	$10a	; 6 bytes 起動/リセット判別用マジックナンバー


;----------------
; ランダムシステム
;----------------
RND_SEL		EQU	$10b
RND_WK0		EQU	$10c
RND_WK1		EQU	$10d
RND_WK2		EQU	$10e
RND_WK3		EQU	$10f


;----------------
; ゲーム内表示関連
;----------------
GM_SCORE	EQU	$110	; 4 bytes
GM_HISCORE	EQU	$114	; 4 bytes ハイスコア実作業用
SCR_CHG_SW	EQU	$118	; 1 byte  スコア変化フラグ

DEBUG_FLG		EQU	$11a	; デバッグモード突入フラグ
DEBUG_MT_FLG	EQU	$11b	; デバッグモード突入フラグ


PLY_LIFE		EQU $11e	; B 残機数
PLY_CONTINUE	EQU $11f	; コンティニュー回数 カウント上限99

;----------------
; その他
;----------------
DEBUG_SEL	EQU	$120

DEBUG_DT0	EQU	$121
DEBUG_DT1	EQU	$122
DEBUG_DT2	EQU	$123
DEBUG_DT3	EQU	$124
DEBUG_DT4	EQU	$125
DEBUG_DT5	EQU	$126
DEBUG_DT6	EQU	$127
DEBUG_DT7	EQU	$128

DEBUG_DT	EQU	DEBUG_DT0


PLY_STAGE		EQU	DEBUG_DT0	; ステージ番号
DBD_SOUND_TST	EQU	DEBUG_DT1	; サウンドテスト
DBD_M_TYPE		EQU	DEBUG_DT2	; ミッションタイプ
DBD_MT_SUB		EQU	DEBUG_DT3	; ミッションタイプサブ
DBD_STEP_JUMP	EQU	DEBUG_DT4	; ステップジャンプ



;========================================
; サウンドワーク
; $200-$328
;========================================
SND_WK1		EQU	$200	; size $128

;========================================
;  NSF再生用
;========================================
_eff		EQU $200		; 効果音テーブル開始番号
_play		EQU $201		; フレームオーバー防止用変数








;-----------------------------------------------------
; ここから下の$300台のワークは ステージ開始時に０クリアー
;-----------------------------------------------------
CLEAR_300W_TOP  EQU  $32E



MISSON_ATK_NO	EQU  $384	; B ミッション攻撃番号
MISSON_ATK_IDX	EQU  $385	; B ミッション攻撃インデックス
MISSON_ATK_CNT	EQU  $386	; B ミッション攻撃カウンター


MISSON_LDBG0	EQU  $38A	; B ミッションBG番号
MISSON_LDBG1	EQU  $38B	; B ミッションBG番号
MISSON_LDBG2	EQU  $38C	; B ミッションBG番号

MISSON_ANM_NO	EQU  $38D	; B ミッションアニメ番号
MISSON_ANM_CNT	EQU  $38E	; B ミッションアニメカウンター
;---- ミッション用ソフトスタック -------
MISSON_PC_SP	EQU  $38F	; B ミッションPCスタックポインタ

MISSON_STACK	EQU $390		; 16byte ミッション用スタック

;----------------
; ミッション制御系ワーク
;----------------
MISSON_NO		EQU $3A0	; B ステージ内ミッション番号
MISSON_TYPE		EQU $3A1	; B ミッションタイプ
MISSON_TYPE_SUB	EQU $3A2	; B ミッションタイプ
MISSON_WAIT		EQU $3A3	; B MISSON_PCの次の処理までのウェイト
MISSON_PC		EQU $3A4	; W ミッションPC

MISSON_LOOP_CNT	EQU $3A6	; B ミッション ループカウンタ
MISSON_FLG		EQU $3A9	; B ミッション フラグ
MISSON_STEP		EQU $3AA	; B ミッション 処理ステップ
MISSON_TMP		EQU $3AB	; B ミッション 汎用

SECRET_STAT		EQU $3AC	; B シークレット状態(0:初期値 1:解放 2:取得)
SECRET_LIFE_ADD	EQU $3AD	; B シークレットアイテム獲得時のライフボーナス

MISSON_ASM		EQU $3AE	; W ミッション毎フレーム割込み処理

ENEMY_ATK_LV	EQU $3B0	; B 敵の攻撃LV 
							;  0:攻撃しない 1:自機狙い弾 2:ホーミング弾
							;  3:自機狙い＆ホーミング 4: 高速ホーミング

MISSON_CMP_P	EQU $3B1    ; B 比較命令時のフラグ保存

DAM_BG_FLASH	EQU $3B2	; ダメージＢＧフラッシュ

;----------------
; プレーヤーワーク
;----------------
PLY_ANM_NO		EQU $3B3	; アニメーション番号
PLY_FORM		EQU $3B4	; フォーメーション
PLY_DISP_FG		EQU $3B5	; プレーヤー表示制御
PLY_MUTEKI_TM	EQU $3B6	; 無敵タイマー

;---- ボスミッション用 ワーク -------
BM_DEATH_ANM	EQU $3B7		; w ボス死亡アニメ
BM_DEATH_MSC	EQU $3B9		; w ボス死亡ミッションスクリプト

SHOT_TARGET		EQU $3BB


;----------------
; プレーヤーワーク
;----------------

PLY_OBJ_KIND	EQU	$500
PLY_OBJ_DIR		EQU	$501
PSHOT_KIND		EQU	$502
PSHOT_DIR		EQU	$503

POS_PLY_X		EQU $520
POS_PLY_Y		EQU $521

;----------------
; 自機の通常弾ワーク
;----------------
PSHOT_A_X	 EQU (POS_PLY_X+2)
PSHOT_A_Y	 EQU (POS_PLY_Y+2)	; =0 の時はスタンバイ状態
PSHOT_A_SUU	 EQU 15


PLY_OBJ_WX		EQU	$540
PLY_OBJ_WY		EQU	$541
PSHOT_A_WX		 EQU (PLY_OBJ_WX+2)
PSHOT_A_WY		 EQU (PLY_OBJ_WY+2)




;----------------
; 敵のノーマル弾ワーク
; $600-$6AF 8x22セット
;----------------
ENEMY_NT_WORK	EQU	$600
ENEMY_NT_KIND	EQU	$600
ENEMY_NT_MP		EQU	$601	; 移動パターン番号
ENEMY_NT_DT		EQU	$602	; 特殊制御用データ
ENEMY_NT_HP		EQU	$603	; 耐久力
ENEMY_NT_X		EQU $604	; W
ENEMY_NT_Y		EQU $606	; W

ENEMY_NT_SIZE	 EQU 8
ENEMY_NT_SUU	 EQU 22

;----------------
; 爆発演出ワーク
; $6B0-$6D7 3x13セット
;----------------
BAKU_EFC_X	 EQU $6B0
BAKU_EFC_Y	 EQU $6B1
BAKU_EFC_CNT EQU $6B2

BAKU_EFC_SUU EQU 13



;----------------
; パレット関連
;----------------
PALFADE_TIME	EQU	$6D7	; パレットフェード速度
PALFADE_CNT		EQU	$6D8	; パレットフェードカウンタ
PALFADE_VAL		EQU	$6D9	; 加算値、減算値
PALFADE_ADD		EQU	$6DA	; 変化の加算値
PALFADE_MASK	EQU	$60B	; 変化させないパレットビット指定

;--- 空きあり ---
; 6DC-6DF


PAL_WRK2	EQU	$6E0 	;size $20	フェード中転送用


OBJ_BUF		EQU	$700		; 256 bytes
BPE_BUF 	EQU	$700		; BEP展開バッファ


