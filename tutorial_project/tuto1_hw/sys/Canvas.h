/*
    Canvas.h - Simple canvas.
 */

/**
 * @file Canvas.h
 * @brief Software rasteriser writing into a byte-per-pixel canvas with a Z plane.
 * @ingroup graphics
 *
 * The canvas is the only place a frame exists as pixels. Once drawing is done,
 * rp_system::convVram() transposes it into the two-bitplane layout the PPU
 * fetches, and the pixel form is discarded.
 *
 * Only four colour values (0..3) are representable, matching one NES palette.
 * Shading is therefore expressed as **dither density** rather than as colour --
 * see getDitherCol() and @ref graphics_page.
 *
 * @see Canvas::frame_buff for the colour/depth split, which is easy to get wrong.
 */

#ifndef Canvas_h
#define Canvas_h

#include "Arduino.h"

#define CANVAS_WIDTH 256    ///< Canvas width in pixels; matches the NES visible width.
#define CANVAS_HEIGHT 240   ///< Canvas height in pixels; matches the NES visible height.
#define FRAME_BUF_SIZE (CANVAS_WIDTH * CANVAS_HEIGHT)  ///< Pixels per plane (not the buffer size -- there are two planes).

#define FLIP_W	1		///< Sprite flip flag: mirror horizontally. // 左右反転
#define FLIP_H	2		///< Sprite flip flag: mirror vertically. // 上限反転


/**
 * @brief A 256x240 drawing surface with an integrated depth buffer.
 * @ingroup graphics
 *
 * A single global instance, #c, is used throughout. All drawing happens on
 * core 0, between the frame handshake and rp_system::convVram().
 */
class Canvas {

public:
    /// @brief Constructs the canvas. Call clear() before the first frame.
    Canvas();

    /// @brief Clears the colour plane and resets the depth plane for a new frame.
    void clear();

    /**
     * @brief Sets the clipping rectangle by corner coordinates.
     * @param xl Left edge, inclusive.
     * @param yl Top edge, inclusive.
     * @param xh Right edge.
     * @param yh Bottom edge.
     */
    void setClip(int xl, int yl,int xh, int yh ) {
    	clipXL = xl;
    	clipXH = xh;
    	clipYL = yl;
    	clipYH = yh;
    };

    /**
     * @brief Sets the clipping rectangle by origin and size.
     * @param x Left edge.
     * @param y Top edge.
     * @param w Width in pixels.
     * @param h Height in pixels.
     */
    void setClipWH(int x, int y,int w, int h ) {
		setClip( x, y ,x + w, y+h );
    };

    // スプライトキャラデータアドレスセット
    /**
     * @brief Binds the tile sheet that subsequent sprite draws read from.
     * @param pData NES 2bpp pattern data: 16 bytes per tile, plane 0 at +0, plane 1 at +8.
     * @note Bound once at scene setup, e.g. `c.setSprData(_acOBJ)`.
     */
    void setSprData( const uint8_t *pData ) {
		pSprData = pData;
	};

   	// スプライト描画キャラ番号セット
    /**
     * @brief Selects which tile the next sprite draw uses.
     * @param c Tile index into the sheet bound by setSprData().
     * @note Multi-tile sprites step by `0x10` per row, so a 16x16 sprite occupies
     *       `c`, `c+1`, `c+0x10`, `c+0x11`.
     */
    void setSprChr( const uint8_t c ) {
		SprChr = c;
	};

   	// スプライト描画倍率セット
    /**
     * @brief Sets independent horizontal and vertical sprite scaling.
     * @param mgw Horizontal magnification; 1.0 is unscaled.
     * @param mgh Vertical magnification.
     */
    void setSprZoom( float mgw, float mgh );

   	// w値でスプライト描画倍率セット
    /**
     * @brief Sets sprite scale from a clip-space @c w, giving perspective sizing.
     * @param w Homogeneous coordinate from the transform; scale becomes `32.0f / w`.
     * @see ArduinoGL.cpp, `GL_SPR16` / `GL_SPR8`
     */
    void setSprZoomW( float w );

   	// スプライト反転描画セット
    /**
     * @brief Sets the mirroring applied to subsequent sprite draws.
     * @param flip Bitwise OR of #FLIP_W and #FLIP_H, or 0.
     */
    void setSprFlip( const uint8_t flip ) {
		SprFlip = flip;
	};

    /**
     * @brief Plots one pixel in the current default colour, honouring clip and depth.
     * @param x Column.
     * @param y Row.
     */
    void setPixel(int x, int y );

    /**
     * @brief Draws a straight line.
     * @param startX,startY First endpoint.
     * @param endX,endY Second endpoint.
     */
    void drawLine(int startX, int startY, int endX, int endY);

    /**
     * @brief Draws a circle outline.
     * @param centreX,centreY Centre.
     * @param radius Radius in pixels.
     */
    void drawCircle(int centreX, int centreY, int radius);

    /**
     * @brief Draws a rectangle outline.
     * @param startX,startY One corner.
     * @param endX,endY The opposite corner.
     */
    void drawSquare(int startX, int startY, int endX, int endY);

    /**
     * @brief Draws a triangle outline.
     * @param x1,y1 First vertex.
     * @param x2,y2 Second vertex.
     * @param x3,y3 Third vertex.
     */
    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3);

    /**
     * @brief Rasterises a filled, depth-tested, dithered triangle.
     * @param x1,y1 First vertex.
     * @param x2,y2 Second vertex.
     * @param x3,y3 Third vertex.
     * @details Sorts by Y, splits at the middle vertex and fills each half with
     *          draw_flatTriangle(). Depth comes from the last setZval() and the
     *          shade from the last setDitherNo().
     * @note The original source marks this as a candidate for offloading to core 1.
     */
	void draw_triangle( int x1, int y1, int x2, int y2, int x3, int y3);
//	void draw_triangle2( int x1, int y1, int x2, int y2, int x3, int y3);

    /**
     * @brief Draws a 16x16 sprite (2x2 tiles) centred on a point.
     * @param x,y Centre of the sprite.
     */
	void drawSPR16( int x, int y );

    /**
     * @brief Draws an 8x8 sprite (one tile) centred on a point.
     * @param x,y Centre of the sprite.
     */
	void drawSPR8(  int x, int y );

    /**
     * @brief Draws a sprite of arbitrary tile dimensions, centred on a point.
     * @param x,y Centre of the sprite.
     * @param cw Width in tiles.
     * @param ch Height in tiles.
     */
	void drawSPR_WH( int x, int y, int cw, int ch );

    /**
     * @brief Draws one 8x8 character.
     * @param c Tile index.
     * @param x,y Top-left position in pixels.
     * @param pData Pattern data to read from, e.g. `_font`.
     * @note Colour 0 is transparent. The glyph is masked with the current default
     *       colour, which is how one font renders in several colours.
     */
	void drawCHR( uint8_t c, int x, int y, const uint8_t* pData );

    /**
     * @brief Draws a NUL-terminated string.
     * @param str Text to draw.
     * @param x,y Top-left position in pixels.
     * @param pData Pattern data supplying the glyphs, e.g. `_font`.
     */
	void drawString( const char *str, int x, int y, const uint8_t* pData );

    /**
     * @brief Returns the colour plane.
     * @return Pointer to `CANVAS_WIDTH * CANVAS_HEIGHT` bytes.
     * @warning Only bits 0-1 are colour; the upper bits carry depth sub-precision
     *          and must be masked off. @see frame_buff
     */
    uint8_t* bitmap();

    int width();    ///< @brief Canvas width.  @return #CANVAS_WIDTH.
    int height();   ///< @brief Canvas height. @return #CANVAS_HEIGHT.

    /**
     * @brief Sets the colour used by subsequent draws.
     * @param c Colour index 0..3, indexing the active NES palette.
     */
    void setDefCol( uint8_t c ) {
		defCol = c;
	}

    /**
     * @brief Sets a bias added to every subsequent dither level.
     * @param add Signed offset applied by setDitherNo() before clamping.
     * @details Lets an object be drawn consistently lighter or darker than the
     *          lighting calculation alone would make it.
     */
    void setDitherAdd( int add ) {
		m_DitherAdd = add;
	}

    /**
     * @brief Sets the depth value used by subsequent draws.
     * @param fz Normalised depth; 0.0 is far, 1.0 is near.
     * @details Split across two stores: the high byte goes to the depth plane and
     *          the low six bits into the colour byte's upper bits, which is why
     *          colour reads must mask `& 3`.
     */
    void setZval( float fz );

    /**
     * @brief Selects the dither pattern, i.e. the apparent shade.
     * @param no Level 0..15; #m_DitherAdd is added first, then the result is clamped.
     */
    void setDitherNo( int no );

    /**
     * @brief Resolves the dithered colour for one pixel position.
     * @param x,y Pixel position; only the low two bits of each are used.
     * @return The colour to plot at that position for the current dither level.
     */
    uint8_t getDitherCol( int x, int y );


private:
    /**
     * @brief Fills one horizontal span with depth testing and dithering.
     * @param y Scanline.
     * @param x2,x3 Span endpoints.
     * @warning Contains a line the original author documented as apparently
     *          redundant but which, when removed, intermittently corrupts the
     *          display. Cause never identified; treat as load-bearing.
     */
	void draw_Xaxis( int y, int x2, int x3 );

    /**
     * @brief Fills a triangle with one horizontal edge, using 16.16 fixed-point stepping.
     * @param x1,y1 Apex.
     * @param x2,y2 First base vertex.
     * @param x3 X of the second base vertex; its Y equals @p y2.
     */
	void draw_flatTriangle( int x1, int y1, int x2, int y2, int x3);

    /**
     * @brief Decodes one 8x8 NES tile into a 64-byte colour buffer.
     * @param c Tile index.
     * @param pCBUF Destination, at least 64 bytes.
     * @param pData Pattern data; tile at `pData[c*16]`, plane 0 at +0, plane 1 at +8.
     * @details Applies #SprFlip and masks the result with #defCol.
     */
	void makeCBUF(uint8_t c, uint8_t* pCBUF , const uint8_t* pData );

    /**
     * @brief Colour plane followed by depth plane, in one allocation.
     * @details `[0 .. FRAME_BUF_SIZE-1]` is colour, one byte per pixel;
     *          `[FRAME_BUF_SIZE .. 2*FRAME_BUF_SIZE-1]` is depth.
     * @warning Colour bytes carry depth sub-precision in bits 2-7. Always mask
     *          `& 3` when reading them as colour, as rp_system::convVram() does.
     */
	uint8_t frame_buff[FRAME_BUF_SIZE *2];
	const uint8_t *pSprData;    ///< Tile sheet bound by setSprData().
	uint8_t defCol;             ///< Current draw colour, 0..3.
	uint8_t DitherNo;           ///< Current dither level, 0..15.
	uint8_t zval_L;             ///< Low 6 bits of the depth value, ORed into colour bytes.
	uint8_t zval_H;             ///< High byte of the depth value, stored in the depth plane.
	int  m_DitherAdd;           ///< Bias added by setDitherNo().

	int clipXL;                 ///< Clip rectangle, left edge.
	int clipXH;                 ///< Clip rectangle, right edge.
	int clipYL;                 ///< Clip rectangle, top edge.
	int clipYH;                 ///< Clip rectangle, bottom edge.

	uint8_t SprChr;             ///< Tile index for the next sprite draw.
	uint8_t SprFlip;            ///< Mirroring flags, #FLIP_W / #FLIP_H.
	uint8_t Spr_nw;             ///< @deprecated Unused.
	uint8_t Spr_nh;             ///< @deprecated Unused.
	bool  Spr_mg;               ///< True when sprite scaling is active.
	float Spr_mgw;              ///< Horizontal sprite magnification.
	float Spr_mgh;              ///< Vertical sprite magnification.

};

/// @brief The one and only canvas instance.
/// @ingroup graphics
extern Canvas c;

#endif

