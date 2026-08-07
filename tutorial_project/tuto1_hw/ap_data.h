/*
    ap_data.h
 */

#ifndef ap_data_h
#define ap_data_h

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"
#include "sys/Obj3d.h"

enum {
	MDL_TITLE_LOGO = 0,
	MDL_PLAYER_NO,
	MDL_ENEMY_NO,
	MDL_ENEMY2_NO,
	MDL_ENEMY3_NO,

};

extern const unsigned char _font[4096];
extern const unsigned char _acOBJ[4096];
extern const unsigned char sound_nsf[];
extern void setModelDataObj( Obj3d *obj, int no );
extern void setMP3data( int no );

#endif

