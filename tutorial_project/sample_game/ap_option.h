/*
    ap_option.h
 */

#ifndef ap_option_h
#define ap_option_h

#include "_build_option.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"

enum {
	OBJ_OPT_LOGC = 0,
	OBJ_OPT_CUBE = 1,

	OPTION_MENU_MAX = 7,
};


class ap_option {
    
public:
    ap_option() {};
    void init(void);
    void main();

private:
    void resetDemoTime();

	uint8_t option_sel;
	unsigned long demo_time;
	uint8_t op_dt[ OPTION_MENU_MAX ];

};

extern ap_option ap_op;

#endif

