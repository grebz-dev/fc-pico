/*
    ap_title.cpp
*/


#include "ap_main.h"


ap_title ap_t;


const uint8_t pal_title[] = {
	// BG PAL
	0x0F,0x01,0x15,0x20,
	0x0F,0x2A,0x2A,0x2A,
	0x0F,0x12,0x2C,0x30,
	0x0F,0x1A,0x1A,0x1A,
	// OBJ PAL
	0x0F,0x0F,0x20,0x3c,
	0x0F,0x00,0x10,0x20,
	0x0F,0x15,0x19,0x20,
	0x0F,0x21,0x10,0x20
};


void ap_title::init(void) {

	snd.stopBGM();

	sys.startDataMode();

	ap.m_timer = 0;

	sys.clearAtrData();
	sys.setPalData( pal_title );
	sys.startDataMode();

	c.setSprData( _acOBJ );
}


void ap_title::main() {

	TRACE(DTR_TITLE)

	c.clear();

	// キーリピートテスト　上下ボタン
	if ( sys.getKeyRep() & (KEY_UP | KEY_DOWN)) {
		snd.playSE( SE_CUR_SEL );
	}

	// BGM再生テスト
	if ( sys.getKeyTrg() & KEY_A ) {
		static int bgm_sel = BGM_BOSS;
		snd.playBGM( bgm_sel );
		bgm_sel++;
		if ( bgm_sel > BGM_OVER ) {
			bgm_sel = BGM_BOSS;
		}
	}

	// 効果音再生テスト
	if ( sys.getKeyTrg() & KEY_B ) {
		snd.playSE( SE_CUR_SEL );
		snd.stopBGM();
	}

	c.setDefCol( 3 );
	c.drawString( "HELLO WORLD!", 8*10, 100, _font );

	c.setDefCol( 2 );
	c.drawString( "HELLO WORLD!", 8*10, 120, _font );

	c.setDefCol( 1 );
	c.drawString( "HELLO WORLD!", 8*10, 140, _font );


	{
		char buffer[40]; // バッファを確保
		sprintf(buffer, "%d",ap.m_timer );
		c.setSprMG( 2.0f, 2.0f );
		c.setSprFlip( 0 );
		c.setDefCol(3);
		c.drawString( buffer, 8*1, 20, _font );
	}


	TRACE_END(DTR_TITLE)
}


