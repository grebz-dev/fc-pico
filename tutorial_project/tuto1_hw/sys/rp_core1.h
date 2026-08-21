/**
 * @file rp_core1.h
 * @brief Core 1: audio, and the watchdog handover.
 * @ingroup platform
 *
 * Core 1 is a message pump on the inter-core FIFO. Its real work is one call to
 * rp_sound::jobSound() per frame, triggered by #C1_SNDJOB; when idle it decodes
 * streamed MP3 audio and covers the watchdog on core 0's behalf.
 *
 * @see @ref architecture, @ref audio_page
 */


/**
 * @brief Core 1 startup; also re-run on #C1_RESET after a soft reset.
 */
void setup1() {
	snd.init();
}


/**
 * @brief Core 1 main loop: drains the inter-core FIFO, or decodes MP3 when idle.
 * @details #C1_SND_MP3PLAY is a tagged command carrying a track index in its low
 *          byte, so it is matched on its high byte before the plain values are
 *          compared. #C1_SNDJOB advances the sound driver by one frame and then
 *          packs the resulting APU writes into the outgoing mailbox.
 * @note WDT_check() runs first so that a stalled core 0 is detected even when no
 *       messages are arriving. @see setWDT_mode
 */
void loop1() {
	WDT_check();


	//--------------------------------------
	if (rp2040.fifo.available() == 0){
		snd.jobMP3();
		sleep_ms(LOOP_MS);
		return;
	}
	uint32_t buffer_Number = rp2040.fifo.pop();
	uint32_t com_sel = 0xFF000000 & buffer_Number;
	if ( com_sel == C1_SND_MP3PLAY ) {
		setMP3data( buffer_Number & 0xff );
	} else {
		switch ( buffer_Number ) {
		case C1_RESET:
			setup1();
			break;
		case C1_SNDJOB:
			snd.jobSound();
			sys.setPF_APU();
			break;

		}
	}

}

