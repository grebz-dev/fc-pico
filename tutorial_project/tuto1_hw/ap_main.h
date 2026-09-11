/*
    ap_main.h
 */

/**
 * @file ap_main.h
 * @brief Application entry point, scene state machine and the app-wide identifier enums.
 * @ingroup app
 *
 * The sample application is deliberately small; most of what is here is the
 * vocabulary the rest of the firmware shares -- inter-core message codes, scene
 * codes, save-data slots and the sound identifier space.
 *
 * @note The sound list is inherited from the author's full shooter game. Only
 *       `SE_CUR_SEL` and `BGM_BOSS`..`BGM_OVER` are used by this tutorial; the
 *       rest are retained because they are NSD.Lib song-table indices and
 *       renumbering them would break the mapping. @see @ref audio_page
 */

#ifndef ap_main_h
#define ap_main_h

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"

#include "ap_data.h"
#include "ap_title.h"


// プロセス間通信用
/**
 * @brief Messages sent from core 0 to core 1 over the hardware FIFO.
 * @ingroup app
 * @note #C1_SND_MP3PLAY is a *tagged* command: `loop1()` masks with `0xFF000000`,
 *       so its low byte carries an MP3 index. The others are plain values.
 */
enum {
	C1_RESET,      ///< Re-run core 1 initialisation after a soft reset.
	C1_SNDJOB,     ///< Advance the sound driver by one frame; posted by rp_system::ppu_dma().

	C1_SND_MP3PLAY = 0x10000000,   ///< Start MP3 playback; low byte is the track index.
	

};


/**
 * @brief Scene codes for ap_main::m_step.
 * @ingroup app
 * @details Each scene also has a sub-step: a sub-step of 0 means "run the scene's
 *          init once", which is the idiom used throughout the application layer.
 */
enum {
	ST_INIT = 0,   ///< Startup; immediately advances to #ST_TITLE.
	ST_TITLE,      ///< Title screen. @see ap_title

	ST_WAIT = 255, ///< Idle; runs nothing until the step is changed externally.
};


// セーブデータ
/**
 * @brief Indices into rp_system::SaveData, the 256-byte EEPROM-backed save area.
 * @ingroup app
 * @note A #SDT_HEAD of `0xFF` means the EEPROM has never been written, which is
 *       how ap_main::init() detects a first run.
 */
enum {
	SDT_HEAD = 0,      ///< Format marker; `0xFF` means uninitialised.
	SDT_MP3_ENA,       ///< MP3 playback enabled.
	SDT_MP3_VOL,       ///< MP3 volume, 0..40.
	SDT_GAME_MODE,     ///< Selected game mode.

};


enum {
	BGM_STOP = 0,
	BGM_BOSS,
	BGM_MAIN,
	BGM_CLEAR,
	BGM_OVER,

	SE_CUR_SEL,		// 00 カーソル 移動
	SE_CUR_ENT,		// 01 カーソル 決定
	SE_CUR_CAN,		// 02 カーソル キャンセル　(オプション 使用)
	SE_SHOT_A,		// 03 自機ショット音
	SE_PLY_DAME,	// 04 自機ダメージ音
	SE_PLY_DEAD,	// 05 自機死亡
	SE_PLY_FORM,	// 06 自機フォーメーションチェンジ

	SE_BAKU_S,		// 07 敵 撃破 敵サイズ小 ザコ
	SE_BAKU_M,		// 08 敵 撃破 敵サイズ中 ザコ
	SE_BAKU_L,		// 09 敵 撃破 敵サイズ大 ボス

	SE_NO_DAME,		// 10 敵無敵音
	SE_DAME,		// 11 ダメージ受け音

	SE_BOSS_MOVE1,	// 12 ボス移動1
	SE_BOSS_MOVE2,	// 13 ボス移動2

	SE_BOSS_ATK1,	// 14 ボス攻撃
	SE_TITLE,		// 15 タイトルＳＥ
	SE_START_JET,	// 16 スタートジェット
	SE_HADOU_CHG,	// 17 波動砲　チャージ
	SE_HADOU_SHT,	// 18 波動砲　発射
	SE_DM_DIVE,		// 19 次元潜航
	SE_YAMATO_S,	// 20 ヤマト発進

	SND_SEL_MAX,
};



/**
 * @brief The application: a two-level scene state machine.
 * @ingroup app
 *
 * A single global instance, #ap. main() is called exactly once per console frame
 * from core 0's `loop()`, gated on rp_system::frame_draw.
 */
class ap_main {

public:

    /// @brief Constructs the application. Real setup happens in init().
    ap_main() {};

    /**
     * @brief One-time application setup.
     * @details Initialises the save data on first run (detected by a #SDT_HEAD of
     *          `0xFF`), copies the audio preferences into rp_sound, and enters
     *          scene #ST_INIT.
     */
    void init();

    /**
     * @brief Runs one frame of the application.
     * @details Refreshes key state, dispatches on #m_step, and advances #m_timer.
     *          A sub-step of 0 means the scene's init has not run yet.
     */
    void main();

	/**
	 * @brief Switches to a scene and resets its sub-step.
	 * @param step One of the `ST_*` codes.
	 */
	void setStep( uint8_t step );

	/**
	 * @brief Sets the sub-step within the current scene.
	 * @param step_sub New sub-step; 0 means "run the scene's init again".
	 */
	void setStepSub( uint8_t step_sub );

	/// @brief Current scene.  @return One of the `ST_*` codes.
	uint8_t getStep() { return m_step; }
	/// @brief Current sub-step within the scene.  @return The sub-step.
	uint8_t getStepSub() { return m_step_sub; }


	uint16_t m_timer;   ///< Frames elapsed since startup; wraps freely.
	uint8_t m_DemoFG;   ///< Non-zero while the attract/demo mode is running.

private:
	uint8_t m_step;              ///< Current scene; one of the `ST_*` codes.
	uint8_t m_step_sub;          ///< Sub-step within the scene; 0 triggers scene init.
	uint16_t m_StartStarObjNo;   ///< First object index used by the starfield effect.
	uint16_t m_StarObjNum;       ///< Number of starfield objects.
	float m_StarSpd;             ///< Starfield scroll speed.
	unsigned long ap_main_time;  ///< Timestamp of the last frame, from `micros()`; used by the FPS overlay.



};

/// @brief The one and only application instance.
/// @ingroup app
extern ap_main ap;

#endif

