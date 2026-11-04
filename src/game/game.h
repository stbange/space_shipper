/* game.h - game logic layer: platform independent, testable headless.

   Owns the world state, the ship map, UI states and the ASCII rendering of
   every screen into an FrGrid. The platform layer (SDL) only collects input
   and blits the grid.
*/
#ifndef FR_GAME_H
#define FR_GAME_H

#include <stddef.h>

#include "../render/grid.h"
#include "../render/raster3d.h"
#include "../sim/level.h"
#include "../sim/shipgen.h"
#include "../sim/world.h"
#include "../sim/save.h"

typedef enum {
    FR_UI_MENU = 0,
    FR_UI_WALK,
    FR_UI_TURBO,
    FR_UI_NAV,
    FR_UI_MARKET,
    FR_UI_CONTRACTS,
    FR_UI_FUEL,
    FR_UI_ENG,
    FR_UI_TRAVEL,
    FR_UI_DOCKING,
    FR_UI_GAMEOVER
} FrUiState;

typedef struct {
    /* held movement */
    uint8_t fwd, back, strafe_l, strafe_r, turn_l, turn_r, rot_l, rot_r;
    /* discrete presses this frame */
    uint8_t interact, escape, up, down, left, right, enter;
    uint8_t key_1, key_2, key_3, key_4, key_5;
    uint8_t buy, sell;
    float mouse_dx, mouse_dy;
} FrInput;

typedef struct {
    float x, y, vx, vy, angle;
    float pad_x, pad_y;
    int ticks;   /* sub-ticks elapsed */
    int failed;  /* collision flashes */
} FrDockState;

#define FR_LOG_LINES 4
#define FR_LOG_LEN 88

typedef struct {
    FrWorld world;
    FrShipMap ship_map;
    FrLevel station_map;
    int station_generated;

    FrPlayerState player; /* deck, on_station, pos, ui state (persisted) */
    FrUiState ui;
    int ui_cursor;
    int ui_sub;     /* secondary selection (commodity etc.) */
    int speed;      /* travel time compression: ticks per second */
    float tick_acc; /* fractional tick accumulator */
    int running;

    FrCamera cam;
    FrDockState dock;
    float dock_acc; /* fixed-step accumulator for docking */

    FrGrid grid;
    char log[FR_LOG_LINES][FR_LOG_LEN];
    int log_count;
    char save_path[256]; /* set by platform; autosave on dock when set */
} FrGame;

void fr_game_init(FrGame *g, uint64_t seed); /* enters menu */
void fr_game_new(FrGame *g, uint64_t seed);  /* starts a new run */
int fr_game_save(FrGame *g, const char *path);
int fr_game_load(FrGame *g, const char *path);

/* One frame of game logic. dt in seconds (fixed 1/60 recommended). */
void fr_game_update(FrGame *g, const FrInput *in, float dt);
/* Advance n world ticks through the game layer (arrival/docking events). */
void fr_game_tick_n(FrGame *g, int n);
/* Render current state into g->grid. */
void fr_game_render(FrGame *g);

/* Helpers used by the platform layer and tests. */
const FrLevel *fr_game_current_level(const FrGame *g);
FrLevel *fr_game_current_level_mut(FrGame *g);
void fr_game_log(FrGame *g, const char *msg);
const char *fr_ui_state_name(FrUiState s);

#endif
