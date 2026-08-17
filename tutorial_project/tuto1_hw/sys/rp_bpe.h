/**
 * @file rp_bpe.h
 * @brief Byte-pair-encoding decompressor: format constants and the entry point.
 * @ingroup platform
 *
 * BPE replaces a recurring byte pair with one unused byte value, repeatedly. A
 * stream is a run of independent frames, each carrying its own substitution
 * dictionary, so decoding needs no state beyond the current frame.
 *
 * @note Compiled into this firmware through system.h, but nothing in the
 *       tutorial calls bpe_decode(); the compressed assets belong to the sample
 *       game. @see @ref conventions
 */
#pragma once

#define BPE_ERROR 65536          ///< Encoder error sentinel. Unused by the decoder. @ingroup platform
#define DEFAULTFRAMESIZE 0x8000  ///< Frame size the encoder targets. Unused by the decoder. @ingroup platform
#define ENCODE 1                 ///< Direction flag from the PC-side tool. Unused here. @ingroup platform
#define DECODE 2                 ///< Direction flag from the PC-side tool. Unused here. @ingroup platform

/// @brief Width-named aliases carried over from the PC-side `bpe_fc` tool, kept so
///        the struct declarations below match its source. `ulong` is 32-bit here.
/// @ingroup platform
typedef unsigned long ulong;
/// @copydoc ulong
typedef unsigned short ushort;
/// @copydoc ulong
typedef unsigned char uchar;

#pragma pack(push,1)

/**
 * @brief File header written by the PC-side `bpe_fc` tool.
 * @ingroup platform
 * @note bpe_decode() never reads this: the assets are packed as bare frame
 *       streams, so the signature and CRC are not checked at run time.
 */
typedef struct {
	uchar sign[4];      ///< Magic, `"BPE2"`.
	ulong crc32;        ///< CRC32 of the uncompressed data.
	ushort maxframesize;///< Largest decompressed frame, for sizing the output buffer.
} BPEHEADR;//"BPE2"

/**
 * @brief Per-frame header preceding each dictionary and compressed block.
 * @ingroup platform
 * @warning Declared, but not used to read the stream: bpe_decode() parses the
 *          five bytes by hand, little-endian, and would not agree with this
 *          struct anyway -- `#pragma pack(1)` or not, the compiler is free to
 *          lay it out differently from the wire format. The block comment at
 *          the foot of this file describes `pass` as a `ushort`; the code reads
 *          one byte. The code is authoritative.
 */
typedef struct {
	uchar pass;   ///< Number of dictionary entries; 0 terminates the stream.
	ushort decomp;///< Decompressed size of this frame, in bytes.
	ushort comp;  ///< Compressed size of this frame, in bytes.
} FRAMEHEADR;


//変換辞書
/**
 * @brief One substitution: the byte pair that `onebyte` stands for.
 * @ingroup platform
 * @note Also declarative only. On the wire the dictionary is three parallel
 *       arrays of `pass` bytes -- high halves, low halves, then replacements --
 *       not an array of these structs. See `frame_decode2()`.
 */
typedef struct {
	ushort twobyte;///< The original byte pair.
	uchar onebyte; ///< The unused byte value substituted for it.
} BPE_DIC;
#pragma pack(pop)



/**
 * @brief Decompresses a whole BPE frame stream.
 * @param buf Compressed input, positioned at the first frame header.
 * @param wbuf Output buffer; frames are appended end to end.
 * @return Total bytes written, or -1 if a frame decompressed to a size other
 *         than the one its header declared.
 * @warning `wbuf` is written without any bound: the caller must already know the
 *          decompressed size. Nothing in the stream is validated before the
 *          write, so corrupt input overruns the buffer rather than failing.
 * @ingroup platform
 */
extern int bpe_decode(uint8_t *buf, uint8_t *wbuf );


//BPEファイル
/*

[フレームヘッダ]
パス(ushort)
元のサイズ(ushort)
圧縮サイズ(ushort)
辞書(パス数 * 3byte)

[圧縮データ]
バイナリ(圧縮サイズ)

以下[フレームヘッダ][圧縮データ]...が続く

*/
