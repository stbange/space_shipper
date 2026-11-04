/* raster3d.h - software 3D rasterizer into the ASCII character grid.

   Wolfenstein-style per-column raycasting over a tile level, with
   perspective floor/ceiling rows and billboard sprites. Output is purely
   character cells + RGB332 colors + per-cell depth, so it is fully
   deterministic and golden-frame testable.
*/
#ifndef FR_RASTER3D_H
#define FR_RASTER3D_H

#include "grid.h"
#include "../sim/level.h"

typedef struct {
    float x, y;    /* position in tile units */
    float angle;   /* yaw, radians */
    float pitch;   /* vertical look offset in grid rows */
    float eye_z;   /* eye height 0..1 of wall height (default 0.5) */
} FrCamera;

typedef struct {
    uint8_t floor_color; /* RGB332 base colors */
    uint8_t ceil_color;
    float max_dist;      /* distance at which shading fades out */
} FrViewStyle;

/* Render the level view into the grid. Grid is not cleared first; the
 * renderer covers every cell. */
void fr_raster_world(FrGrid *grid, const FrLevel *lv, const FrCamera *cam,
                     const FrViewStyle *style);

/* 10-level ASCII brightness ramp. */
extern const char FR_RAMP[11];

/* Compute the open/visible tile map used for rendering & collision: doors
 * within `open_dist` tiles of the player become floor. Writes into out
 * (must be FR_LEVEL_CELLS bytes). */
void fr_level_render_map(const FrLevel *lv, float px, float py, float open_dist,
                          uint8_t *out);

#endif
