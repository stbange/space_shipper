/* font.h - embedded 8x8 bitmap font atlas builder. */
#ifndef FR_FONT_H
#define FR_FONT_H

#include <stdint.h>

/* Builds a 16x16-glyph atlas (128x128 RGBA). Glyph cells 0..127 hold the
 * ASCII font; cell 255 is a solid white block used for background quads. */
void fr_font_atlas_rgba8(uint8_t out[128 * 128 * 4]);

#endif
