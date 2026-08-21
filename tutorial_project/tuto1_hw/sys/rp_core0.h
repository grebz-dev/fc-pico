/**
   Copyright (c) 2025 impact soft

*/

/**
 * @file rp_core0.h
 * @brief Core 0: the bus interrupt and the render loop.
 * @ingroup platform
 *
 * Core 0 owns everything time-critical about the console link. It services the
 * PIO interrupt, renders frames, and converts them into the PPU's byte order.
 *
 * The loop is **not** free-running: it is gated on rp_system::frame_draw, which
 * only the bus interrupt clears. The console's vertical blank therefore sets the
 * pace, and a frame the firmware cannot finish in time is dropped rather than
 * torn. @see @ref architecture
 */



//=================================================
//			LED点滅関数
//=================================================
/// @brief Toggles the status LED; called from the (currently disabled) repeating timer.
void BLINK_LED() {
	static char led_fg = 0;
	led_fg ^= 1;
	gpio_put(LED_PIN, led_fg);
}

//=================================================
//			割り込み関数
//=================================================


/**
 * @brief PIO0 interrupt handler -- the entry point for everything the console sends.
 * @details Fires whenever the #SM_RECV receive FIFO becomes non-empty, i.e. on
 *          every byte the 6502 writes to `$2007`. Clears the PIO IRQ flags and
 *          hands the byte to rp_system::jobRcvCom().
 * @warning This runs in interrupt context and sits inside the 6502's 1.3 ms
 *          spin-loop handshake. Anything slow here desynchronises the link.
 *          @see @ref protocol
 */
// 割り込みハンドラ
void pio0_itr0() {
	// IRQ命令でセットされた値を取得
	uint32_t irq = pio0_hw->irq;
	// 各ビットは１書き込む事でクリアできます
	pio0_hw->irq = irq;

	//	Serial.println("IRQ0");
	sys.jobRcvCom();

//	sys.WDT_update();
//	BLINK_LED();
	// 割り込み要求のクリア
	irq_clear(PIO0_IRQ_0);
}

/**
 * @brief Installs pio0_itr0() and enables the receive interrupt.
 * @details Arms it specifically on the RX-FIFO-not-empty condition of #SM_RECV,
 *          rather than on any state machine event.
 */
void enable_pico_ir() {
	irq_set_exclusive_handler(PIO0_IRQ_0, pio0_itr0);
	irq_set_enabled(PIO0_IRQ_0, true);
	//	pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;
	pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_RXNEMPTY_BITS;
}


//=================================================
//			タイマー割り込み
//=================================================

/**
 * @brief Repeating-timer callback.
 * @param rt Timer that fired.
 * @return Always true, to keep the timer running.
 * @note Not installed; init_timer_callback() leaves the registration commented out.
 */
bool timer_callback(repeating_timer_t *rt) {
	BLINK_LED();
	/* 500us待機 */
	//  busy_wait_us_32( 500 );
//	WDT_check();

	return (true);
}

/**
 * @brief Would start the periodic timer.
 * @note The @c add_repeating_timer_ms call is commented out, so no timer
 *       interrupt is installed. Frame pacing comes from the console instead.
 */
void init_timer_callback() {
	static repeating_timer_t timer;
	/* インターバルタイマ設定 */
//	add_repeating_timer_ms(TIME_INTERVAL, &timer_callback, NULL, &timer);
}





/**
 * @brief Core 0 startup.
 * @details Opens the serial console (with a 1800 ms settle so a terminal can
 *          attach), brings up the cartridge hardware, reports whether the last
 *          reset came from the watchdog, enables the bus interrupt, and starts
 *          the application.
 */
void setup() {

	Serial.begin(115200);
	sleep_ms(1800);

	sys.init();

	if (watchdog_caused_reboot()){
		Serial.printf("Rebooted by Watchdog!\n");
	}

	//	ap.init();

	enable_pico_ir();
	init_timer_callback();  // タイマー割り込み開始


	sys.frame_draw = 1;
	ap.init();
}

/**
 * @brief Core 0 main loop: renders exactly one frame per console frame.
 * @details Gated on rp_system::frame_draw, which the bus interrupt clears when the
 *          console sends its controller byte. Runs the application, then
 *          rp_system::update() to flush palette and attribute changes and convert
 *          the canvas into the PPU byte stream.
 * @note The sleep only bounds how quickly a new frame is noticed; it does not pace
 *       rendering. @see @ref architecture
 */
void loop() {
	if( sys.frame_draw == 0 ) {
		WDT_update();
		TRACE(DTR_ROOT)
		ap.main();
		TRACE(DTR_ROOT)
		sys.frame_draw++;
		sys.update();
		TRACE(DTR_ROOT)
		TRACE_END(DTR_ROOT)
	}
	sleep_ms(LOOP_MS);
}
