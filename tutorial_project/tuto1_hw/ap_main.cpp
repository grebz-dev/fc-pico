/*
    rp_system.h
 */

/**
 * @file ap_main.cpp
 * @brief Implementation of the application scene state machine.
 * @ingroup app
 * @see ap_main.h
 */



#include "ap_main.h"


ap_main ap;




void ap_main::init() {
//	Serial.printf("ap_main::init\n" );
	setStep( 0 );

//	Serial.printf("SaveData[0] %02x\n", sys.SaveData[ SDT_HEAD ] );
	
	if ( sys.SaveData[ SDT_HEAD ] == 0xff ) {
		// セーブデータ初期化
		sys.SaveData[ SDT_HEAD ] = 1;
		sys.SaveData[ SDT_MP3_ENA ] = 1;
		sys.SaveData[ SDT_MP3_VOL ] = 20;
		sys.SaveData[ SDT_GAME_MODE ] = 0;
		sys.commitSaveData();
	}

	snd.m_MP3_ENA = sys.SaveData[ SDT_MP3_ENA ];
	snd.m_MP3_VOL = sys.SaveData[ SDT_MP3_VOL ];

	m_DemoFG = 0;
}



void ap_main::setStep( uint8_t step ) {
//	Serial.printf("setStep %d\n", step );

	m_step = step;
	m_step_sub = 0;
}
void ap_main::setStepSub( uint8_t step_sub ) {
	m_step_sub = step_sub;
}


void ap_main::main() {
	TRACE(DTR_MAIN)
	unsigned long ap_main_time_old = ap_main_time;
	ap_main_time = micros();

	sys.setKeyUpdate();
	switch ( m_step ) {
	case ST_INIT:
		TRACE(DTR_MAIN)
		setStep( ST_TITLE );
		break;

	case ST_TITLE:
		if ( m_step_sub == 0 ) {
			TRACE(DTR_MAIN)
			ap_t.init();
			m_step_sub++;
		} else {
			TRACE(DTR_MAIN)
			ap_t.main();
		}
		break;

	case ST_WAIT:
		break;
	
	default:
		setStep( ST_INIT );
		break;
	}
//	TRACE_END(DTR_MAIN)

#if 0
	{
		char buffer[40]; // バッファを確保
		sprintf(buffer, "FPS:%8d %8d", micros() - ap_main_time,  ap_main_time - ap_main_time_old );
		c.drawString( buffer, 40, 40, _font );
	}
#endif
	ap.m_timer++;
}


