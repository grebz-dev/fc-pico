/*
    ap_main.h
 */

#ifndef ap_main_h
#define ap_main_h

#include "_build_option.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"

#include "ap_title.h"
#include "ap_game.h"
#include "ap_over.h"
#include "ap_clear.h"
#include "ap_demo0.h"
#include "ap_data.h"
#include "ap_option.h"
#include "ap_license.h"

// プロセス間通信用
enum {
	C1_RESET,
	C1_SNDJOB,

	C1_SND_MP3PLAY = 0x10000000,
	

};


enum {
	ST_INIT = 0,
	ST_TITLE,
	ST_GAME,
	ST_DEMO0,
	ST_DEMO1,
	ST_OVER,
	ST_CLEAR,
	ST_OPTION,
	ST_LICENSE,

	ST_WAIT = 255,
};

// セーブデータ
enum {
	SDT_HEAD = 0,
	SDT_MP3_ENA,
	SDT_MP3_VOL,
	SDT_GAME_MODE,

};


enum {
	BGM_STOP = 0,
	BGM_BOSS,
	BGM_MAIN,
	BGM_CLEAR,
	BGM_OVER,

	SE_CUR_SEL,		// 00 カーソル 移動
	SE_CUR_ENT,		// 01 カーソル 決定
	SE_CUR_CAN,		// 02 カーソル キャンセル　(オプション 使用)
	SE_SHOT_A,		// 03 自機ショット音
	SE_PLY_DAME,	// 04 自機ダメージ音
	SE_PLY_DEAD,	// 05 自機死亡
	SE_PLY_FORM,	// 06 自機フォーメーションチェンジ

	SE_BAKU_S,		// 07 敵 撃破 敵サイズ小 ザコ
	SE_BAKU_M,		// 08 敵 撃破 敵サイズ中 ザコ
	SE_BAKU_L,		// 09 敵 撃破 敵サイズ大 ボス

	SE_NO_DAME,		// 10 敵無敵音
	SE_DAME,		// 11 ダメージ受け音

	SE_BOSS_MOVE1,	// 12 ボス移動1
	SE_BOSS_MOVE2,	// 13 ボス移動2

	SE_BOSS_ATK1,	// 14 ボス攻撃
	SE_TITLE,		// 15 タイトルＳＥ
	SE_START_JET,	// 16 スタートジェット
	SE_HADOU_CHG,	// 17 波動砲　チャージ
	SE_HADOU_SHT,	// 18 波動砲　発射
	SE_DM_DIVE,		// 19 次元潜航
	SE_YAMATO_S,	// 20 ヤマト発進

	SND_SEL_MAX,
};



enum {

	OBJ_MAX = 256,
};


class ap_main {
    
public:

    ap_main() {};
    void init();
    void initObj();

    void main();
    void move();
    void draw();



	void setStep( uint8_t step );
	void setStepSub( uint8_t step_sub );

	uint8_t getStep() { return m_step; }
	uint8_t getStepSub() { return m_step_sub; }
	Obj3d  m_obj[ OBJ_MAX ];

    void initStarObj( uint16_t StartStarObjNo, uint16_t StarObjNum, float StarSpd );
    void moveStarObj();
    void initStarObjGame( uint16_t StartStarObjNo, uint16_t StarObjNum, float StarSpd );
    void moveStarObjGame();

	uint16_t m_timer;
	uint8_t m_DemoFG;

private:
	uint8_t m_step;
	uint8_t m_step_sub;
	uint16_t m_StartStarObjNo;
	uint16_t m_StarObjNum;
	float m_StarSpd;
	unsigned long ap_main_time;



};

extern ap_main ap;
extern rp_fcemu emu;

#endif

