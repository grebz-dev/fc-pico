/*
    rp_debug.h
 */

/**
 * @file rp_debug.h
 * @brief Execution tracing, assertions and the dual-core watchdog handover.
 * @ingroup platform
 *
 * Two independent facilities:
 *
 * - A 16-entry ring of `(trace id, line number)` pairs, written by #TRACE. It
 *   survives a watchdog reset long enough to show where the firmware was when it
 *   stopped responding.
 * - The watchdog itself, which is unusual here because **either core** may feed
 *   it depending on whether the console link is active. @see setWDT_mode
 *
 * Define @c NDEBUG to compile the tracing and assertions away entirely.
 */

#ifndef rp_debug_h
#define rp_debug_h

/// @brief Uncomment to compile out #TRACE, #TRACE_END and #ASSERT.
//#define NDEBUG	1


/**
 * @brief Trace-point identifiers recorded by #TRACE.
 * @ingroup platform
 * @details One per major execution region, so the ring shows which part of the
 *          frame was running rather than just a line number.
 */
// デバッグトレース番号
enum {
	DTR_ROOT,    ///< Core 0 main loop.
	DTR_MAIN,    ///< Application frame update.
	DTR_TITLE,   ///< Title scene.
	DTR_DEMO,    ///< Attract/demo mode.

	DTR_MAX,     ///< Number of trace points; sizes the name table.
};

/// @brief Depth of the trace ring. Must be a power of two.
#define DTR_LOG_SIZE  16



#ifdef NDEBUG
/// @brief Records a trace point. Compiled out when #NDEBUG is defined.
/// @param dtrno One of the `DTR_*` identifiers.
#define TRACE(dtrno) /* Nothing */
/// @brief Records an end-of-region trace point. Compiled out when #NDEBUG is defined.
/// @param dtrno One of the `DTR_*` identifiers.
#define TRACE_END(dtrno) /* Nothing */
#else
/// @brief Records a trace point together with the current line number.
/// @param dtrno One of the `DTR_*` identifiers.
#define TRACE(dtrno) setDebugTrace( dtrno, __LINE__);
/// @brief Records an end-of-region trace point.
/// @param no One of the `DTR_*` identifiers.
#define TRACE_END(no) setDebugTrace( no, __LINE__);
#endif



//---------------------------------------
// ASSERT
//---------------------------------------
/// @brief Stringisation helper; do not use directly.  @param arg Token to stringise.
#define QUOTE_detail(arg) #arg
/// @brief Stringises a macro's expansion.  @param arg Token to stringise.
#define QUOTE(arg) QUOTE_detail(arg)

#ifdef NDEBUG
/// @brief Runtime assertion. Compiled out when #NDEBUG is defined.
/// @param arg Condition that must hold.
#define ASSERT(arg) /* Nothing */
#else

/**
 * @brief Halts with a serial message if @p arg is false.
 * @param arg Condition that must hold.
 * @warning On failure this enters an infinite loop rather than resetting. The
 *          watchdog will fire ~5 s later, so a tripped assertion presents as a
 *          reboot loop unless a serial terminal is attached to catch the message.
 */
#define ASSERT(arg) \
    do \
    { \
        if ( false == static_cast<bool>(arg) ) \
        { \
            Serial.println("assert failed on line #" QUOTE(__FILE__) QUOTE(__LINE__) " : '" #arg "'" \
                           " -- now entering infinite loop"); \
            for (;;) yield(); \
        } \
    } while ( false )
#endif


/// @brief Size of the formatted-trace scratch buffer.
#define TRS_BUFF_SIZE  16


/**
 * @brief Enables the hardware watchdog with a 5000 ms timeout.
 * @ingroup platform
 */
void WDT_init();

/**
 * @brief Feeds the watchdog and records the time it was fed.
 * @ingroup platform
 * @note Called from core 0's loop and from the longer-running paths in
 *       rp_system::jobRcvCom().
 */
void WDT_update();

/**
 * @brief Feeds the watchdog on core 0's behalf, if permitted.
 * @ingroup platform
 * @details Runs on core 1. If more than 2000 ms have passed since the last
 *          WDT_update() **and** the mode is 0, core 1 feeds the dog. In mode 1
 *          it does not, so a stalled core 0 causes a reset instead of a hang.
 * @see setWDT_mode
 */
void WDT_check();

/**
 * @brief Selects which core is responsible for the watchdog.
 * @param mode 0: core 1 may feed it (console link idle).
 *             1: only core 0 may feed it (console link live).
 * @ingroup platform
 * @details Set to 1 by rp_system::jobRcvCom() whenever the console is talking, and
 *          back to 0 when a #PF_DAT_STEP is issued. The effect is that once the
 *          link is up, a core 0 stall reboots the cartridge rather than freezing
 *          the display.
 */
void setWDT_mode( uint8_t mode  );

/**
 * @brief Appends an entry to the trace ring.
 * @param dtrno One of the `DTR_*` identifiers.
 * @param line Source line number, normally `__LINE__`.
 * @ingroup platform
 */
void setDebugTrace( uint8_t dtrno, uint16_t line );


#endif

