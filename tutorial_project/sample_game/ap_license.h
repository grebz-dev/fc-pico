/*
    ap_license.h
 */

#ifndef ap_license_h
#define ap_license_h

#include "_build_option.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"



class ap_license {
    
public:
    ap_license() {};
    void init(void);
    void main();

private:
    void resetDemoTime();

	uint8_t option_sel;
	unsigned long demo_time;
	uint8_t op_dt[ OPTION_MENU_MAX ];

};

extern ap_license ap_li;

#endif

