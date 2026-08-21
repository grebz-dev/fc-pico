/**
 * @file system.h
 * @brief The unity build: pulls every translation unit into one compilation.
 * @ingroup platform
 *
 * The Arduino IDE compiles only the sketch directory, so the platform layer under
 * `sys/` is brought in by textual inclusion of the `.cpp` files themselves rather
 * than by separate compilation.
 *
 * @warning **Include order matters.** rp_core0.h and rp_core1.h define the
 *          Arduino entry points and reference the global instances (`sys`, `ap`,
 *          `snd`) that are defined at the bottom of the `.cpp` files above them.
 *          They must stay last.
 *
 * @note A new source file under `sys/` will not be built until it is added here.
 * @see @ref architecture, @ref dev_setup
 */

#include "_build_option.h"

#include "sys/rp_system.h"

#include "ap_data.h"
#include "ap_main.h"

#include "sys/ArduinoGL.cpp"
#include "sys/Canvas.cpp"
#include "sys/Obj3d.cpp"
#include "sys/rp_debug.cpp"
#include "sys/rp_dma.cpp"
#include "sys/rp_fcemu.cpp"
#include "sys/rp_nsfplayer.cpp"
#include "sys/rp_sound.cpp"
#include "sys/rp_system.cpp"
#include "sys/rp_bpe.cpp"



#include "sys/rp_core0.h"
#include "sys/rp_core1.h"
