/**
 * @file fcbus_protocol.h
 * @brief FC PICO <-> console wire protocol constants (the tutorial protocol).
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Every value here is the one used by `tutorial_project/tuto1_hw/sys/rp_system.h`
 * and `tutorial_project/BOOTROM/SysPico.asm`; `platform/tests/test_protocol_parity.py`
 * checks them against those files. Keep each constant on one
 * `#define NAME value  // comment` line so that test can read it.
 */
#ifndef FCBUS_PROTOCOL_H
#define FCBUS_PROTOCOL_H

/* ------------------------------------------------------------------------ */
/* Mailbox: cartridge -> console, on the tail of every frame stream          */
/* ------------------------------------------------------------------------ */
#define PF_MAGIC_NO          0xFC   // mailbox[1] validity sentinel (asm: PF_MAGIC_CODE)
#define PF_COM_NONE          0x00   // end of command list
#define PF_COM_DMOD          0x01   // blank display, enter bulk data mode
#define PF_COM_FDIN          0x02   // palette fade in
#define PF_COM_FDOT          0x03   // palette fade out
#define PF_COM_SE            0x80   // 0x80|n play sound effect n
#define PF_COM_BGM           0xA0   // 0xA0|n play music n
#define PF_COM_VRAM          0xC0   // 0xC0|adrH, adrL, data: poke one VRAM byte

#define PF_DAT_VRAM          0x80   // bulk data mode: VRAM block
#define PF_DAT_RAM           0x81   // bulk data mode: CPU RAM block
#define PF_DAT_STEP          0x82   // bulk data mode: leave and switch console step

#define FC_COM_BUF_SIZE16    16     // mailbox bytes the command executor parses (2..15)
#define FC_COM_BUF_SIZE      64     // mailbox length appended to every frame
#define PICO_SNDREG          0x10   // offset of the APU (reg,value) pair area
#define PICO_APU_BUF_SIZE    0x40   // depth of the firmware's APU write ring
#define APU_PAIRS_MAX        23     // pairs that fit: (64 - 16 - 2) / 2, terminator 0xFF

#define MBX_MAGIC            1      // byte 1: PF_MAGIC_NO
#define MBX_CMD              2      // bytes 2..15: command list, PF_COM_NONE terminated
#define MBX_APU              PICO_SNDREG // bytes 16..: (reg,value) pairs, 0xFF terminated
#define FCBUS_ATTR_LEN       64     // attribute table $23C0-$23FF
#define FCBUS_BG_PAL_LEN     16     // background palette $3F00-$3F0F
#define FCBUS_PAL_LEN        32     // full palette $3F00-$3F1F

/* ------------------------------------------------------------------------ */
/* Opcodes: console -> cartridge, single bytes written to $2007              */
/* ------------------------------------------------------------------------ */
#define FP_COM_ACK           0x0F   // unused on both sides
#define FP_COM_NAK           0x1F   // unused on both sides
#define FP_COM_VER           0x2F   // request the 14-byte boot-ROM build stamp
#define FP_COM_ROM           0x3F   // + page: request 256 bytes of the boot ROM image
#define FP_COM_LOG           0xBF   // + up to 7 bytes: print on the cartridge serial console
#define FP_COM_DRQ           0xCF   // bulk data mode: request next block header
#define FP_COM_DLD           0xDF   // + page: bulk data mode: request one 256-byte page
#define FP_COM_RST           0xEF   // restart the cartridge firmware
#define FP_COM_INI           0xFF   // + stage: initialise the cartridge
/* Any other byte is the controller state, and also the frame heartbeat. */

/* ------------------------------------------------------------------------ */
/* Controller byte bit layout                                                */
/* ------------------------------------------------------------------------ */
#define KEY_A                0x80
#define KEY_B                0x40
#define KEY_SEL              0x20
#define KEY_RUN              0x10   // Start
#define KEY_UP               0x08
#define KEY_DOWN             0x04
#define KEY_LEFT             0x02
#define KEY_RIGHT            0x01

/* ------------------------------------------------------------------------ */
/* Frame stream geometry                                                     */
/* ------------------------------------------------------------------------ */
#define PPU_PICTURE_COUNT    15426  // 66 pre-render + 240*64 visible qualifying reads
#define PPU_COUNTER_REPORT_BIAS 1   // fcppu_rna reports the zero-based index of the last read
#define PPU_COUNT_WINDOW     2      // +/- reads tolerated before the DMA is stopped
#define PPU_COUNT_VAL        (PPU_PICTURE_COUNT + FC_COM_BUF_SIZE)   // 15490
#define VRAM_LINE_WORDS      34     // convVram() batch size; not the selected-read stride
#define VRAM_HEAD_WORDS      31     // word index of line 0, tile 0
#define VRAM_LINES           240
#define VRAM_TILE_COLS       32     // selected 16-bit words per visible scanline
#define VRAM_BUF_BYTES       (36 * 2 * 240 + FC_COM_BUF_SIZE)       // 17344, DMA length in bytes
#define VRAM_MAILBOX_OFF     (PPU_COUNT_VAL - FC_COM_BUF_SIZE)      // 15426: mailbox byte offset
#define FCBUS_VER_SYNC_W0    0x21212121   // "!!!!" pushed before the version stamp
#define FCBUS_VER_SYNC_W1    0x43462321   // "!#FC" little-endian; the fix bank syncs on 'C'
#define FCBUS_ROM_STAMP_OFF  0x6FF0       // stamp offset within the 32 KB PRG image ($EFF0)
#define FCBUS_ROM_STAMP_LEN  14           // bytes compared by CHK_ROMVER (16 streamed)
#define FCBUS_ROM_PRG_BYTES  0x8000
#define FCBUS_ROM_INES_HDR   16
#define FCBUS_DRQ_MAGIC_BASE 0xFC00       // drq_ret(): word0 = base + com + (adr << 16), word1 = size

#endif /* FCBUS_PROTOCOL_H */
