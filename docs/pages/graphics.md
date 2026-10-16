@page graphics_page Graphics pipeline

Rendering runs entirely on core 0, into a linear byte-per-pixel canvas that is then
transposed into the PPU's bitplane fetch order.

## The canvas

Canvas holds `frame_buff[256 * 240 * 2]` -- **two** planes in one array:

| Half | Bytes | Contents |
|---|---|---|
| `frame_buff[0 .. 61439]` | 61440 | Colour, one byte per pixel |
| `frame_buff[61440 .. 122879]` | 61440 | Depth, one byte per pixel |

Only the low **two bits** of each colour byte are colour, giving the four values the
NES can display per palette. The upper bits are not spare: Canvas::setZval() stores the
low six bits of the depth value there, so the colour plane doubles as depth
sub-precision.

@warning Never read a colour byte without masking `& 3`. rp_system::convVram() does
this; anything else touching the buffer must too.

Depth testing in `draw_Xaxis` compares the depth plane first and uses the colour byte's
upper bits as the tiebreaker.

## Converting to bitplanes

rp_system::convVram() is where the canvas becomes something the PPU can fetch. Each run
of eight pixels becomes one 16-bit word:

```cpp
const uint16_t conv_tbl[4] = { 0x0000, 0x0001, 0x0100, 0x0101 };
for (f = 0; f < 8; f++) { dt <<= 1; dt |= conv_tbl[frame_buff[fidx++] & 3]; }
```

The lookup splits a 2-bit colour into one bit for each plane: low byte becomes bitplane
0, high byte bitplane 1 -- exactly the two bytes the PPU fetches per tile row.

The PPU consumes the stream as one flat sequence. Line `y`, tile `x` is word
`31 + 32 * y + x`, and the console makes 66 selected fetches on the pre-render line
and 64 on each visible line (measured on an NES-001; see @ref platform_findings).
The loop above converts
34 words per pass over a flat 256-pixel-wide canvas, so its passes do not line up with
scanlines; the words still land where the PPU reads them, but the last passes read
3,840 bytes past the end of the 61,440-byte canvas into words that are never displayed.
#VRAM_BUF_SIZE allocates 36 columns per line, more than either needs. See
@ref platform_findings.

The buffers are then swapped -- rendering and transmission always use different
buffers.

## Colour, and the dither trick

With four colours and no per-pixel palette selection, shading is done by **dithering**.
Canvas::getDitherCol() indexes a 16-entry pattern table by `(x & 3) | ((y & 3) << 2)`,
giving 16 apparent intensity levels from a 4-colour palette.

Lighting reduces to choosing a dither level. In ArduinoGL.cpp:

```cpp
int getLightData(float *A, float *B);   // cos <= 0 -> 10 + cos*10, else 10 + cos*5
```

The result is a dither index, not a colour. `polygon_light()` computes the face normal,
normalises it, and feeds the result straight to `Canvas::setDitherNo()`.

## Sprites and text

Sprite and glyph data are ordinary NES 2bpp tiles. `Canvas::makeCBUF()` decodes one 8x8
tile (16 bytes, plane 0 at +0, plane 1 at +8) into a 64-byte colour buffer, applying
flip flags and masking with the current default colour.

That masking is what the tutorial's three-colour "HELLO WORLD" demo exploits: the same
glyph data drawn with `setDefCol(3)`, `(2)` and `(1)` produces three different
renderings.

Multi-tile sprites use a tile stride of `0x10` per row, so a 16x16 sprite is tiles
`n`, `n+1`, `n+0x10`, `n+0x11`.

## The 3D layer

ArduinoGL is a fixed-function OpenGL 1.x subset -- immediate mode, two matrix stacks,
a maximum of 24 vertices per primitive.

Beyond the standard entry points it adds `GL_SPR16` and `GL_SPR8`, which draw a
billboarded NES sprite at a transformed 3D point, scaling it by `32.0f / w` so
perspective applies to sprites as well as polygons.

`glEnd()` performs the whole pipeline: build the model-view-projection matrix,
transform, keep clip-space copies for lighting, perspective-divide, backface-cull
against the view vector, light, then rasterise.

Obj3d wraps this for scene objects, with 8-bit angles (256 units = 360°) and a cached
camera matrix that `Obj3d::draw()` reloads per object.

@note **The 3D path is dormant in this tutorial.** `glUseCanvas()` is never called, so
`glCanvas` stays null and `glEnd()` returns immediately. To enable it, call
`glUseCanvas(&c)` during setup.

## Palette and attributes

These do not travel in the pixel stream -- the console still owns them. The cartridge
keeps shadow copies (`m_PAL_W`, `m_ATR_W`), and rp_system::update() diffs them each
frame, emitting #PF_COM_VRAM pokes for whatever changed. Bulk changes go through data
mode instead.

@warning The shipped boot ROM has its #PF_COM_VRAM handler disabled (`.if 0` in
`xPF_COM_VRAM`), so those per-frame pokes have no effect. With this ROM, palette and
attribute changes reach the console only through data mode: rp_system::setPalData() or
setAtrData() (or setPal()/setAtr()), then rp_system::startDataMode(). The console blanks
the display while it uploads, and the frame link stops and resynchronises afterwards.

rp_system::setAtr() packs a palette selector into the right two bits of the right
attribute byte for a given tile coordinate.

## Known quirk

Canvas.cpp carries a candid note from the original author at the span-fill: removing a
particular line that has no effect on drawing *sometimes* corrupts the display. The
cause was never identified and the line was left in place. Treat that function as
load-bearing until someone determines why.
