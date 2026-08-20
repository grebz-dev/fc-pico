/*
    ap_game.h
 */

#ifndef ap_game_h
#define ap_game_h

#include "_build_option.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"



enum {
	OBJ_GAM_MODEL = 0,
	OBJ_GAM_PLAYER = 2,
	OBJ_GAM_BULLET = 3,		// 自機の弾のオブジェ番号
	OBJ_GAM_ENEMY = 64,
	OBJ_GAM_EXPEFC = 110,	// 爆発エフェクト

	OBJ_GAM_STAR = 128,
};


enum {
	GMST_MAIN		= 5,
	GMST_CLEAR		= 6,
	GMST_OVER		= 7,
};


//==========================================================
// プレーヤーアニメ制御関連定義
//==========================================================
enum {
	PLY_AN_WAIT		= 0,	// 待機
//	PLY_AN_CHARGE	EQU  1		; チャージ
//	PLY_AN_SHOTA	EQU  2		; ショットA
//	PLY_AN_SHOTB	EQU  3		; ショットB
	PLY_AN_DEAD		= 4,		// 死亡アニメ
};


enum {
	EM_DEMO_FG = 0xB7,	// デモフラグ

	EM_KEY_TRG = 0xC3,
	EM_KEY_NEW = 0xC5,

	EM_STG_COD	= 0xE8,
	EM_STG_COD_SUB	= 0xE9,


	EM_REQ_SE_NO	= 0xF5,
	EM_REQ_SE_NO2	= 0xF6,
	EM_REQ_SE_NO3	= 0xF7, 
	EM_REQ_BGM_NO	= 0xFA,

	
	GM_SCORE		= 0x110,	// 4 bytes
	PLY_LIFE		= 0x11e,	// B 残機数
	PLY_STAGE		= 0x121,	// ステージ番号

	EM_PLY_STAGE = 0x121,		// ステージ番号
//	EM_MISSON_TYPE = 0x3A1,		// B ミッションタイプ
//	EM_MISSON_TYPE_SUB = 0x3A2,	// B ミッションタイプ
//	EM_MISSON_WAIT	= 0x3A3,    // B MISSON_PCの次の処理までのウェイト
//	EM_MISSON_PC	= 0x3A4,	// W ミッションPC

	

	STAGE_MAX = 3,				// ステージの最大数

	//----------------
	// ミッション制御系ワーク
	//----------------
	MISSON_NO		= 0x3A0,	// B ステージ内ミッション番号
	MISSON_TYPE		= 0x3A1,	// B ミッションタイプ
	MISSON_TYPE_SUB	= 0x3A2,	// B ミッションタイプ
	MISSON_WAIT		= 0x3A3,	// B MISSON_PCの次の処理までのウェイト
	MISSON_PC		= 0x3A4,	// W ミッションPC

	MISSON_LOOP_CNT	= 0x3A6,	// B ミッション ループカウンタ
	MISSON_FLG		= 0x3A9,	// B ミッション フラグ
	MISSON_STEP		= 0x3AA,	// B ミッション 処理ステップ
	MISSON_TMP		= 0x3AB,	// B ミッション 汎用

	DAM_BG_FLASH	= 0x3B2,	// ダメージＢＧフラッシュ

	PLY_ANM_NO		= 0x3B3,	// アニメーション番号



//	PLY_FORM		= 0x3B4,	// フォーメーション
//	PLY_DISP_FG		= 0x3B5,	// プレーヤー表示制御
	PLY_MUTEKI_TM	= 0x3B6,	// 無敵タイマー

	PLY_OBJ_KIND	= 0x500,
	PLY_OBJ_DIR		= 0x501,
	PSHOT_KIND		= 0x502,
	PSHOT_DIR		= 0x503,
	
	POS_PLY_X		= 0x520,
	POS_PLY_Y		= 0x521,

	PSHOT_A_X		= (POS_PLY_X+2),
	PSHOT_A_Y		= (POS_PLY_Y+2),
	PSHOT_A_SUU	 = 15,

	PLY_OBJ_WX		= 0x540,
	PLY_OBJ_WY		= 0x541,
	PSHOT_A_WX		= (PLY_OBJ_WX+2),
	PSHOT_A_WY		= (PLY_OBJ_WY+2),

	
	//----------------
	// 敵のノーマル弾ワーク
	// $600-$6AF 8x22セット
	//----------------
	ENEMY_NT_WORK	= 0x600,
	ENEMY_NT_KIND	= 0x600,
	ENEMY_NT_MP		= 0x601,	// 移動パターン番号
	ENEMY_NT_DT		= 0x602,	// 特殊制御用データ
	ENEMY_NT_HP		= 0x603,	// 耐久力
	ENEMY_NT_X		= 0x604,	// W
	ENEMY_NT_Y		= 0x606,	// W

	ENEMY_NT_SIZE	= 8,
	ENEMY_NT_SUU	= 22,

	//----------------
	// 爆発演出ワーク
	// 3x13セット
	//----------------
	BAKU_EFC_X		= 0x6B0,
	BAKU_EFC_Y		= 0x6B1,
	BAKU_EFC_CNT	= 0x6B2,

	BAKU_EFC_SUU = 13,


	ENEMY_LINE_SUU	= 240-24,	// 敵BG表示エリアライン数

};

//;========================================
//  敵種類定義
//;========================================
enum {
	NTK_ANGLE	= 1,	// 自機狙い弾
	NTK_NOMAL	= 2,	// 通常弾
	NTK_HORMI	= 3,	// ホーミング弾
	NTK_MISS	= 4,	// ミサイル弾
	NTK_HHORM	= 6,	// 半誘導弾
	NTK_HHORM3	= 8,	// 半誘導弾３分裂
	NTK_METEO	= 0x80+11,	// 隕石
	NTK_WARP	= 0x80+12,	// ワープエフェクト
	NTK_SPZK0	= 0x80+13,	// SPザコ0
	NTK_SPZK1	= 0x80+14,	// SPザコ1
	NTK_SPZK2	= 0x80+15,	// SPザコ2
	NTK_LIFE	= 0x80+16,	// ライフ回復

};


class ap_game {
    
public:
    ap_game() {};
    void init(void);
    void main();

private:
	// アセンブラソースから移植
	void initMission() {
		
	};
	void getMission();
	
	void drawRader();
	void conv3DObje();
	void conv3Dxy( Obj3d  *obj, int x, int y);

	int m_over_wait;
	int m_clear_wait;

	uint8_t m_emobj_kind[ ENEMY_NT_SUU ];

};

extern ap_game ap_g;

#endif

