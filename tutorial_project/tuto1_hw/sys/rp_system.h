/*
    rp_system.h
 */

/**
 * @file rp_system.h
 * @brief Cartridge bus interface: PIO setup, frame streaming and the FC<->PICO protocol.
 * @ingroup fcbus
 *
 * This is the boundary between the RP2350 and the Famicom.  The cartridge does
 * not appear on the CPU bus at all -- it replaces the CHR-ROM on the **PPU
 * bus** and answers the PPU's pattern fetches with a pre-rendered byte stream.
 * Because the PPU fetches in a fixed order every frame, the address lines can
 * be ignored entirely; only @c /RD, @c /WR and the chip select matter.  See
 * @ref nes_doom for why this works.
 *
 * Three things ride on that one wire:
 *  - the framebuffer, as ~15426 bitplane bytes pulled out by the PPU itself;
 *  - a 64-byte mailbox appended to the tail of every frame, carrying commands
 *    and APU register writes from the RP2350 to the 6502;
 *  - single bytes written by the 6502 to `$2007`, captured by PIO and dispatched
 *    in interrupt context by rp_system::jobRcvCom().
 *
 * @see @ref protocol for the full wire format.
 * @see rp_dma, fcppu.pio
 */

#ifndef rp_system_h
#define rp_system_h

#include "Arduino.h"

#include "pico/stdlib.h"
#include "pico/unique_id.h" // 固有ID用ヘッダー
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include <hardware/watchdog.h>
#include <EEPROM.h>

#include "ArduinoGL.h"
#include "Canvas.h"
#include "rp_dma.h"

#include "pio/fcppu.pio.h"

#include "rp_debug.h"

#include "rp_fcemu.h"
#include "rp_nsfplayer.h"
#include "rp_sound.h"
#include "rp_bpe.h"

#include "Obj3d.h"

#include "Obj3d.h"



/// @brief Idle sleep between core-0 loop iterations, in milliseconds.
/// @note The loop is gated on rp_system::frame_draw, so this only bounds how
///       quickly core 0 notices that the console has started a new frame.
#define LOOP_MS 1

/// @brief Repeating-timer period in microseconds (negative = fixed rate, not fixed delay).
/// @warning Currently unused: init_timer_callback() leaves @c add_repeating_timer_ms
///          commented out, so no timer interrupt is installed.
#define TIME_INTERVAL -500  // タイマー割り込みの間隔


//---------------------------------------
// GPIO
//---------------------------------------

/// @name Board GPIO assignments
/// @ingroup fcbus
/// Pin numbers are Pico GP numbers. The data bus and strobes are declared by
/// fcppu.pio instead, so that the PIO programs and the C++ agree by construction.
/// @{

#define LED_COUNT 1           ///< Number of chained status LEDs.
#define BRIGHTNESS 32        ///< LED brightness, 0-255.

#define	LED_PIN 25           ///< Status LED (blinks to show the firmware is alive).
#define	LED_PIN2 24          ///< Secondary LED, shares the pin with #USRKEY_PIN.
#define	USRKEY_PIN 24        ///< On-cartridge user button.
//#define DIN_PIN 23            // NeoPixel　の出力ピン番号はGP23

/// @}

/// @name PPU bus signal masks
/// @ingroup fcbus
/// GPIO bit masks derived from the `PI_*_BIT` pin numbers exported by fcppu.pio.
/// @{

#define	PI_WR		(1<<PI_WR_BIT)   ///< PPU write strobe, active low (GP21).
#define	PI_RD		(1<<PI_RD_BIT)   ///< PPU read strobe, active low (GP20).
//#define	PI_PA13		(1<<PI_PA13_BIT)
//#define	PI_PA12		(1<<PI_PA12_BIT)
#define	PI_CS1		(1<<PI_CS1_BIT)  ///< PPU chip select for CHR space (GP17); gates every PIO program.
//#define	PI_OD_DIR	(1<<PI_OD_DIR_BIT)
#define	PI_DATA_SHIFT	PI_D0_BIT            ///< GPIO position of D0; the bus is D0..D7 on GP6..GP13.
#define	PI_DATA_MASK	(0x00FF<<PI_DATA_SHIFT)  ///< Mask covering the whole 8-bit data bus.

//#define	PI_INP_PINS		(PI_WR|PI_RD|PI_PA13|PI_PA12|PI_CS1|PI_OD_DIR)
#define	PI_INP_PINS		(PI_WR|PI_RD|PI_CS1)   ///< Pins driven by the console, always inputs.
#define	PI_BIDIR_PINS	(PI_DATA_MASK)         ///< Pins turned around per access by the #SM_BUSDIR program.

/// @}


//---------------------------------------
// ステートマシン関連
//---------------------------------------

/// @name PIO state machine allocation
/// @ingroup fcbus
/// All four state machines of PIO0 are used; the programs live in fcppu.pio.
/// @{

#define PIO_NO_0	0    ///< PIO block 0 -- the only one used for the bus.
#define PIO_NO_1	1    ///< PIO block 1 (unused).
#define PIO_NO_2	2    ///< PIO block 2 (RP2350 only; unused).

/// @brief Captures bytes the 6502 writes to `$2007`.
/// @details Runs @c fcppu_w. A non-empty RX FIFO raises @c PIO0_IRQ_0, which
///          lands in pio0_itr0() and then rp_system::jobRcvCom().
#define	SM_RECV	0
/// @brief Drives the outgoing byte stream onto the data bus on every PPU read.
/// @details Runs @c fcppu_r, fed by the rp_system::vram_dma channel.
#define	SM_TRAN	1
/// @brief Turns the data bus around so it is only driven during a read.
/// @details Runs @c fcppu_dir, handshaking with #SM_TRAN via a PIO IRQ flag.
#define	SM_BUSDIR	2
/// @brief Counts qualifying PPU reads so the frame stream can be resynchronised.
/// @details Runs @c fcppu_rna; the count is compared against #PPU_COUNT_VAL in
///          rp_system::ppu_dma().
#define	SM_TRCNT	3

/// @}


//---------------------------------------
// PICO->FC command
//---------------------------------------

/// @addtogroup fcbus
/// @{

/// @name PICO -> Famicom commands
/// Opcodes placed in the per-frame mailbox (bytes 2..15) and executed by
/// @c jobPICO in `BOOTROM/SysPico.asm`. The list is terminated by #PF_COM_NONE.
///
/// @warning These values are duplicated in `BOOTROM/SysPico.asm` as
///          `PF_COM_*`. Changing one side without the other silently corrupts
///          the link -- there is no version negotiation.
/// @{

#define  PF_COM_NONE  0		///< End of the command list; no operation. // コマンドなし
#define  PF_COM_DMOD  1		///< Blank the display and enter bulk data-transfer mode. @see rp_system::startDataMode
#define  PF_COM_FDIN  2		///< Start a palette fade-in on the 6502 side.
#define  PF_COM_FDOT  3		///< Start a palette fade-out on the 6502 side.

#define  PF_COM_SE    0x80		///< Play sound effect: `0x80 | SE_NO`.
#define  PF_COM_BGM   0xA0		///< Play background music: `0xA0 | BGM_NO`.
#define  PF_COM_VRAM  0xC0		///< Poke one VRAM byte: `0xC0|adrH`, `adrL`, `data`. @see rp_system::setPF_VRAM


	// データモードコマンド

/// @brief Bulk VRAM write: `adrH`, `adrL`, `size`, `data...`.
/// @note A @c size of 0 means 256 bytes -- the maximum for one block. Larger
///       regions must be split across several commands.
#define  PF_DAT_VRAM  0x80
/// @brief Bulk CPU-RAM write, same payload layout as #PF_DAT_VRAM.
#define  PF_DAT_RAM   0x81

/// @brief Leave data mode and jump the 6502 to the step code in the payload.
#define  PF_DAT_STEP  0x82

/// @brief Sentinel written into `FC_COM_BUF[1]` so the 6502 can reject a torn mailbox.
/// @details The 6502 checks this before acting on any command; a mismatch means
///          the frame stream lost synchronisation and the buffer is discarded.
///          Known as `PF_MAGIC_CODE` on the assembly side.
#define  PF_MAGIC_NO  0xFC

/// @}

//---------------------------------------
// FC->PICO command
//---------------------------------------

/**
 * @brief Famicom -> PICO commands.
 *
 * Single bytes written by the 6502 to `$2007` with the PPU address parked in
 * pattern space. rp_system::jobRcvCom() dispatches them in interrupt context.
 *
 * @warning Any value *not* listed here is treated as a controller state byte --
 *          that is the hot path, sent once per frame at the end of the NMI
 *          handler, and it is what drives the whole render/transfer cycle.
 */
enum{
//	FP_COM_ACK	= 0x0F,		// FCからのコマンド正常終了応答
//	FP_COM_NAK	= 0x1F,		// FCからのコマンド失敗終了応答
	FP_COM_VER	= 0x2F,		///< Request the boot-ROM build stamp. @see rp_system::ver_dma
	FP_COM_ROM	= 0x3F,		///< Request a 256-byte page of the boot ROM image; next byte is the page. @see rp_system::rom_dma

	FP_COM_LOG	= 0xBF,		///< Debug log: up to 7 following bytes are printed to the serial console.
	FP_COM_DRQ	= 0xCF,		///< Data request, issued by the 6502 while in data mode. @see rp_system::jobFP_COM_DRQ
	FP_COM_DLD	= 0xDF,		///< Data load; next byte selects the 256-byte page to send.
	FP_COM_RST	= 0xEF,		///< Restart the RP2350 firmware (full rp_system::init()).
	FP_COM_INI	= 0xFF,		///< Initialise; next byte is the stage number to start on.
};



//---------------------------------------
// FCキー入力定義
//---------------------------------------

/// @name Controller bits
/// Bit layout of the controller byte the 6502 sends once per frame. Matches the
/// `KEY_*` equates in `BOOTROM/SysEqu.h`.
/// @{
#define KEY_A		0x80   ///< A button.
#define KEY_B		0x40   ///< B button.
#define KEY_SEL		0x20   ///< Select.
#define KEY_RUN		0x10   ///< Start.
#define KEY_UP		0x08   ///< D-pad up.
#define KEY_DOWN	0x04   ///< D-pad down.
#define KEY_LEFT	0x02   ///< D-pad left.
#define KEY_RIGHT	0x01   ///< D-pad right.
/// @}


//---------------------------------------
// キーリピート設定
//---------------------------------------
/// @brief Frames a direction must be held before auto-repeat begins.
#define REP_WAIT	24		// リピート開始までの時間 (フレーム数)
/// @brief Frames between successive auto-repeat events once repeating.
#define REP_INTERVAL 8		// リピート間隔 (フレーム数)


//---------------------------------------
// フェードイン、アウト
//---------------------------------------
/// @brief Frames to wait for a fade issued to the 6502 to complete.
#define FADE_WAIT	8


//---------------------------------------
// FC STEP
//---------------------------------------
/// @brief 6502 step code for the options screen, used with rp_system::setFcStep().
#define FCST_OPTION  3

//---------------------------------------
//---------------------------------------
#define MICROS_1S  (1000*1000)   ///< One second, in microseconds.
#define MICROS_1MS  (1000)       ///< One millisecond, in microseconds.


//#define FC_COM_BUF_SIZE	8
//#define FC_COM_BUF_SIZE	16
/// @brief Size of the command area the 6502 actually parses.
/// @details `jobPICO` scans mailbox bytes 2..15 only; bytes 16..63 are the APU
///          register area. rp_system::setPF_COM() refuses to write past this.
#define FC_COM_BUF_SIZE16	16
/// @brief Total mailbox size appended to the tail of every frame stream.
#define FC_COM_BUF_SIZE	64

/// @brief Depth of the APU register ring buffer filled by rp_system::setAPU().
#define PICO_APU_BUF_SIZE  0x40

/// @brief Offset within the mailbox where APU (register, value) pairs begin.
/// @details Mirrors `PICO_SNDREG EQU $30` in `BOOTROM/SysEqu.h`: the mailbox
///          lands at zero page `$20`, so offset `0x10` is address `$30`.
#define PICO_SNDREG		0x10

/// @brief Expected number of PPU bus reads per frame, including the mailbox.
/// @details 15426 pattern fetches plus the #FC_COM_BUF_SIZE trailer.
///          rp_system::ppu_dma() compares the #SM_TRCNT counter against this to
///          decide whether the stream is still in phase.
/// @warning If the measured count drifts more than +/-2 from this value the DMA
///          is stopped rather than restarted: the console has lost sync and
///          pushing more data would tear the display.
#define PPU_COUNT_VAL	(15426 + FC_COM_BUF_SIZE)

/// @brief Size of the EEPROM-backed save area, in bytes.
#define SAVE_DATA_SIZE	256

/// @} end of fcbus group opened above the protocol constants



//=================================================
//			仮想VRAM関連
//=================================================

/// @brief Size of one virtual-VRAM buffer, in 32-bit words.
/// @details 36 tile columns x 2 bitplanes x 240 scanlines, plus the mailbox
///          trailer. Only 34 columns are emitted by rp_system::convVram()
///          (32 visible + 2 that the PPU prefetches for scrolling); the extra
///          two columns are slack so the DMA never runs off the end.
#define VRAM_BUF_SIZE ((36 * 2 * 240 + FC_COM_BUF_SIZE) / sizeof(uint32_t))
//#define VRAM_BUF_SIZE ((32 * 2 * 240) / sizeof(uint32_t))


/**
 * @brief Owns the Famicom link: PIO bring-up, frame streaming and protocol state.
 * @ingroup fcbus
 *
 * A single global instance, #sys, is shared by both cores. Core 0 calls
 * update() once per frame and services the bus interrupt; core 1 only touches
 * setAPU() and setPF_APU().
 *
 * The object holds two virtual-VRAM buffers and flips between them: one is
 * being streamed to the PPU while the other is being filled by convVram().
 *
 * @warning Several members run in interrupt context (jobRcvCom() and everything
 *          it calls). Anything that blocks or prints there will overrun the
 *          6502's spin-loop handshake and desynchronise the frame stream.
 */
class rp_system {

public:
    /// @brief Constructs the system object. Hardware bring-up happens in init().
    rp_system();

    /**
     * @brief Full hardware bring-up: GPIO, PIO0, DMA, EEPROM and watchdog.
     * @details Configures all four state machines, primes the TX FIFO with the
     *          `"!#FC"` signature, loads the save data and calls init2().
     *          Also used as the soft-reset path for #FP_COM_RST.
     */
    void init(void);

    /**
     * @brief Resets the soft state without touching the hardware.
     * @details Clears the virtual VRAM, mailbox, palette/attribute shadows and
     *          key state, and pushes #C1_RESET so core 1 re-initialises too.
     */
    void init2();

    /**
     * @brief Per-frame update: flush palette/attribute changes, then convert the frame.
     * @details Diffs the palette and attribute shadows against their previous
     *          contents and appends #PF_COM_VRAM pokes for whatever changed,
     *          then calls convVram() to build the next frame's byte stream.
     * @note If the mailbox fills, the remaining differences are deferred to the
     *       next frame rather than dropped.
     */
    void update(void);

	/// @brief Restarts the firmware in response to #FP_COM_RST.
	void soft_reset();

	/// @brief Clears both virtual-VRAM buffers and resets the draw/display pointers.
    void initVram();

    /**
     * @brief Transposes the canvas into the PPU's two-bitplane fetch order.
     * @details Each run of 8 canvas pixels becomes one 16-bit word whose low
     *          byte is bitplane 0 and whose high byte is bitplane 1 -- exactly
     *          the two bytes the PPU fetches per tile row. 34 columns x 240
     *          lines are emitted, then the draw and display buffers are swapped.
     * @note Only the low two bits of each canvas byte are colour; the upper bits
     *       carry depth sub-precision and are masked off here.
     *       @see Canvas::setZval
     */
    void convVram();

    /**
     * @brief Dispatches one byte received from the 6502.
     * @details Called from pio0_itr0() whenever the #SM_RECV FIFO is non-empty.
     *          Recognised opcodes are the #FP_COM_VER family; **any other value
     *          is a controller byte**, which latches the key state, kicks
     *          ppu_dma() and releases core 0 to render the next frame.
     * @warning Runs in interrupt context.
     */
	void jobRcvCom();

    /**
     * @brief Re-arms the frame DMA and appends the mailbox to the byte stream.
     * @details Reads the #SM_TRCNT read counter for the frame just finished. If
     *          it is within +/-2 of #PPU_COUNT_VAL the stream is still in phase,
     *          so the DMA is restarted and the residual drift is absorbed by
     *          manually stepping the transmit state machine; otherwise the DMA
     *          is stopped until the console resynchronises.
     * @warning Runs in interrupt context.
     */
    void ppu_dma(void);

    /**
     * @brief Answers #FP_COM_VER with the boot-ROM build stamp.
     * @details Streams the 16 bytes at `_rom[0x6FF0]` -- the string baked in by
     *          `BOOTROM/m.BAT` via `dbdate.h`. The 6502 compares it against its
     *          own copy and reflashes itself if they differ.
     * @see @ref boot_reflash
     */
    void ver_dma();

	/// @brief Requests a palette fade-in on the 6502 and waits for it to finish.
	void FadeIn();
	/// @brief Requests a palette fade-out on the 6502 and waits for it to finish.
	void FadeOut();

	/**
	 * @brief Sleeps while keeping the watchdog fed.
	 * @param ms Milliseconds to sleep.
	 */
	void SleepMS( int ms );


	/**
	 * @brief Latches the raw controller byte received from the console.
	 * @param key Bitmask of #KEY_A .. #KEY_RIGHT.
	 * @note Only stores the value; edge and repeat state are derived later by
	 *       setKeyUpdate(), which runs on the application thread.
	 */
	void setKeyData( uint8_t key ) { m_key_imp = key; }

	/// @brief Derives held/triggered/repeat key state from the latest raw byte.
	void setKeyUpdate();

	/**
	 * @brief Appends one command byte to the outgoing mailbox.
	 * @param com One of the #PF_COM_NONE family.
	 * @retval true  The mailbox is full and the command was dropped.
	 * @retval false The command was queued.
	 */
	bool setPF_COM( uint8_t com );

	/**
	 * @brief Queues a single-byte VRAM poke for the 6502 to perform.
	 * @param vadr PPU address; masked to 14 bits.
	 * @param dt   Byte to write.
	 * @retval true  The mailbox is full and the poke was dropped.
	 * @retval false The poke was queued.
	 */
	bool setPF_VRAM( uint16_t vadr, uint8_t dt );

	/**
	 * @brief Enters bulk data-transfer mode and blocks until the console agrees.
	 * @details Issues #PF_COM_DMOD repeatedly (up to 100 attempts, 50 ms apart)
	 *          until the 6502 answers with #FP_COM_DRQ. Rendering is off for the
	 *          duration, which is why this is a distinct explicit mode rather
	 *          than something the per-frame path can do.
	 */
    void startDataMode(void);

    /// @brief Currently-held keys.  @return Bitmask of #KEY_A .. #KEY_RIGHT.
    uint8_t  getKeyNew(void) { return m_key_new; }
    /// @brief Keys pressed this frame (rising edge).  @return Bitmask.
    uint8_t  getKeyTrg(void) { return m_key_trg; }
    /// @brief Keys reported by auto-repeat this frame.  @return Bitmask.
    uint8_t  getKeyRep(void) { return m_rep_new; }


	/**
	 * @brief Records an APU register write made by the emulated sound driver.
	 * @param reg  Register index relative to `$4000`.
	 * @param data Value written.
	 * @details Buffered in a ring and drained by setPF_APU() once per frame. The
	 *          RP2350 never synthesises audio for these channels -- the console's
	 *          own APU does, one frame later.
	 * @note Called from core 1.  @see rp_fcemu
	 */
	void setAPU( uint8_t reg, uint8_t data );

	/**
	 * @brief Packs the buffered APU writes into the mailbox as (reg, value) pairs.
	 * @details Written from offset #PICO_SNDREG and terminated with `0xFF`, which
	 *          is the terminator the 6502's NMI replay loop stops on.
	 * @note Called from core 1, immediately after the sound driver ticks.
	 */
	void setPF_APU();

	/// @brief Loads all 32 palette entries.  @param paldt 32 NES palette indices (16 BG, 16 OBJ).
	void setPalData( const uint8_t *paldt );
	/// @brief Sets one palette entry.  @param idx 0..31.  @param dt NES palette index.
	void setPal( uint8_t idx, uint8_t dt );
	/// @brief Loads all 64 attribute bytes.  @param atrdt 64-byte attribute table.
	void setAtrData( const uint8_t *atrdt );
	/// @brief Clears the attribute table to zero.
	void clearAtrData( void );

	/**
	 * @brief Sets the palette selector for one 2x2-tile attribute quadrant.
	 * @param lx Tile column.
	 * @param ly Tile row.
	 * @param dt Palette number, 0..3.
	 */
	void setAtr( uint8_t lx, uint8_t ly, uint8_t dt );

	/// @brief Queues a jump to a 6502 step code, delivered on leaving data mode.
	/// @param step Step number, e.g. #FCST_OPTION.
	void setFcStep( uint8_t step ) { m_FC_STEP = step; }

	/// @brief Writes #SaveData back to EEPROM.
	void commitSaveData();


	/// @brief PPU read count measured over the last frame.  @see PPU_COUNT_VAL
	uint32_t ppu_count;

	/**
	 * @brief Frame handshake flag: 0 means "the console wants a new frame".
	 * @details Cleared by jobRcvCom() when the controller byte arrives and set
	 *          again by the core-0 loop once the frame has been rendered.
	 * @note This is why the console's 60 Hz timing, not a local timer, paces the
	 *       whole firmware.
	 */
	uint8_t frame_draw;

	/// @brief EEPROM-backed save area, indexed by the `SDT_*` enum in ap_main.h.
	uint8_t SaveData[SAVE_DATA_SIZE];


private:
	/// @brief Resets the mailbox: zeroed, magic byte stamped, write index at 2.
	void initFC_COM_BUF();

	/**
	 * @brief Services one #FP_COM_DRQ while in bulk data mode.
	 * @details Answers with the next outstanding payload in priority order:
	 *          palette (`$3F00`, 32 bytes), then attributes (`$23C0`, 64 bytes),
	 *          then a pending step change, then #PF_COM_NONE to end the mode.
	 */
	void jobFP_COM_DRQ();

	/// @brief Streams one 256-byte page of the current data-mode payload.
	/// @param adrh Page number within the payload.
	void jobFP_COM_DLD( uint8_t adrh );

	/**
	 * @brief Streams one 256-byte page of the compiled-in boot ROM image.
	 * @param adrh Page number; masked to 7 bits.
	 * @details Backs the console's self-reflash path -- the 6502 pulls its own
	 *          replacement program image through the PPU bus one page at a time.
	 * @see @ref boot_reflash
	 */
    void rom_dma( uint8_t adrh );

	/**
	 * @brief Emits a data-mode response header into the transmit FIFO.
	 * @param com  Command byte (#PF_DAT_VRAM, #PF_DAT_STEP, ...).
	 * @param adr  Destination address on the console.
	 * @param size Payload length in bytes; 0 means 256.
	 */
	void drq_ret( uint8_t com, uint16_t adr, uint16_t size );

	/// @brief Blocking read of one byte from the #SM_RECV FIFO.  @return The byte.
	uint8_t getRcvCom();

	uint8_t m_waitFP_COM_DRQ;   ///< Non-zero while startDataMode() is waiting for the console to answer.

	uint8_t m_key_imp;          ///< Raw controller byte as received this frame.
	uint8_t m_key_new;          ///< Currently-held keys.
	uint8_t m_key_trg;          ///< Keys that went down this frame.
	uint8_t m_key_old;          ///< Previous frame's held keys, for edge detection.
	uint8_t m_rep_key;          ///< Direction currently being auto-repeated.
	uint8_t m_rep_cnt;          ///< Frames remaining until the next repeat event.
	uint8_t m_rep_new;          ///< Repeat events generated this frame.
	uint32_t ppu_count_old;     ///< Previous frame's PPU read count, for drift tracking.
	uint8_t m_fade_wait;        ///< Frames left to wait for a fade to complete.

	uint8_t *m_pDRQ;            ///< Source buffer for the in-flight data-mode transfer.

	uint8_t m_PAL_W[0x20];      ///< Shadow of the console's 32-entry palette.
	uint8_t m_PAL_W_old[0x20];  ///< Last palette actually sent, for diffing.
	uint8_t m_PAL_CHG;          ///< Set when #m_PAL_W differs from what the console holds.

	uint8_t m_ATR_W[0x40];      ///< Shadow of the 64-byte attribute table.
	uint8_t m_ATR_W_old[0x40];  ///< Last attribute table actually sent, for diffing.
	uint8_t m_ATR_CHG;          ///< Set when #m_ATR_W differs from what the console holds.

	uint8_t FC_COM_BUF[ FC_COM_BUF_SIZE ];  ///< The outgoing mailbox; copied to the tail of each frame.
	uint8_t m_FC_COM_IDX;                   ///< Write cursor into #FC_COM_BUF; starts at 2, past the magic byte.
	uint32_t vram_buf0[VRAM_BUF_SIZE];      ///< Virtual VRAM, buffer A.
	uint32_t vram_buf1[VRAM_BUF_SIZE];	    ///< Virtual VRAM, buffer B. // バックバッファ

	uint32_t *vram_buf;         ///< Buffer currently being streamed to the PPU.
	uint32_t *vram_bufDraw;     ///< Buffer currently being filled by convVram().

	uint8_t *vram;              ///< Byte-wise alias of #vram_bufDraw used during conversion.

	uint8_t m_FC_STEP;          ///< Pending 6502 step code.  @see setFcStep

	uint8_t m_APU[ 0x18 ];      ///< Mirror of the console's APU registers.

	uint8_t m_APU_REG[PICO_APU_BUF_SIZE];   ///< Ring buffer of APU register indices.
	uint8_t m_APU_DAT[PICO_APU_BUF_SIZE];   ///< Ring buffer of APU register values.
	uint8_t m_APU_W_IDX;                    ///< APU ring write cursor (core 1).
	uint8_t m_APU_R_IDX;                    ///< APU ring read cursor (drained by setPF_APU()).

	rp_dma vram_dma;            ///< The single DMA channel feeding #SM_TRAN.

};

/// @brief The one and only system instance, shared by both cores.
/// @ingroup fcbus
extern rp_system sys;

#endif

