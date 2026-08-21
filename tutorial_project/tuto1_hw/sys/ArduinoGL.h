/*
    ArduinoGL.h - OpenGL subset for Arduino.
    Created by Fabio de Albuquerque Dela Antonio
    fabio914 at gmail.com
 */

/**
 * @file ArduinoGL.h
 * @brief A fixed-function OpenGL 1.x subset rendering into a Canvas.
 * @ingroup graphics
 *
 * Immediate mode only: two matrix stacks, no textures, no shaders, and a hard
 * limit of 24 vertices per primitive. glEnd() performs the entire pipeline --
 * transform, perspective divide, backface cull, light, rasterise.
 *
 * Two draw modes are extensions rather than OpenGL: #GL_SPR16 and #GL_SPR8 draw
 * a billboarded NES sprite at a transformed 3D point, scaled by `32.0f / w` so
 * that perspective applies to sprites too.
 *
 * Lighting has no colour to work with, so it is expressed entirely as **dither
 * density**: the light calculation produces a dither level, not a shade.
 *
 * @warning This pipeline is dormant in the tutorial -- glUseCanvas() is never
 *          called, so the canvas pointer stays null and glEnd() returns
 *          immediately. Call `glUseCanvas(&c)` during setup to enable it.
 * @note Third-party code; see @ref references.
 * @see @ref graphics_page
 */

#ifndef ArduinoGL_h
#define ArduinoGL_h

#include "Arduino.h"
#include "Canvas.h"

/**
 * @brief Primitive type passed to glBegin().
 * @ingroup graphics
 */
typedef enum {
    GL_NONE = 0,        ///< No primitive in progress.
    GL_POINTS,          ///< Points; drawn as a 2x2 blob when near enough.
    GL_LINE_LOOP,       ///< Closed wireframe outline.
    GL_TRIANGLE_STRIP,  ///< Triangle strip, drawn as wireframe.
    GL_POLYGON,         ///< Filled, culled and lit polygon; the main 3D path.
    GL_SPR16,           ///< Extension: billboarded 16x16 sprite at a transformed point.
    GL_SPR8,            ///< Extension: billboarded 8x8 sprite at a transformed point.
} GLDrawMode;

/**
 * @brief Selects which matrix stack subsequent matrix calls affect.
 * @ingroup graphics
 */
typedef enum {
    GL_PROJECTION = 0,  ///< Projection stack.
    GL_MODELVIEW,       ///< Model-view stack.
//  GL_TEXTURE
} GLMatrixMode;

/* Masks */
/// @brief glClear() mask selecting the colour buffer.
#define GL_COLOR_BUFFER_BIT 0x1

/**
 * @brief A vertex in homogeneous coordinates.
 * @ingroup graphics
 * @note Two arrays of these are kept per primitive: one post-divide in normalised
 *       device coordinates for rasterising, and one pre-divide in clip space,
 *       which is what the lighting calculation needs.
 */
typedef struct {
    float x, y, z, w;
} GLVertex;

/**
 * @var GLVertex::x
 * @brief X coordinate.
 */
/**
 * @var GLVertex::y
 * @brief Y coordinate.
 */
/**
 * @var GLVertex::z
 * @brief Z coordinate; also the depth fed to Canvas::setZval().
 */
/**
 * @var GLVertex::w
 * @brief Homogeneous coordinate; sprite scale is `32.0f / w`.
 */

/* Matrices */
/// @brief Selects the active matrix stack.  @param mode Which stack.  @ingroup graphics
void glMatrixMode(GLMatrixMode mode);
/// @brief Post-multiplies the active matrix.  @param m Column-major 4x4.  @ingroup graphics
void glMultMatrixf(float * m);
/// @brief Replaces the active matrix.  @param m Column-major 4x4.  @ingroup graphics
void glLoadMatrixf(float * m);
/// @brief Sets the active matrix to the identity.  @ingroup graphics
void glLoadIdentity(void);
/// @brief Copies a 4x4 matrix.  @param dest Destination.  @param src Source.  @ingroup graphics
void copyMatrix(float * dest, float * src);

/// @brief Pushes the active matrix.  @warning The stack is only 8 deep.  @ingroup graphics
void glPushMatrix(void);
/// @brief Pops the active matrix.  @ingroup graphics
void glPopMatrix(void);

/**
 * @brief Multiplies in an orthographic projection.
 * @param left,right Horizontal clip planes.
 * @param bottom,top Vertical clip planes.
 * @param zNear,zFar Depth clip planes.
 * @ingroup graphics
 */
void glOrtho(float left, float right, float bottom, float top, float zNear, float zFar);

/**
 * @brief Multiplies in a 2D orthographic projection.
 * @param left,right Horizontal clip planes.
 * @param bottom,top Vertical clip planes.
 * @ingroup graphics
 */
void gluOrtho2D(float left, float right, float bottom, float top);

/**
 * @brief Multiplies in a perspective projection given frustum edges.
 * @param left,right Horizontal frustum edges at the near plane.
 * @param bottom,top Vertical frustum edges at the near plane.
 * @param zNear,zFar Depth clip planes.
 * @ingroup graphics
 */
void glFrustum(float left, float right, float bottom, float top, float zNear, float zFar);

/**
 * @brief Multiplies in a perspective projection given a field of view.
 * @param fovy Vertical field of view in degrees.
 * @param aspect Width divided by height.
 * @param zNear,zFar Depth clip planes.
 * @ingroup graphics
 */
void gluPerspective(float fovy, float aspect, float zNear, float zFar);

/**
 * @brief Multiplies in a rotation.
 * @param angle Rotation in degrees.
 * @param x,y,z Axis of rotation.
 * @ingroup graphics
 */
void glRotatef(float angle, float x, float y, float z);

/// @brief Multiplies in a translation.  @param x,y,z Offset.  @ingroup graphics
void glTranslatef(float x, float y, float z);
/// @brief Multiplies in a scale.  @param x,y,z Scale factors.  @ingroup graphics
void glScalef(float x, float y, float z);

/**
 * @brief Multiplies in a viewing transform.
 * @param eyeX,eyeY,eyeZ Camera position.
 * @param centerX,centerY,centerZ Point being looked at.
 * @param upX,upY,upZ Up vector.
 * @note Also records the view direction, which backface culling tests against.
 * @ingroup graphics
 */
void gluLookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);

/**
 * @brief Sets the directional light vector.
 * @param x,y,z Direction the light travels.
 * @note Feeds the dither-level calculation; there is no light colour or intensity.
 * @ingroup graphics
 */
void setLight(float x, float y, float z);

/* Vertices */
/// @brief Emits a vertex from an array.  @param v Four floats: x, y, z, w.  @ingroup graphics
void glVertex4fv(float * v);
/// @brief Emits a vertex.  @param x,y,z,w Homogeneous coordinates.  @ingroup graphics
void glVertex4f(float x, float y, float z, float w);
/// @brief Emits a vertex from an array, with w = 1.  @param v Three floats.  @ingroup graphics
void glVertex3fv(float * v);
/// @brief Emits a vertex with w = 1.  @param x,y,z Position.  @ingroup graphics
void glVertex3f(float x, float y, float z);

/* OpenGL */
/**
 * @brief Binds the canvas the pipeline renders into.
 * @param c Target canvas, normally the global Canvas instance.
 * @warning Until this is called the pipeline is inert and glEnd() returns
 *          immediately. The tutorial never calls it.
 * @ingroup graphics
 */
void glUseCanvas(Canvas * c); /* <-- Arduino only */

/// @brief Sets the size used for #GL_POINTS.  @param size Point size in pixels.  @ingroup graphics
void glPointSize(unsigned size);
/// @brief Clears the bound canvas.  @param mask Currently only #GL_COLOR_BUFFER_BIT.  @ingroup graphics
void glClear(int mask);
/// @brief Begins a primitive.  @param mode Primitive type.  @ingroup graphics
void glBegin(GLDrawMode mode);

/**
 * @brief Ends the primitive and renders it.
 * @details Builds the model-view-projection matrix, transforms every vertex
 *          (retaining clip-space copies for lighting), performs the perspective
 *          divide, culls back faces against the view vector, computes a dither
 *          level from the face normal, and rasterises.
 * @warning Returns immediately if glUseCanvas() has not been called.
 * @ingroup graphics
 */
void glEnd(void);

#endif
