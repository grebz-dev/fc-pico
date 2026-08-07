/*
    ap_data.cpp
 */


#include "ap_data.h"


#include "res/OBJ.c"
#include "res/font.c"
#include "res/sound_nsf.c"



void setModelDataObj( Obj3d *obj, int no ) {
#if 0
	obj->init();
	switch( no ) {
	case MDL_TITLE_LOGO:
		obj->setModelData( FCPICO, DM_FCPICO );
		break;
	case MDL_PLAYER_NO:
		obj->setModelData( MDL_PLAYER, DM_MDL_PLAYER, mdl_player_col );
		obj->m_angle_x = 64;
		obj->m_scale = 0.7;
//		obj->m_scale = 2.0;
		break;

	case MDL_ENEMY_NO:
		obj->setModelData( MDL_ENEMY, DM_MDL_ENEMY, mdl_enemy_col );
		obj->m_angle_x = -64;
		obj->m_scale = 0.7;
		break;

	case MDL_ENEMY2_NO:
		obj->setModelData( MDL_ENEMY2, DM_MDL_ENEMY2, mdl_enemy2_col );
		obj->m_angle_x = -64;
		obj->m_scale = 0.7;
		break;

	case MDL_ENEMY3_NO:
		obj->setModelData( MDL_ENEMY3, DM_MDL_ENEMY3, mdl_enemy3_col );
		obj->m_angle_x = -64;
		obj->m_scale = 0.7;
		break;
	}
#endif

}


void setMP3data( int no ) {
//	Serial.printf("setMP3data %d\n", no );

#if 0
	switch( no ) {
	case 1:
		snd.setMP3( _mp3_007, _mp3_007_length, true );
		break;
	case 2:
		snd.setMP3( _mp3_005, _mp3_005_length, true );
		break;
	case 3:
		snd.setMP3( _mp3_013, _mp3_013_length, false );
		break;
	case 4:
		snd.setMP3( _mp3_014, _mp3_014_length, false );
		break;

	default:
		snd.stopMP3();
		break;
	}

#endif

}


