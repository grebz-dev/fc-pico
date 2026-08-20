/*
    ap_data.h
 */

/**
 * @file ap_data.h
 * @brief Resource binding: declares the generated asset blobs and the model loader.
 * @ingroup app
 *
 * The corresponding `.cpp` physically includes the generated `res/resdata.c`, so
 * this is where the built-in resource archive lands in flash. Individual assets
 * are not separate symbols any more: they are entries inside one packed archive,
 * addressed by the `res/res_id.h` identifiers through getResData().
 *
 * @see @ref generated_resources for how the archive is produced and regenerated.
 */

#ifndef ap_data_h
#define ap_data_h

#include "_build_option.h"

#include "res/res_id.h"
//#include "res/res_id2.h"

#include <Arduino.h>
#include "sys/ArduinoGL.h"
#include "sys/Canvas.h"
#include "sys/rp_system.h"
#include "sys/Obj3d.h"

/**
 * @brief Flash address of the second, externally uploaded resource archive.
 * @ingroup app
 *
 * Resource ids of 10000 and above are served from here rather than from the
 * archive linked into the firmware; see getResHead(). The address is a raw
 * XIP-mapped flash offset, so nothing in the build checks that the sketch has
 * not grown into it.
 *
 * @see @ref generated_resources
 */
#define RES_DATA_ADR   0x10200000

/**
 * @brief Model identifiers accepted by setModelDataObj().
 * @ingroup app
 * @note Inherited from the author's full game; the tutorial ships no model data,
 *       and setModelDataObj() is compiled out.
 */
enum {
	MDL_TITLE_LOGO = 0,   ///< Title logo mesh.
	MDL_PLAYER_NO,        ///< Player ship mesh.
	MDL_ENEMY_NO,         ///< Enemy mesh, type 1.
	MDL_ENEMY2_NO,        ///< Enemy mesh, type 2.
	MDL_ENEMY3_NO,        ///< Enemy mesh, type 3.

};

//extern const unsigned char _font[4096];
/// @brief Text glyphs: 256 NES 2bpp tiles, from `res/font.chr` via `CHR_FONT`. Set by initResData(). @ingroup app
extern const unsigned char *_font;
/// @brief Sprite sheet: 256 NES 2bpp tiles, from `res/OBJ.chr` via `CHR_OBJ`. Set by initResData(). @ingroup app
extern const unsigned char *_acOBJ;
/// @brief Vestigial: declared but never defined or referenced. The music is
///        fetched with `getResData( NSF_SOUND )` instead. @see @ref audio_page @ingroup app
extern const unsigned char *sound_nsf;


/**
 * @brief Resolves #_acOBJ and #_font out of the built-in resource archive.
 * @ingroup app
 * @note Must run before anything draws; the two pointers are null until it does.
 */
extern void initResData();

/**
 * @brief Returns a pointer to one resource inside an archive.
 * @note In this tutorial only the built-in archive is ever populated; the
 *       10000-and-above range that selects #RES_DATA_ADR is never used. The
 *       sample game uses both. @see @ref sample_game
 * @ingroup app
 */
extern const unsigned char* getResData( int resid );

/**
 * @brief Returns the byte length of one resource inside an archive.
 * @note Nothing in the tutorial calls it: every asset here is a fixed size the
 *       caller already knows.
 * @ingroup app
 */
extern int getResDataSize( int resid );

/**
 * @brief Attaches the mesh and colour data for a model to an object.
 * @note Reduced to `obj->init()` in this tutorial, which ships no model data. It
 *       is retained as a worked example of the intended calling convention; the
 *       sample game has the real one. @see @ref sample_game
 * @ingroup app
 */
extern void setModelDataObj( Obj3d *obj, int no );

/**
 * @brief Starts playback of a compiled-in MP3 track.
 * @note An empty stub in the tutorial, which ships no MP3 data. The sample game
 *       implements it against a separately flashed archive.
 *       @see @ref sample_game
 * @ingroup app
 */
extern void setMP3data( int no );

#endif

