/* test_render.c - grid operations and ASCII rasterizer smoke tests. */
#include "unity.h"

#include "raster3d.h"
#include "grid.h"
#include "../sim/level.h"
#include "../sim/shipgen.h"
#include <string.h>

static FrGrid grid;
static FrLevel level;

void setUp(void)
{
    fr_grid_init(&grid, FR_GRID_W, FR_GRID_H);
    memset(&level, 0, sizeof(level));
    level.w = 16;
    level.h = 16;
    memset(level.tiles, FR_T_WALL, sizeof(level.tiles));
    for (int y = 4; y < 12; y++)
        for (int x = 4; x < 12; x++) level.tiles[y * level.w + x] = FR_T_FLOOR;
}

void tearDown(void) {}

void test_grid_puts_and_bounds(void)
{
    fr_grid_puts(&grid, 0, 0, "HELLO", FR_WHITE, FR_BLACK);
    TEST_ASSERT_EQUAL_UINT8('H', grid.glyph[0]);
    fr_grid_putc(&grid, -1, -1, 'X', FR_WHITE, FR_BLACK); /* no crash */
    fr_grid_putc(&grid, FR_GRID_W, 0, 'X', FR_WHITE, FR_BLACK);
    TEST_ASSERT_EQUAL_UINT8('H', grid.glyph[0]);
}

void test_grid_dump_matches_dims(void)
{
    static char buf[FR_GRID_W * (FR_GRID_H + 1) + 16];
    fr_grid_dump(&grid, buf, sizeof(buf));
    int rows = 0, cols = 0, maxcols = 0;
    for (const char *p = buf; *p; p++) {
        if (*p == '\n') {
            rows++;
            if (cols > maxcols) maxcols = cols;
            cols = 0;
        } else {
            cols++;
        }
    }
    TEST_ASSERT_EQUAL_INT(FR_GRID_H, rows);
    TEST_ASSERT_EQUAL_INT(FR_GRID_W, maxcols);
}

void test_ramp_is_ordered(void)
{
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_NOT_EQUAL(FR_RAMP[i], FR_RAMP[i + 1]);
    }
    TEST_ASSERT_EQUAL_UINT8(' ', FR_RAMP[0]);
    TEST_ASSERT_EQUAL_UINT8('@', FR_RAMP[9]);
}

void test_raster_center_wall(void)
{
    /* Camera at floor center looking east: a wall must appear ahead. */
    FrCamera cam = {8.0f, 8.0f, 0.0f, 0.0f, 0.5f};
    FrViewStyle style = {FR_DGRAY, FR_DGRAY, 24.0f};
    fr_raster_world(&grid, &level, &cam, &style);
    /* Center column must show wall glyphs, not spaces. */
    int mid_col = grid.w / 2;
    int wall_cells = 0;
    for (int y = 0; y < grid.h; y++) {
        if (grid.glyph[y * grid.w + mid_col] != ' ') wall_cells++;
    }
    TEST_ASSERT_TRUE(wall_cells > grid.h / 4);
}

void test_raster_look_at_floor(void)
{
    FrCamera cam = {8.0f, 8.0f, 0.0f, 30.0f, 0.5f}; /* pitch down */
    FrViewStyle style = {FR_DGRAY, FR_DGRAY, 24.0f};
    fr_raster_world(&grid, &level, &cam, &style);
    /* Bottom rows should be floor (non-space after ramp). */
    int floor_cells = 0;
    for (int y = grid.h - 10; y < grid.h; y++)
        for (int x = 0; x < grid.w; x++)
            if (grid.glyph[y * grid.w + x] != ' ') floor_cells++;
    TEST_ASSERT_TRUE(floor_cells > 100);
}

void test_raster_render_is_deterministic(void)
{
    FrShipMap ship;
    fr_shipgen_generate(&ship, 31);
    FrCamera cam = {56.0f, 32.0f, 0.7f, 4.0f, 0.5f};
    FrViewStyle style = {FR_DGRAY, FR_DGRAY, 22.0f};
    FrGrid g2;
    fr_grid_init(&g2, FR_GRID_W, FR_GRID_H);
    fr_raster_world(&grid, &ship.decks[0], &cam, &style);
    fr_raster_world(&g2, &ship.decks[0], &cam, &style);
    TEST_ASSERT_EQUAL_MEMORY(grid.glyph, g2.glyph, FR_GRID_CELLS);
}

void test_door_opens_near_player(void)
{
    uint8_t out[FR_LEVEL_CELLS];
    level.tiles[8 * level.w + 9] = FR_T_DOOR; /* east of room center */
    fr_level_render_map(&level, 8.0f, 8.0f, 2.5f, out);
    TEST_ASSERT_EQUAL_UINT8(FR_T_FLOOR_HALL, out[8 * level.w + 9]);
    fr_level_render_map(&level, 0.5f, 0.5f, 2.5f, out);
    TEST_ASSERT_EQUAL_UINT8(FR_T_DOOR, out[8 * level.w + 9]);
}

void test_sprite_visible_in_room(void)
{
    FrSprite s = {10.5f, 8.5f, 'H', FR_BROWN, 1.0f, 0.0f};
    level.sprites[0] = s;
    level.sprite_count = 1;
    FrCamera cam = {8.0f, 8.0f, 0.0f, 0.0f, 0.5f};
    FrViewStyle style = {FR_DGRAY, FR_DGRAY, 24.0f};
    fr_raster_world(&grid, &level, &cam, &style);
    int found = 0;
    for (int i = 0; i < FR_GRID_CELLS; i++)
        if (grid.glyph[i] == 'H') found++;
    TEST_ASSERT_TRUE(found > 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_grid_puts_and_bounds);
    RUN_TEST(test_grid_dump_matches_dims);
    RUN_TEST(test_ramp_is_ordered);
    RUN_TEST(test_raster_center_wall);
    RUN_TEST(test_raster_look_at_floor);
    RUN_TEST(test_raster_render_is_deterministic);
    RUN_TEST(test_door_opens_near_player);
    RUN_TEST(test_sprite_visible_in_room);
    return UNITY_END();
}
