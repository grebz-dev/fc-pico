---
name: fcpico-native-graphics
description: Plan or change FC PICO graphics that the console draws itself - CHR tiles, the 2-bit pixel stream, background palettes and attribute tables, sprites and OAM - within NES limits. Use this whenever someone edits palettes or attributes, adds sprites or text, designs a HUD or menu, converts artwork to tiles, sees wrong colours or blocks, or asks why a palette change does not appear on the console, even if they only mention "colours" or "the screen".
---

# FC PICO native graphics

The picture stream is 2 bits per pixel; colours come from the console's palette and
attribute table, and sprites from OAM. Reference: `docs/pages/graphics.md` and
`docs/pages/platform-findings.md`.

## Limits to design against

- Four background and four sprite palettes of three colours each, plus one shared
  backdrop (colour 0 of every background palette shows `$3F00`).
- One background palette per 16 x 16 block (attribute table, 64 bytes).
- 64 sprites, at most eight per scanline, in 8 x 8 and 8 x 16 mode alike.
- The shipped boot ROM ignores `PF_COM_VRAM`: palette and attribute changes reach the
  console only through data mode (`setPalData`/`setAtrData` or
  `setPal`/`setAtr`, then `startDataMode`), which blanks the display briefly.

## Steps

1. Write down the palettes, which blocks use which palette, and the sprites per
   scanline the design needs. Reject designs that need a ninth sprite on a line or a
   fourth colour in a block, before writing code.
2. Change the tables through the rp_system calls (or the `fcbus_core_*` equivalents in
   `platform/fcbus`), and draw pixels with values 0-3.
3. Check in the emulator with the hardware sprite limit on (`fcpico-cosim`): look at
   the screenshot and the console's palette RAM. For a stream picture, decode it with
   `fcpico.ppu_decode` and compare colour indices, not RGB, because every emulator uses
   a different palette.
4. When switching between screens that use OAM differently, restore the complete OAM
   state of the screen being returned to.

## Done when

The design fits the limits above, the emulator shows it with the sprite limit on, and
you can point to the screenshot and the palette/attribute bytes that produced it. Say
whether it has been seen on a console.
