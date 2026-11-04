/* grid.c - ASCII character-cell framebuffer. */
#include "grid.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void fr_grid_init(FrGrid *g, int w, int h)
{
    if (w > FR_GRID_W) w = FR_GRID_W;
    if (h > FR_GRID_H) h = FR_GRID_H;
    g->w = w;
    g->h = h;
    fr_grid_clear(g, FR_BLACK);
}

void fr_grid_clear(FrGrid *g, uint8_t bg)
{
    memset(g->glyph, ' ', FR_GRID_CELLS);
    memset(g->fg, FR_WHITE, FR_GRID_CELLS);
    memset(g->bg, bg, FR_GRID_CELLS);
}

void fr_grid_set_depth(FrGrid *g, float d)
{
    for (int i = 0; i < FR_GRID_CELLS; i++) g->depth[i] = d;
}

bool fr_grid_in_bounds(const FrGrid *g, int x, int y)
{
    return x >= 0 && y >= 0 && x < g->w && y < g->h;
}

void fr_grid_putc(FrGrid *g, int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    if (!fr_grid_in_bounds(g, x, y)) return;
    int i = y * g->w + x;
    g->glyph[i] = (uint8_t)ch;
    g->fg[i] = fg;
    g->bg[i] = bg;
}

int fr_grid_putsn(FrGrid *g, int x, int y, const char *s, int max_w, uint8_t fg, uint8_t bg)
{
    int n = 0;
    for (const char *p = s; *p && n < max_w; p++, n++) {
        fr_grid_putc(g, x + n, y, *p, fg, bg);
    }
    return n;
}

int fr_grid_puts(FrGrid *g, int x, int y, const char *s, uint8_t fg, uint8_t bg)
{
    return fr_grid_putsn(g, x, y, s, g->w - x, fg, bg);
}

void fr_grid_fill_rect(FrGrid *g, int x, int y, int w, int h, char ch, uint8_t fg, uint8_t bg)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            fr_grid_putc(g, x + i, y + j, ch, fg, bg);
}

void fr_grid_hline(FrGrid *g, int x, int y, int w, char ch, uint8_t fg, uint8_t bg)
{
    fr_grid_fill_rect(g, x, y, w, 1, ch, fg, bg);
}

void fr_grid_vline(FrGrid *g, int x, int y, int h, char ch, uint8_t fg, uint8_t bg)
{
    fr_grid_fill_rect(g, x, y, 1, h, ch, fg, bg);
}

void fr_grid_box(FrGrid *g, int x, int y, int w, int h, uint8_t fg, uint8_t bg)
{
    if (w < 2 || h < 2) return;
    fr_grid_hline(g, x, y, w - 1, '-', fg, bg);
    fr_grid_hline(g, x, y + h - 1, w - 1, '-', fg, bg);
    fr_grid_vline(g, x, y, h, '|', fg, bg);
    fr_grid_vline(g, x + w - 1, y, h, '|', fg, bg);
    fr_grid_putc(g, x, y, '+', fg, bg);
    fr_grid_putc(g, x + w - 1, y, '+', fg, bg);
    fr_grid_putc(g, x, y + h - 1, '+', fg, bg);
    fr_grid_putc(g, x + w - 1, y + h - 1, '+', fg, bg);
}

void fr_rgb332_to_rgba8(uint8_t c, uint8_t out[4])
{
    int r = (c >> 5) & 7;
    int g = (c >> 2) & 7;
    int b = c & 3;
    out[0] = (uint8_t)(r * 255 / 7);
    out[1] = (uint8_t)(g * 255 / 7);
    out[2] = (uint8_t)(b * 255 / 3);
    out[3] = 255;
}

void fr_grid_dump(const FrGrid *g, char *out, size_t outsz)
{
    size_t pos = 0;
    for (int y = 0; y < g->h; y++) {
        for (int x = 0; x < g->w; x++) {
            if (pos + 2 >= outsz) return;
            out[pos++] = (char)g->glyph[y * g->w + x];
        }
        out[pos++] = '\n';
    }
    out[pos] = 0;
}
