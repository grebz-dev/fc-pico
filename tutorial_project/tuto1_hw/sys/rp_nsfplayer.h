/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file rp_nsfplayer.h
 * @brief NSF player specialised for the NSD.Lib sound driver.
 * @ingroup audio
 *
 * Rather than reimplementing NSD.Lib, this class **runs the driver's own 6502
 * code** on the interpreter in rp_fcemu. Each public call is a subroutine entry
 * into the driver, invoked with the argument convention NSD.Lib expects.
 *
 * @warning The entry-point addresses below are those of the specific `nsd.bin`
 *          shipped in `mml/bin/`. Upgrading NSD.Lib moves them and every constant
 *          in this file must be re-derived.
 *
 * @note Licensing: this file and rp_fcemu are derived from FabGL and are
 *       **GPL v3**. @see @ref references
 * @see @ref audio_page
 */

#pragma once


#include "rp_fcemu.h"

/// @brief Magic value at the start of an NSF file: the ASCII bytes `NESM`.
#define NSF_NESM	0x4D53454E	// ヘッダーチェック用

/**
 * @brief The 128-byte NSF file header.
 * @ingroup audio
 * @details Layout per the NSF specification; see @ref references. Only
 *          #load_address, #init_address and #play_address are consulted here,
 *          because NSD.Lib's own entry points are known statically.
 */
typedef struct {
	uint32_t NESM;				///< Magic, #NSF_NESM. // denotes an NES sound format file 0x4D53454E
	uint8_t NESM_E;				///< Terminator byte, `$1A`. // $1A
	uint8_t Version_number;		///< NSF version. // $01 (or $02 for NSF2)
	uint8_t Total_songs;		///< Number of songs in the file. // (1=1 song, 2=2 songs, etc)
	uint8_t Starting_song;		///< Default song, 1-based. // (1=1st song, 2=2nd song, etc)
	uint16_t load_address;		///< Where the data is mapped, `$8000`-`$FFFF`. // (lo, hi) ($8000-FFFF)
	uint16_t init_address;		///< Song-select entry point. // (lo, hi) ($8000-FFFF)
	uint16_t play_address;		///< Per-frame entry point. // (lo, hi) ($8000-FFFF)
	uint8_t  str_song[32];		///< Song title, NUL-terminated. // The name of the song, null terminated
	uint8_t  str_artist[32];	///< Artist, NUL-terminated. // The artist, if known, null terminated
	uint8_t  str_copyright[32];	///< Copyright holder, NUL-terminated. // The copyright holder, null terminated
	uint16_t Play_speed_N;		///< NTSC tick period in microseconds. // (lo, hi) Play speed, in 1/1000000th sec ticks, NTSC (see text)
	uint8_t  Bankswitch[8];		///< Initial bank values; unused by this player. // Bankswitch init values
	uint16_t Play_speed_P;		///< PAL tick period in microseconds. // (lo, hi) Play speed, in 1/1000000th sec ticks, PAL (see text)
	uint8_t PAL_NTSC_bits;		///< Region flags; bit 0 selects PAL, bit 1 marks dual. // PAL/NTSC bits
//                bit 0: if clear, this is an NTSC tune
//                bit 0: if set, this is a PAL tune
//                bit 1: if set, this is a dual PAL/NTSC tune
//                bits 2-7: reserved, must be 0
	uint8_t Extra_Sound;		///< Expansion-chip flags; none are emulated here. // Extra Sound Chip Support
//                bit 0: if set, this song uses VRC6 audio
//                bit 1: if set, this song uses VRC7 audio
//                bit 2: if set, this song uses FDS audio
//                bit 3: if set, this song uses MMC5 audio
//                bit 4: if set, this song uses Namco 163 audio
//                bit 5: if set, this song uses Sunsoft 5B audio
//                bit 6: if set, this song uses VT02+ audio
//                bit 7: reserved, must be zero
	uint8_t RsvNSF2[4];		///< Reserved for NSF2. // Reserved for NSF2t

} NSF_HEADER;

/**
 * @brief Fixed addresses inside the NSD.Lib driver image.
 * @ingroup audio
 * @warning Tied to the exact `nsd.bin` in `mml/bin/`. @see rp_nsfplayer
 */
enum {
	__ptr = 0x05,	///< Driver scratch pointer, 2 bytes. // 汎用ポインタ 2byte
/*
	__tmp = 0x07,
*/

	/// @brief Driver status/control bitfield.
	/// @details `D... ....` playback control disabled; `...J ....` fast-forward;
	///          `.... PP..` effect priority; `.... ..S.` effect playing;
	///          `.... ...B` music playing.
	__flag = 0x0A,
/*
	lda	#nsd_flag::BGM + nsd_flag::SE
	sta	__flag		;BGM, SE処理を禁止（RAM未初期化対策）

	__flag
		D... .... : 再生制御無効
		...J .... : 早送り中
		.... PP.. : 効果音の優先度
		.... ..S. : ＳＥ再生中
		.... ...B : ＢＧＭが再生中
*/

	_eff  = 0x200,	///< First index in the song table that denotes a sound effect. // 効果音テーブル開始番号
	_play = 0x201,	///< Frame-overrun guard; the driver clears it to skip a tick. // =0 フレームオーバー防止用変数

/*
	ax = Pointer 		; x = Hadr / a = Ladr
*/

	_nsf_init = 0x8010,		///< NSF init entry point. // NSF init address
	_nmi_main = 0x8084,		///< NSF play entry point, called once per frame. // NSF play address
	_nsd_init = 0x80A1,		///< NSD.Lib driver initialisation.
	_nsd_set_dpcm = 0x80AB,	///< Install DPCM table; A/X = pointer. // ax = Pointer of ⊿PCM infomation Struct
	_nsd_main = 0x80B2,		///< NSD.Lib per-frame worker.
	_nsd_play_bgm = 0x8137,	///< Start music; A/X = song data pointer. // ax = Pointer of BGM
	_nsd_stop_bgm = 0x8219,	///< Stop music.
	_nsd_play_se = 0x8239,	///< Start sound effect; A/X = effect data pointer. // ax = Pointer of SE
	_nsd_stop_se = 0x82B7,	///< Stop sound effects.
	_nsd_snd_init = 0x8AB6,	///< Reset the APU shadow registers.

	_nsd_table_idx = 0x8F6E,	///< Base of the song pointer table. @see rp_nsfplayer::nsd_data_addr //テーブルインデックス

/*
+$0000	B 効果音テーブル開始番号？
+$0002	W DPCM情報テーブルアドレス
+$0004～ 2バイト単位でBGM,SEのデータアドレス
*/

};


/**
 * @brief Drives the NSD.Lib sound driver on the emulated 6502.
 * @ingroup audio
 *
 * A single global instance, #nsf, owned by rp_sound on core 1.
 */
class rp_nsfplayer : public rp_fcemu {

public:
	/**
	 * @brief Validates an NSF image and maps it into the emulated address space.
	 * @param nsf Pointer to the NSF file, header included.
	 * @note Checks the #NSF_NESM magic, then offsets past the header by
	 *       `load_address - 0x8000` so the data lands where the driver expects.
	 */
	void setNSF( const uint8_t *nsf );

	/**
	 * @brief Maps an NSF image without validating the header.
	 * @param nsf Pointer to the NSF file.
	 */
	void setNSF_NSDLIB( const uint8_t *nsf );

	/**
	 * @brief Calls the NSF init entry point to select a song.
	 * @param song_no Song index.
	 */
	void play( uint8_t song_no );

	/**
	 * @brief Starts music.
	 * @param song_no Index into the table at #_nsd_table_idx.
	 */
	void playBGM( uint8_t song_no );

	/**
	 * @brief Starts a sound effect.
	 * @param song_no Index into the table at #_nsd_table_idx.
	 */
	void playSE( uint8_t song_no );

	/// @brief Stops music.
	void stopBGM();

	/// @brief Stops sound effects.
	void stopSE();

	/**
	 * @brief Advances the driver by one frame.
	 * @details Runs the emulated 6502 from #_nmi_main until it returns. Every APU
	 *          register write it performs is trapped by rp_fcemu and forwarded to
	 *          rp_system::setAPU().
	 */
	void main( );
private:
	NSF_HEADER *nsf_head;   ///< The mapped file's header.

	/**
	 * @brief Looks up a song's data pointer.
	 * @param song_no Song index.
	 * @return The little-endian pointer stored at `_nsd_table_idx + 2 + song_no*2`.
	 * @note This is why the `BGM_*` and `SE_*` values in ap_main.h share one
	 *       numbering space: they are both indices into this table.
	 */
	uint16_t nsd_data_addr(  uint8_t song_no );

};


/// @brief The one and only NSF player instance.
/// @ingroup audio
extern rp_nsfplayer nsf;


