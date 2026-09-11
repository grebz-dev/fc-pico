/*
    Obj3d.h
 */

/**
 * @file Obj3d.h
 * @brief Scene-object wrapper over the ArduinoGL immediate-mode pipeline.
 * @ingroup graphics
 *
 * An Obj3d bundles a transform, a velocity, a draw mode and (for models) vertex
 * data, so a scene can be a plain array of these rather than a sequence of
 * matrix calls. draw() reloads the cached camera matrix first, so objects are
 * independent of one another and of draw order.
 *
 * Angles are 8-bit: 256 units is a full turn, which makes wrapping free.
 *
 * @note The 3D path is dormant in this tutorial -- `glUseCanvas()` is never
 *       called, so `glEnd()` returns immediately. @see @ref graphics_page
 */

#ifndef Obj3d_h
#define Obj3d_h

#include "Arduino.h"

/**
 * @brief Draw mode for an Obj3d.
 * @ingroup graphics
 * @note Modes below #OMD_2D bypass the 3D transform and draw in screen space.
 */
enum {
	OMD_NONE = 0,   ///< Not drawn.
	OMD_MODEL,		///< Triangle model from setModelData(). // モデル描画モード
	OMD_CUBE,		///< Built-in unit cube. // CUBE描画モード

	OMD_2D,			///< Marker: modes at or after this are 2D. // ---- 以下は2D描画モード ---
	OMD_PSET,		///< Single point. // 点描画モード
	OMD_SPR16,		///< 16x16 sprite. // 16x16スプライト描画モード
	OMD_SPR8,		///< 8x8 sprite. // 8x8スプライト描画モード

};



/**
 * @brief One transformable, drawable object in a scene.
 * @ingroup graphics
 */
class Obj3d {
public:
    /// @brief Constructs the object. Call init() to establish a usable transform.
    Obj3d();

	/// @brief Resets the transform, velocity and draw mode to defaults.
	void init();

	/**
	 * @brief Attaches triangle model data.
	 * @param dt Flat vertex array, 9 floats per triangle, read with @c pgm_read_float.
	 * @param pn Triangle count.
	 * @param col_dt Optional per-face colour array; each byte packs colour in bits
	 *               0-1 and a dither offset in bits 2-6. Pass @c NULL for a flat colour.
	 */
	void setModelData( const float *dt, int pn, const uint8_t *col_dt = NULL );

	/**
	 * @brief Sets the object's orientation.
	 * @param ax,ay,az Rotation about each axis in 1/256-turn units.
	 */
	void setAngle( uint8_t ax, uint8_t ay,uint8_t az ) {
		m_angle_x = ax;
		m_angle_y = ay;
		m_angle_z = az;
	}

	/**
	 * @brief Draws the object in its current mode.
	 * @details Reloads the cached camera matrix, applies translation, then rotation
	 *          and scale for 3D modes or a sprite zoom for 2D modes, and dispatches
	 *          on #m_mode.
	 */
	void draw();

	/// @brief Integrates the velocities into position and orientation by one step.
	void move();

	uint8_t m_mode;      ///< Draw mode; one of the @c OMD_ values.
	uint8_t m_color;     ///< Base colour 0..3, applied via Canvas::setDefCol().
	uint8_t m_chrNo;     ///< Tile index for the sprite modes.
	uint8_t m_angle_x;   ///< Orientation about X, in 1/256-turn units.
	uint8_t m_angle_y;   ///< Orientation about Y.
	uint8_t m_angle_z;   ///< Orientation about Z.
	uint8_t m_angle_vx;  ///< Angular velocity about X, added by move().
	uint8_t m_angle_vy;  ///< Angular velocity about Y.
	uint8_t m_angle_vz;  ///< Angular velocity about Z.
	int  m_DitherAdd;    ///< Shade bias passed to Canvas::setDitherAdd().


	float m_scale;       ///< Uniform scale, or sprite magnification in 2D modes.
	float m_x;           ///< Position, X.
	float m_y;           ///< Position, Y.
	float m_z;           ///< Position, Z.
	float m_vx;          ///< Velocity, X; added by move().
	float m_vy;          ///< Velocity, Y.
	float m_vz;          ///< Velocity, Z.

private:
	void drawPSET();     ///< @brief Draws #OMD_PSET: a single transformed point.
	void drawSPR16();    ///< @brief Draws #OMD_SPR16: a billboarded 16x16 sprite.
	void drawSPR8();     ///< @brief Draws #OMD_SPR8: a billboarded 8x8 sprite.
	void drawCube();     ///< @brief Draws #OMD_CUBE: the built-in unit cube.

	/**
	 * @brief Emits model vertex @p i to the GL pipeline.
	 * @param i Vertex index into #m_model_dt.
	 */
	void glVertexFromMemory(int i);

	/// @brief Draws #OMD_MODEL: every triangle of #m_model_dt, lit and culled.
	void drawModel(void);

	const float *m_model_dt;         ///< Vertex data supplied to setModelData().
	int  m_model_pn;                 ///< Triangle count.
	const uint8_t *m_model_col_dt;   ///< Optional per-face colour/dither bytes.

};




/**
 * @brief Installs one of the preset camera setups and caches its matrix.
 * @param mode Preset index; mode 1 places the eye at `(0, 1.3*zoom, -zoom)` with `zoom = 20`.
 * @ingroup graphics
 */
extern void initLookAt( int mode );

/**
 * @brief Restores the cached camera matrix.
 * @details Called at the top of every Obj3d::draw() so each object starts from
 *          the camera transform rather than from whatever the previous object left.
 * @ingroup graphics
 */
extern void reloadLookAt();

/**
 * @brief Pans the camera and refreshes the cached matrix.
 * @param x Horizontal input 0..255; 128 is centred.
 * @param y Vertical input 0..255.
 * @ingroup graphics
 */
extern void moveLookAt( int x, int y );

#endif

