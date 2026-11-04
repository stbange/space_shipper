/* raster3d.c - ASCII 3D software rasterizer. */
#include "raster3d.h"

#include <math.h>
#include <string.h>

const char FR_RAMP[11] = " .:-=+*#%@";

#define PI_FLOAT 3.14159265358979f

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void fr_level_render_map(const FrLevel *lv, float px, float py, float open_dist,
                         uint8_t *out)
{
    memcpy(out, lv->tiles, (size_t)(lv->w * lv->h));
    float od2 = open_dist * open_dist;
    for (int y = 0; y < lv->h; y++) {
        for (int x = 0; x < lv->w; x++) {
            int i = y * lv->w + x;
            if (out[i] == FR_T_DOOR) {
                float dx = (float)x + 0.5f - px;
                float dy = (float)y + 0.5f - py;
                if (dx * dx + dy * dy < od2) out[i] = FR_T_FLOOR_HALL;
            }
        }
    }
}

static uint8_t shade_color(uint8_t base, float b)
{
    if (b > 1.0f) b = 1.0f;
    if (b < 0.05f) b = 0.05f;
    int r = (int)(((base >> 5) & 7) * b);
    int g = (int)(((base >> 2) & 7) * b);
    int bl = (int)((base & 3) * b);
    return (uint8_t)((r << 5) | (g << 2) | bl);
}

static char ramp_glyph(float b)
{
    b = clampf(b, 0.0f, 1.0f);
    int idx = (int)(b * 9.0f + 0.5f);
    if (idx > 9) idx = 9;
    return FR_RAMP[idx];
}

void fr_raster_world(FrGrid *grid, const FrLevel *lv, const FrCamera *cam,
                     const FrViewStyle *style)
{
    int gw = grid->w, gh = grid->h;
    float dirX = cosf(cam->angle), dirY = sinf(cam->angle);
    float planeX = -dirY * 0.8f, planeY = dirX * 0.8f;
    float horizon = gh * 0.5f + cam->pitch;
    float eye = clampf(cam->eye_z, 0.1f, 0.9f);
    float maxd = style->max_dist > 0.1f ? style->max_dist : 24.0f;
    float proj = gh * 0.5f; /* projection plane distance in rows */

    /* Pre-open doors (they render/collide as open floor near the player). */
    static uint8_t rmap[FR_LEVEL_CELLS];
    fr_level_render_map(lv, cam->x, cam->y, 2.5f, rmap);

    /* Per-column wall cast. */
    for (int col = 0; col < gw; col++) {
        float camX = 2.0f * col / (float)gw - 1.0f;
        float rdX = dirX + planeX * camX;
        float rdY = dirY + planeY * camX;

        int mapX = (int)cam->x, mapY = (int)cam->y;
        float deltaX = rdX != 0.0f ? fabsf(1.0f / rdX) : 1e30f;
        float deltaY = rdY != 0.0f ? fabsf(1.0f / rdY) : 1e30f;
        int stepX, stepY;
        float sideX, sideY;
        if (rdX < 0) {
            stepX = -1;
            sideX = (cam->x - (float)mapX) * deltaX;
        } else {
            stepX = 1;
            sideX = ((float)mapX + 1.0f - cam->x) * deltaX;
        }
        if (rdY < 0) {
            stepY = -1;
            sideY = (cam->y - (float)mapY) * deltaY;
        } else {
            stepY = 1;
            sideY = ((float)mapY + 1.0f - cam->y) * deltaY;
        }

        int hit = 0, side = 0;
        uint8_t tile = FR_T_WALL;
        for (int guard = 0; guard < 512 && !hit; guard++) {
            if (sideX < sideY) {
                sideX += deltaX;
                mapX += stepX;
                side = 0;
            } else {
                sideY += deltaY;
                mapY += stepY;
                side = 1;
            }
            if (mapX < 0 || mapY < 0 || mapX >= lv->w || mapY >= lv->h) {
                tile = FR_T_WALL;
                hit = 1;
                break;
            }
            uint8_t t = rmap[mapY * lv->w + mapX];
            if (t >= 32) {
                tile = t;
                hit = 1;
            }
        }

        float perp = side == 0 ? (sideX - deltaX) : (sideY - deltaY);
        if (perp < 0.02f) perp = 0.02f;

        /* Wall texture coordinate (where the ray hit the tile face). */
        float wallX;
        if (side == 0) wallX = cam->y + perp * rdY;
        else wallX = cam->x + perp * rdX;
        wallX -= floorf(wallX);

        float lineH = gh / perp;
        int drawStart = (int)(horizon - lineH * eye);
        int drawEnd = (int)(horizon + lineH * (1.0f - eye));

        uint8_t wallcol = fr_tile_wall_color(tile);
        float boost = fr_tile_wall_boost(tile);
        float distB = 1.0f - perp / maxd;
        if (distB < 0.0f) distB = 0.0f;
        float sideB = side == 1 ? 0.75f : 1.0f;
        float wb = boost * distB * sideB;
        char wglyph = ramp_glyph(wb);
        /* Subtle panel texture: vary glyph on wallX bands. */
        int band = (int)(wallX * 8.0f);
        if ((band & 1) != 0 && tile == FR_T_WALL) wglyph = FR_RAMP[9 - (int)clampf(wb * 9.0f, 0, 9)];

        int y0 = drawStart < 0 ? 0 : drawStart;
        int y1 = drawEnd >= gh ? gh - 1 : drawEnd;

        /* Ceiling rows. */
        for (int row = 0; row < y0 && row < gh; row++) {
            if (row >= (int)horizon) break; /* pitch clamp */
            float rowDist = eye * proj / (horizon - (float)row);
            float b = 1.0f - rowDist / maxd;
            char ch = ' ';
            if (b > 0.35f && tile != FR_T_WINDOW) ch = '.';
            if (b < 0.0f) b = 0.0f;
            uint8_t ccol = shade_color(style->ceil_color, b * 0.5f);
            int i = row * gw + col;
            grid->glyph[i] = (uint8_t)ch;
            grid->fg[i] = ccol;
            grid->bg[i] = shade_color(FR_BLACK, b * 0.2f);
            grid->depth[i] = rowDist;
        }
        /* Wall rows. */
        for (int row = y0; row <= y1; row++) {
            int i = row * gw + col;
            grid->glyph[i] = (uint8_t)wglyph;
            grid->fg[i] = shade_color(wallcol, wb);
            grid->bg[i] = shade_color(wallcol, wb * 0.35f);
            grid->depth[i] = perp;
        }
        /* Floor rows. */
        for (int row = y1 + 1; row < gh; row++) {
            if (row < (int)horizon) continue; /* pitch clamp */
            float rowDist = (1.0f - eye) * proj / ((float)row - horizon);
            float b = 1.0f - rowDist / maxd;
            if (b < 0.0f) b = 0.0f;
            float fx = cam->x + rowDist * rdX;
            float fy = cam->y + rowDist * rdY;
            int tx = (int)fx, ty = (int)fy;
            char ch;
            if (tx >= 0 && ty >= 0 && tx < lv->w && ty < lv->h) {
                uint8_t ft = lv->tiles[ty * lv->w + tx];
                if (ft == FR_T_FLOOR_HALL) ch = ((tx + ty) & 1) ? '.' : '-';
                else if (ft == FR_T_FLOOR_ROOM) ch = ((tx + ty) & 1) ? '.' : '`';
                else ch = '.';
            } else {
                ch = '.';
            }
            if (b < 0.25f) ch = ' ';
            int i = row * gw + col;
            grid->glyph[i] = (uint8_t)ch;
            grid->fg[i] = shade_color(style->floor_color, b);
            grid->bg[i] = shade_color(FR_BLACK, b * 0.15f);
            grid->depth[i] = rowDist;
        }
    }

    /* Sprites (billboards), far to near: sort indices by distance. */
    int n = lv->sprite_count;
    if (n <= 0) return;
    static int order[FR_LEVEL_MAX_SPRITES];
    static float sdist[FR_LEVEL_MAX_SPRITES];
    if (n > FR_LEVEL_MAX_SPRITES) n = FR_LEVEL_MAX_SPRITES;
    for (int i = 0; i < n; i++) {
        const FrSprite *s = &lv->sprites[i];
        float dx = s->x - cam->x, dy = s->y - cam->y;
        sdist[i] = dx * dx + dy * dy;
        order[i] = i;
    }
    for (int i = 1; i < n; i++) {
        int key = order[i];
        float kd = sdist[key];
        int j = i - 1;
        while (j >= 0 && sdist[order[j]] < kd) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    float invDet = 1.0f / (planeX * dirY - dirX * planeY);
    for (int oi = 0; oi < n; oi++) {
        const FrSprite *s = &lv->sprites[order[oi]];
        float relX = s->x - cam->x, relY = s->y - cam->y;
        float tx = invDet * (dirY * relX - dirX * relY);
        float ty = invDet * (-planeY * relX + planeX * relY);
        if (ty <= 0.1f) continue;
        if (ty > maxd + 4.0f) continue;

        float screenX = (gw / 2.0f) * (1.0f + tx / ty);
        float scale = s->scale > 0.05f ? s->scale : 1.0f;
        float spriteH = (gh / ty) * scale;
        float spriteW = spriteH;
        /* Bottom sits on the floor, lifted by v_offset. */
        float floorLine = horizon + (gh / ty) * (1.0f - eye);
        float bottom = floorLine - (gh / ty) * s->v_offset;
        float top = bottom - spriteH;

        int cx = (int)screenX;
        int x0 = (int)(screenX - spriteW * 0.5f);
        int x1 = (int)(screenX + spriteW * 0.5f);
        int y0 = (int)top, y1 = (int)bottom;
        if (x1 < 0 || x0 >= gw || y1 < 0 || y0 >= gh) continue;

        float b = 1.0f - ty / maxd;
        if (b < 0.15f) b = 0.15f;
        uint8_t col = shade_color(s->color, b);
        uint8_t bgc = shade_color(s->color, b * 0.3f);

        for (int row = y0 < 0 ? 0 : y0; row <= y1 && row < gh; row++) {
            for (int colx = x0 < 0 ? 0 : x0; colx <= x1 && colx < gw; colx++) {
                int i = row * gw + colx;
                if (ty < grid->depth[i]) {
                    grid->glyph[i] = (uint8_t)s->glyph;
                    grid->fg[i] = col;
                    grid->bg[i] = bgc;
                    grid->depth[i] = ty;
                }
            }
        }
        (void)cx;
    }
}
