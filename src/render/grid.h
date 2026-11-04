/* grid.h - ASCII character-cell framebuffer (platform independent). */
#ifndef FR_GRID_H_INCLUDED
#define FR_GRID_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../sim/color.h"

#define FR_GRID_W 160
#define FR_GRID_H 90
#define FR_GRID_CELLS (FR_GRID_W * FR_GRID_H)

typedef struct {
    int w, h;
    uint8_t glyph[FR_GRID_CELLS];
    uint8_t fg[FR_GRID_CELLS];
    uint8_t bg[FR_GRID_CELLS];
    float depth[FR_GRID_CELLS]; /* world distance of what covers the cell */
} FrGrid;

void fr_grid_init(FrGrid *g, int w, int h);
void fr_grid_clear(FrGrid *g, uint8_t bg);
void fr_grid_set_depth(FrGrid *g, float d);
bool fr_grid_in_bounds(const FrGrid *g, int x, int y);

void fr_grid_putc(FrGrid *g, int x, int y, char ch, uint8_t fg, uint8_t bg);
/* Draw a string, returns number of cells drawn. */
int fr_grid_puts(FrGrid *g, int x, int y, const char *s, uint8_t fg, uint8_t bg);
/* Draw a string clipped at max_w cells. */
int fr_grid_putsn(FrGrid *g, int x, int y, const char *s, int max_w, uint8_t fg, uint8_t bg);

void fr_grid_fill_rect(FrGrid *g, int x, int y, int w, int h, char ch, uint8_t fg, uint8_t bg);
void fr_grid_hline(FrGrid *g, int x, int y, int w, char ch, uint8_t fg, uint8_t bg);
void fr_grid_vline(FrGrid *g, int x, int y, int h, char ch, uint8_t fg, uint8_t bg);
void fr_grid_box(FrGrid *g, int x, int y, int w, int h, uint8_t fg, uint8_t bg);

/* Utility used by blitters/tests: unpack a palette to RGBA8888. */
void fr_rgb332_to_rgba8(uint8_t c, uint8_t out[4]);

/* Write each grid row as text lines into out (for golden frames). */
void fr_grid_dump(const FrGrid *g, char *out, size_t outsz);

#endif
