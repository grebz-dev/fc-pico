/*
    rp_dma.h
 */

/**
 * @file rp_dma.h
 * @brief Thin wrapper over one RP2350 DMA channel feeding a PIO state machine.
 * @ingroup fcbus
 *
 * In practice the firmware uses exactly **one** channel, owned by
 * rp_system::vram_dma and pointed at the transmit FIFO of #SM_TRAN. It is
 * re-targeted rather than reallocated: the same channel carries the framebuffer,
 * the boot ROM image, the version string and bulk data-mode payloads, depending
 * on what the console last asked for.
 *
 * @see rp_system::ppu_dma, ::fcppu_r
 */

#ifndef rp_dma_h
#define rp_dma_h

#include "Arduino.h"
#include "hardware/pio.h"
#include "hardware/dma.h"


/**
 * @brief One claimed DMA channel bound to a PIO FIFO.
 * @ingroup fcbus
 */
class rp_dma {

public:
    /// @brief Constructs the wrapper without claiming a channel.
    rp_dma();

	/**
	 * @brief Claims a channel that drains a PIO receive FIFO into memory.
	 * @param pio_no PIO block index.
	 * @param sm State machine index.
	 * @param buf Destination buffer.
	 * @param size Transfer length in 32-bit words.
	 * @return The claimed channel number.
	 * @warning Marked "under construction" in the original source and unused by
	 *          this firmware. Unlike the TX variant it starts immediately.
	 */
	int initSM_DMA_RX( int pio_no, int sm, uint32_t *buf, int size );

	/**
	 * @brief Claims a channel that feeds a PIO transmit FIFO from memory.
	 * @param pio_no PIO block index.
	 * @param sm State machine index.
	 * @param buf Source buffer.
	 * @param size Transfer length in 32-bit words.
	 * @return The claimed channel number.
	 * @note Configured but **not started** -- rp_system::ppu_dma() arms it once per
	 *       frame, only when the stream is confirmed to be in phase.
	 */
    int initSM_DMA_TX( int pio_no, int sm, uint32_t *buf, int size );

	/**
	 * @brief Aborts any transfer in flight and starts a new one.
	 * @param buf Source buffer.
	 * @param buf_size Transfer length in 32-bit words.
	 */
	void TransSM_DMA( uint32_t *buf, int buf_size );

	/**
	 * @brief Aborts the transfer in flight.
	 * @details Used when the frame stream has lost synchronisation: sending nothing
	 *          is preferable to sending misaligned data.
	 */
	void StopDMA();

	/**
	 * @brief Byte-wise memory copy via a fresh DMA channel.
	 * @param DstBuf Destination.
	 * @param SrcBuf Source.
	 * @param n Byte count.
	 * @return The channel number used.
	 * @warning Marked "under verification" in the original source, and it claims a
	 *          channel per call without ever releasing it. Do not use.
	 */
	int memcpyDMA(void *DstBuf, const void *SrcBuf, size_t n);

	/**
	 * @brief Word-wise memory copy via a fresh DMA channel.
	 * @param DstBuf Destination, 4-byte aligned.
	 * @param SrcBuf Source, 4-byte aligned.
	 * @param n Length in 32-bit words.
	 * @return The channel number used.
	 * @warning Same caveat as memcpyDMA(): experimental, and leaks a channel per call.
	 */
	int memcpyDMA32(void *DstBuf, const void *SrcBuf, size_t n);
private:
	int dma_chan;   ///< The claimed channel, or -1 before initialisation.
};

/**
 * @brief Releases every claimed DMA channel.
 * @details Called first thing in rp_system::init() so that a soft reset does not
 *          leak channels: #FP_COM_RST re-runs the whole bring-up sequence.
 * @ingroup fcbus
 */
void initDMA(void);

#endif

