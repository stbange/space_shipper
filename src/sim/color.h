/* color.h - shared RGB332 color constants (used by sim and render). */
#ifndef FR_COLOR_H
#define FR_COLOR_H

#include <stdint.h>

/* Colors are packed RGB332 bytes (3 bits R, 3 bits G, 2 bits B). */
#define FR_RGB332(r, g, b) ((uint8_t)((((r) & 7) << 5) | (((g) & 7) << 2) | ((b) & 3)))

/* Named colors. */
#define FR_BLACK FR_RGB332(0, 0, 0)
#define FR_WHITE FR_RGB332(7, 7, 3)
#define FR_GRAY FR_RGB332(4, 4, 2)
#define FR_DGRAY FR_RGB332(2, 2, 1)
#define FR_LGRAY FR_RGB332(6, 6, 3)
#define FR_RED FR_RGB332(7, 0, 0)
#define FR_LRED FR_RGB332(7, 3, 3)
#define FR_GREEN FR_RGB332(0, 7, 0)
#define FR_LGREEN FR_RGB332(3, 7, 3)
#define FR_BLUE FR_RGB332(0, 0, 3)
#define FR_LBLUE FR_RGB332(2, 4, 3)
#define FR_CYAN FR_RGB332(0, 7, 3)
#define FR_LCYAN FR_RGB332(3, 7, 3)
#define FR_YELLOW FR_RGB332(7, 7, 0)
#define FR_LYELLOW FR_RGB332(7, 7, 2)
#define FR_ORANGE FR_RGB332(7, 3, 0)
#define FR_MAGENTA FR_RGB332(7, 0, 3)
#define FR_LMAGENTA FR_RGB332(7, 4, 4)
#define FR_BROWN FR_RGB332(5, 3, 0)

#endif
