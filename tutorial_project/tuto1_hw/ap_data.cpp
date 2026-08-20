/*
    ap_data.cpp
 */

#include "ap_data.h"

#include "res/resdata.c"



const unsigned char *_font;
const unsigned char *_acOBJ;


void initResData() {
	_acOBJ = getResData( CHR_OBJ );
	_font = getResData( CHR_FONT );
}

int*  getResHead( int resid ) {
	int sel = resid / 10000;
	if ( sel == 0 ) {
		return (int*)_resdata;
	} else {
		return (int*)RES_DATA_ADR;
	}
}

const unsigned char* getResData( int resid ) {
	int *head = getResHead( resid );
	resid %= 10000;
	const unsigned char *resbuf = (const unsigned char *)head;
	return &resbuf[  head[ resid * 2 + 0] ];
}


int getResDataSize( int resid ) {
	int *head = getResHead( resid );
	resid %= 10000;
	return head[ resid * 2 + 1];
}


void setModelDataObj( Obj3d *obj, int no ) {

	obj->init();

}


void setMP3data( int no ) {
/*
	int idx =  MP3_RES_ID + no -1;	//BGM_BOSS = 1
	const uint8_t* mp3data = getResData( idx );
	int size =  getResDataSize( idx );


	switch( no ) {
	case 1:
	case 2:
		snd.setMP3( mp3data, size, true );
		break;
	case 3:
	case 4:
		snd.setMP3( mp3data, size, false );
		break;

	default:
		snd.stopMP3();
		break;
	}
*/
}


