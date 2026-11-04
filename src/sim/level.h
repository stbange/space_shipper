/* level.h - tile-grid level shared by ship generation, walking and the renderer. */
#ifndef FR_LEVEL_H
#define FR_LEVEL_H

#include <stdint.h>
#include <stdbool.h>

#include "color.h"

#define FR_LEVEL_MAX_W 128
#define FR_LEVEL_MAX_H 80
#define FR_LEVEL_CELLS (FR_LEVEL_MAX_W * FR_LEVEL_MAX_H)
#define FR_MAX_DECKS 5

/* Tile values. 0-31 are floor-like (walkable), >=32 are solid walls. */
enum {
    FR_T_FLOOR = 0,      /* generic deck floor */
    FR_T_FLOOR_HALL = 1, /* corridor floor */
    FR_T_FLOOR_ROOM = 2, /* room floor */
    FR_T_TURBO = 3,      /* turbolift car (walkable, interact) */
    FR_T_AIRLOCK = 4,    /* docking ramp / airlock (walkable, interact) */
    FR_T_WALK_MAX = 31,

    FR_T_WALL = 32,       /* district-colored structural wall */
    FR_T_WALL_TECH = 33,  /* panelled wall (consoles, machinery) */
    FR_T_DOOR = 34,       /* auto door (solid until near) */
    FR_T_WINDOW = 35,     /* viewport window wall */
    FR_T_CONSOLE_NAV = 36,
    FR_T_CONSOLE_ENG = 37,
    FR_T_CONSOLE_CARGO = 38,
    FR_T_TERMINAL_MARKET = 39,
    FR_T_TERMINAL_CONTRACT = 40,
    FR_T_TERMINAL_FUEL = 41,
    FR_T_SOLID_MAX = 255
};

typedef struct {
    float x, y;    /* world position in tile units (center of tile = +0.5) */
    char glyph;    /* ASCII glyph drawn */
    uint8_t color; /* RGB332 */
    float scale;   /* sprite size in world units (1.0 = one tile) */
    float v_offset; /* vertical offset from floor, in world units */
} FrSprite;

#define FR_LEVEL_MAX_SPRITES 256

typedef struct {
    int w, h;
    uint8_t tiles[FR_LEVEL_CELLS];
    char name[32];
    FrSprite sprites[FR_LEVEL_MAX_SPRITES];
    int sprite_count;
    /* Points of interest. */
    int spawn_x, spawn_y;
    int turbo_x, turbo_y;      /* -1 if none */
    int airlock_x, airlock_y;  /* -1 if none */
} FrLevel;

static inline bool fr_tile_walkable(uint8_t t)
{
    return t < FR_T_WALK_MAX;
}

static inline uint8_t fr_level_at(const FrLevel *lv, int x, int y)
{
    if (x < 0 || y < 0 || x >= lv->w || y >= lv->h) return FR_T_WALL;
    return lv->tiles[y * lv->w + x];
}

static inline void fr_level_set(FrLevel *lv, int x, int y, uint8_t t)
{
    if (x < 0 || y < 0 || x >= lv->w || y >= lv->h) return;
    lv->tiles[y * lv->w + x] = t;
}

static inline int fr_level_idx(const FrLevel *lv, int x, int y)
{
    return y * lv->w + x;
}

/* Wall appearance lookup: returns RGB332 base color + brightness boost for a
 * wall tile type. */
uint8_t fr_tile_wall_color(uint8_t t);
float fr_tile_wall_boost(uint8_t t);

#endif
