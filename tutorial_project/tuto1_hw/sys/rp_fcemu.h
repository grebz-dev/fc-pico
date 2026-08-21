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
 * @file rp_fcemu.h
 * @brief A 6502 interpreter, used solely to run the music driver.
 * @ingroup audio
 *
 * @warning Despite the name, this is **not** the cartridge bus emulation. The
 *          hardware interface lives in rp_system and fcppu.pio. This class
 *          executes 6502 machine code on the RP2350, and its only purpose is to
 *          run NSD.Lib's driver so its APU register writes can be captured.
 *
 * The emulated memory map is deliberately minimal:
 *
 * | Range | Read | Write |
 * |---|---|---|
 * | `$0000`-`$1FFF` | 2 KB RAM, mirrored | same |
 * | `$2000`-`$3FFF` | `0xFF` | ignored |
 * | `$4000`-`$4017` | `0xFF` | **rp_system::setAPU()** |
 * | `$4018`-`$7FFF` | `0xFF` | ignored |
 * | `$8000`-`$BFFF` | song data (16 KB) | — |
 * | `$C000`-`$DFFF` | DPCM samples (8 KB) | — |
 * | `$E000`-`$FFFF` | driver code (8 KB) | — |
 *
 * The APU window is the only outlet, and it is the entire point of the class.
 *
 * @note Licensing: derived from FabGL and therefore **GPL v3**.
 *       @see @ref references
 * @see rp_nsfplayer, @ref audio_page
 */

#pragma once


#include <stdint.h>
//#include "rp_sound.h"




/**
 * @brief Cycle-counting 6502 interpreter with a sound-only memory map.
 * @ingroup audio
 *
 * Supports decimal mode and the common undocumented opcodes (LAX, SAX, DCP,
 * ISC), because NSD.Lib and drivers like it may rely on them.
 */
class rp_fcemu {

public:
  /// @brief Clears RAM and resets the processor state.
  void init();

  /**
   * @brief Performs a reset, loading the program counter from the `$FFFC` vector.
   * @return Cycles consumed.
   */
  int reset();

  /**
   * @brief Raises a maskable interrupt, vectoring through `$FFFE`.
   * @return Cycles consumed, or 0 if interrupts are disabled.
   */
  int IRQ();

  /**
   * @brief Raises a non-maskable interrupt, vectoring through `$FFFA`.
   * @return Cycles consumed.
   */
  int NMI();

  /// @brief Sets the program counter.  @param addr New value.
  void setPC(uint16_t addr) { m_PC = addr; }

  /// @brief Reads the program counter.  @return Current value.
  uint16_t getPC()          { return m_PC; }

  /**
   * @brief Calls an emulated subroutine and runs until it returns.
   * @param addr Entry point.
   * @param a Initial accumulator.
   * @param x Initial X register.
   * @param y Initial Y register.
   *
   * @details This is a *call and return* emulator, not a free-running CPU. The
   *          stack pointer is seeded to `0xFD` and execution stops when:
   *          - a `BRK` is executed, or
   *          - an `RTS` pops past that initial frame (`m_SP == 0xfd`), or
   *          - an unrecognised opcode is reached, which prints a diagnostic.
   *
   * @warning There is no cycle budget or watchdog here. A driver that never
   *          returns will hang core 1 until the hardware watchdog fires.
   */
  void run(uint16_t addr, uint8_t a, uint8_t x, uint8_t y );

  /**
   * @brief Maps a contiguous 32 KB image across all three ROM windows.
   * @param ROM Base pointer; `$8000` maps to `ROM[0]`, `$C000` to `ROM[0x4000]`,
   *            `$E000` to `ROM[0x6000]`.
   */
  void setROM( const uint8_t *ROM ) {
  	m_ROM8000 = ROM;
  	m_ROMC000 = &ROM[0x4000];
  	m_ROME000 = &ROM[0x6000];
  }
  /// @brief Maps the `$8000`-`$BFFF` window.  @param ROM 16 KB of song data.
  void setROM8000( const uint8_t *ROM ) { m_ROM8000 = ROM; }		// Sound data 16kb
  /// @brief Maps the `$C000`-`$DFFF` window.  @param ROM 8 KB of DPCM samples.
  void setROMC000( const uint8_t *ROM ) { m_ROMC000 = ROM; }		// DPCM 8kb
  /// @brief Maps the `$E000`-`$FFFF` window.  @param ROM 8 KB of driver code.
  void setROME000( const uint8_t *ROM ) { m_ROME000 = ROM; }		// Sound Driver 8kb

  /// @brief The emulated 2 KB of work RAM.
  uint8_t   m_RAM[ 0x800 ];
//  uint8_t   m_APU[ 0x18 ];

  /**
   * @brief Reads one byte through the emulated memory map.
   * @param addr Emulated address.
   * @return The byte, or `0xFF` for unmapped regions.
   */
  uint8_t readByte( uint16_t addr );

private:
  /**
   * @brief Executes one instruction.
   * @return Cycles consumed, or 0 to stop execution.
   */
  int step();

  /**
   * @brief Writes one byte through the emulated memory map.
   * @param addr Emulated address.
   * @param value Byte to write.
   * @note Writes in `$4000`-`$4017` are forwarded to rp_system::setAPU(); this is
   *       the emulator's only side effect on the outside world.
   */
  void writeByte( uint16_t addr, uint8_t value );

  /// @brief Fast zero-page read.  @param addr Address; only the low byte matters.  @return The byte.
  uint8_t readByteP0( uint16_t addr );
  /// @brief Fast zero-page write.  @param addr Address.  @param value Byte to write.
  void writeByteP0( uint16_t addr, uint8_t value );
  /// @brief Fast stack-page read.  @param addr Address.  @return The byte at `$0100 + (addr & 0xFF)`.
  uint8_t readByteP1( uint16_t addr );
  /// @brief Fast stack-page write.  @param addr Address.  @param value Byte to write.
  void writeByteP1( uint16_t addr, uint8_t value );

  /// @brief Pointer to an add/subtract implementation, selected by the decimal flag.
  typedef void (rp_fcemu::*ADCSBC)(uint8_t);

  /// @brief Binary-mode ADC.  @param m Operand.
  void OP_BINADC(uint8_t m);
  /// @brief Binary-mode SBC.  @param m Operand.
  void OP_BINSBC(uint8_t m);

  /// @brief BCD-mode ADC.  @param m Operand.
  void OP_BCDADC(uint8_t m);
  /// @brief BCD-mode SBC.  @param m Operand.
  void OP_BCDSBC(uint8_t m);

  /// @brief Repoints #m_OP_ADC and #m_OP_SBC after the decimal flag changes.
  void setADCSBC();


  ADCSBC   m_OP_ADC;      ///< Active ADC implementation.
  ADCSBC   m_OP_SBC;      ///< Active SBC implementation.

  uint16_t m_PC;          ///< Program counter.
  uint8_t  m_A;           ///< Accumulator.
  uint8_t  m_X;           ///< X index register.
  uint8_t  m_Y;           ///< Y index register.
  uint8_t  m_SP;          ///< Stack pointer; `0xFD` marks the initial frame. @see run

  bool     m_carry;       ///< Carry flag.
  bool     m_zero;        ///< Zero flag.
  bool     m_intDisable;  ///< Interrupt-disable flag.
  bool     m_decimal;     ///< Decimal-mode flag; selects the ADC/SBC pair.
  bool     m_overflow;    ///< Overflow flag.
  bool     m_negative;    ///< Negative flag.

  const uint8_t   *m_ROM8000;   ///< `$8000`-`$BFFF`: song data.
  const uint8_t   *m_ROMC000;   ///< `$C000`-`$DFFF`: DPCM samples.
  const uint8_t   *m_ROME000;   ///< `$E000`-`$FFFF`: driver code and vectors.

};

