/*
    ap_title.h
 */

/**
 * @file ap_title.h
 * @brief The tutorial's title scene.
 * @ingroup app
 *
 * A worked example of the scene pattern used throughout the application layer:
 * init() runs once, main() runs every frame. It exercises key auto-repeat, music
 * and sound-effect triggers, and text rendering in each of the three usable
 * colours.
 *
 * @see ap_main for the scene state machine that drives it.
 */

#ifndef ap_title_h
#define ap_title_h


/**
 * @brief Title screen scene.
 * @ingroup app
 *
 * A single global instance, #ap_t.
 */
class ap_title {

public:
    /// @brief Constructs the scene. State lives in file-scope variables, so nothing to do here.
    ap_title() {};

    /**
     * @brief One-time scene setup.
     * @details Stops music, uploads the palette through bulk data mode, clears the
     *          attribute table and binds the sprite sheet.
     * @note rp_system::startDataMode() is called twice; the second call is what
     *       actually flushes the palette set between them.
     */
    void init(void);

    /**
     * @brief One frame of the title screen.
     * @details Clears the canvas, handles Up/Down (key repeat), A (cycle music) and
     *          B (sound effect), then draws three strings and a scaled frame counter.
     */
    void main();

private:
};

/// @brief The one and only title-scene instance.
/// @ingroup app
extern ap_title ap_t;

#endif

