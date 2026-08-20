/*
    ap_over.h
 */

#ifndef ap_over_h
#define ap_over_h

#include "_build_option.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"

enum {
	OBJ_OVE_MODEL = 0,
	OBJ_OVE_STAR = 1,
};


class ap_over {
    
public:
    ap_over() {};
    void init(void);
    void main();

private:

	unsigned long demo_time;
};

extern ap_over ap_ov;

#endif

