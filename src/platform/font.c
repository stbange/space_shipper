/* font.c - rasterize the embedded 8x8 font into an RGBA atlas. */
#include "font.h"

#include <string.h>

#include "font8x8_basic.h"

void fr_font_atlas_rgba8(uint8_t out[128 * 128 * 4])
{
    memset(out, 0, 128 * 128 * 4);
    for (int g = 0; g < 128; g++) {
        int cell_col = g % 16, cell_row = g / 16;
        for (int y = 0; y < 8; y++) {
            uint8_t bits = (uint8_t)font8x8_basic[g][y];
            for (int x = 0; x < 8; x++) {
                int px = cell_col * 8 + x;
                int py = cell_row * 8 + y;
                uint8_t *texel = &out[(py * 128 + px) * 4];
                texel[0] = 255;
                texel[1] = 255;
                texel[2] = 255;
                texel[3] = (bits >> x) & 1 ? 255 : 0;
            }
        }
    }
    /* Solid cell 255 for background quads. */
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int px = 15 * 8 + x, py = 15 * 8 + y;
            uint8_t *texel = &out[(py * 128 + px) * 4];
            texel[0] = 255;
            texel[1] = 255;
            texel[2] = 255;
            texel[3] = 255;
        }
}
