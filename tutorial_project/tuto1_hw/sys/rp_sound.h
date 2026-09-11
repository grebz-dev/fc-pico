/*
 */

/**
 * @file rp_sound.h
 * @brief Audio front end: request queues for music and effects, plus MP3 playback.
 * @ingroup audio
 *
 * Everything here runs on **core 1**. The play/stop calls are non-blocking
 * request setters, so core 0's application code may call them freely; the actual
 * work happens when jobSound() runs, once per frame, in response to the
 * `C1_SNDJOB` message.
 *
 * The music channels are not synthesised here. jobSound() ticks an emulated 6502
 * running the NSD.Lib driver (rp_nsfplayer), and the register writes that driver
 * makes are relayed to the console's real APU. @see @ref audio_page
 */

#pragma once


#include "rp_sound.h"

enum {
	MP3_BUFSIZE = 512,   ///< Decode buffer size for streamed MP3 playback, in bytes.

};


/**
 * @brief Owns the sound driver and the streamed-audio path.
 * @ingroup audio
 *
 * A single global instance, #snd, lives on core 1.
 */
class rp_sound {

public:
	/// @brief Initialises the NSF player, binds the `NSF_SOUND` resource and starts the PWM audio device.
	void init();

	/**
	 * @brief Services one frame of audio work.
	 * @details Applies any pending music and effect requests, then advances the
	 *          NSD.Lib driver by one tick. Called from core 1 on `C1_SNDJOB`,
	 *          which rp_system::ppu_dma() posts once per frame.
	 */
	void jobSound();

	/**
	 * @brief Requests music playback.
	 * @param bgmno Song index, e.g. `BGM_BOSS`; these are NSD.Lib song-table indices.
	 * @note Non-blocking: records the request and returns.
	 */
	void playBGM( uint8_t bgmno );

	/// @brief Requests that music stop.
	void stopBGM();

	/**
	 * @brief Requests a sound effect.
	 * @param seno Effect index, e.g. `SE_CUR_SEL`.
	 * @note Up to 8 effect requests can be outstanding between frames.
	 */
	void playSE( uint8_t seno );

	/// @brief Requests that all sound effects stop.
	void stopSE();

	/**
	 * @brief Mirrors an APU register value into the local shadow.
	 * @param addr Register address in `$4000`-`$4017`.
	 * @param value Value written.
	 * @note Currently unused. The live path is rp_system::setAPU(), called from the
	 *       emulated 6502 in rp_fcemu.
	 */
	void setReg( uint16_t addr, uint8_t value );

	/**
	 * @brief Starts streamed MP3 playback on the PWM audio output.
	 * @param mp3data Pointer to the MP3 data in flash.
	 * @param size Length in bytes.
	 * @param loop Restart from the beginning on completion.
	 */
	void setMP3( const uint8_t* mp3data, int size, bool loop );

	/// @brief Stops streamed MP3 playback.
	void stopMP3();

	/**
	 * @brief Advances streamed MP3 decoding.
	 * @details Core 1's idle work: called whenever the inter-core FIFO is empty.
	 */
	void jobMP3();

	uint8_t   m_MP3_ENA;   ///< MP3 playback enabled; loaded from `SaveData[SDT_MP3_ENA]`.
	uint8_t   m_MP3_VOL;   ///< MP3 volume 0..40; loaded from `SaveData[SDT_MP3_VOL]`.


private:
	uint8_t   m_APU[ 0x18 ];   ///< Shadow of the APU registers, maintained by setReg().
	uint8_t   m_BgmRQ;		///< Pending music request; 255 means stop. // BGMリクエスト用
	uint8_t   m_SeRQ[8];	///< Pending effect requests; 255 means stop. // SEリクエスト用

	const uint8_t*  m_pMP3data;	///< Current MP3 data. // MP3データアドレス
	int m_MP3_idx;		///< Read cursor into #m_pMP3data. // MP3データ インデックス
	int m_MP3_size;		///< Length of #m_pMP3data in bytes. // MP3データ サイズ
	bool m_bMP3loop;	///< Restart at the end. // MP3ループ再生
	int m_loopWait;		///< Ticks remaining before a loop restart. // ループ再生時のウェイト


};

/// @brief The one and only sound instance; lives on core 1.
/// @ingroup audio
extern rp_sound snd;
