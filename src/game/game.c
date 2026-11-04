/* game.c - game logic and ASCII screen rendering (platform independent). */
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WALK_SPEED 3.2f
#define TURN_SPEED 2.4f
#define PLAYER_RADIUS 0.25f
#define DEFAULT_SEED 20261104ULL

const char *fr_ui_state_name(FrUiState s)
{
    switch (s) {
    case FR_UI_MENU: return "MENU";
    case FR_UI_WALK: return "WALK";
    case FR_UI_TURBO: return "TURBO";
    case FR_UI_NAV: return "NAV";
    case FR_UI_MARKET: return "MARKET";
    case FR_UI_CONTRACTS: return "CONTRACTS";
    case FR_UI_FUEL: return "FUEL";
    case FR_UI_ENG: return "ENG";
    case FR_UI_TRAVEL: return "TRAVEL";
    case FR_UI_DOCKING: return "DOCKING";
    case FR_UI_GAMEOVER: return "GAMEOVER";
    }
    return "?";
}

const FrLevel *fr_game_current_level(const FrGame *g)
{
    if (g->player.on_station) return &g->station_map;
    if (g->player.deck < 0 || g->player.deck >= g->ship_map.deck_count)
        return &g->ship_map.decks[0];
    return &g->ship_map.decks[g->player.deck];
}

FrLevel *fr_game_current_level_mut(FrGame *g)
{
    return (FrLevel *)fr_game_current_level(g);
}

void fr_game_log(FrGame *g, const char *msg)
{
    if (g->log_count < FR_LOG_LINES) {
        snprintf(g->log[g->log_count], FR_LOG_LEN, "%s", msg);
        g->log_count++;
    } else {
        memmove(g->log[0], g->log[1], FR_LOG_LEN * (FR_LOG_LINES - 1));
        snprintf(g->log[FR_LOG_LINES - 1], FR_LOG_LEN, "%s", msg);
    }
    snprintf(g->world.message, sizeof(g->world.message), "%s", msg);
}

/* ---------- init / new / save ---------- */

void fr_game_init(FrGame *g, uint64_t seed)
{
    memset(g, 0, sizeof(*g));
    fr_grid_init(&g->grid, FR_GRID_W, FR_GRID_H);
    fr_shipgen_generate(&g->ship_map, DEFAULT_SEED);
    g->ui = FR_UI_MENU;
    g->ui_cursor = 0;
    g->running = 1;
    (void)seed;
}

void fr_game_new(FrGame *g, uint64_t seed)
{
    fr_world_init(&g->world, seed);
    fr_shipgen_generate(&g->ship_map, seed);
    g->station_generated = 0;
    g->player.deck = 1; /* habitation deck: captain's quarters */
    g->player.on_station = 0;
    g->player.docked = 1; /* starts docked at Meridian Station */
    const FrLevel *lv = &g->ship_map.decks[1];
    g->player.px = (float)lv->spawn_x + 0.5f;
    g->player.py = (float)lv->spawn_y + 0.5f;
    g->player.angle = 0.0f;
    g->player.ui_state = FR_UI_WALK;
    g->ui = FR_UI_WALK;
    g->ui_cursor = 0;
    g->speed = 16;
    g->tick_acc = 0.0f;
    g->dock_acc = 0.0f;
    g->cam.pitch = 0.0f;
    g->log_count = 0;
    fr_game_log(g, "You are the captain of the freighter ISV Meridian Haul.");
    fr_game_log(g, "Take the turbolift to the command deck to reach the bridge.");
}

int fr_game_save(FrGame *g, const char *path)
{
    g->player.ui_state = (int)g->ui;
    return fr_save_write_file(path, &g->world, &g->player);
}

int fr_game_load(FrGame *g, const char *path)
{
    FrWorld w;
    FrPlayerState p;
    if (fr_save_read_file(path, &w, &p) != 0) return -1;
    g->world = w;
    fr_shipgen_generate(&g->ship_map, w.seed);
    if (p.on_station && w.port >= 0) {
        fr_station_generate(&g->station_map, w.seed + (uint64_t)w.port + 77,
                            fr_ports()[w.port].name);
        g->station_generated = 1;
    }
    g->player = p;
    g->ui = (FrUiState)p.ui_state;
    if (g->ui == FR_UI_DOCKING) g->ui = FR_UI_WALK;
    g->ui_cursor = 0;
    g->log_count = 0;
    fr_game_log(g, "Save loaded.");
    return 0;
}

/* ---------- collision & interaction ---------- */

static uint8_t frame_map[FR_LEVEL_CELLS];

/* Build the door-opened tile map for this frame around the player. */
static void build_frame_map(FrGame *g)
{
    const FrLevel *lv = fr_game_current_level(g);
    fr_level_render_map(lv, g->player.px, g->player.py, 2.5f, frame_map);
}

static bool walkable_at(const FrLevel *lv, float x, float y)
{
    int tx = (int)x, ty = (int)y;
    if (tx < 0 || ty < 0 || tx >= lv->w || ty >= lv->h) return false;
    return fr_tile_walkable(frame_map[ty * lv->w + tx]);
}

static bool try_move(FrGame *g, float nx, float ny)
{
    const FrLevel *lv = fr_game_current_level(g);
    float dirs[4][2] = {{-PLAYER_RADIUS, 0}, {PLAYER_RADIUS, 0}, {0, -PLAYER_RADIUS}, {0, PLAYER_RADIUS}};
    for (int i = 0; i < 4; i++) {
        if (!walkable_at(lv, nx + dirs[i][0], ny + dirs[i][1])) return false;
    }
    g->player.px = nx;
    g->player.py = ny;
    return true;
}

typedef enum {
    FR_INTERACT_NONE = 0,
    FR_INTERACT_NAV,
    FR_INTERACT_ENG,
    FR_INTERACT_CARGO,
    FR_INTERACT_MARKET,
    FR_INTERACT_CONTRACT,
    FR_INTERACT_FUEL,
    FR_INTERACT_TURBO,
    FR_INTERACT_AIRLOCK
} FrInteract;

static FrInteract find_interact(FrGame *g)
{
    const FrLevel *lv = fr_game_current_level(g);
    int tx = (int)g->player.px, ty = (int)g->player.py;
    uint8_t here = fr_level_at(lv, tx, ty);
    if (here == FR_T_TURBO) return FR_INTERACT_TURBO;
    if (here == FR_T_AIRLOCK) return FR_INTERACT_AIRLOCK;
    /* Look ahead. */
    float dx = cosf(g->player.angle), dy = sinf(g->player.angle);
    for (float d = 0.6f; d <= 1.6f; d += 0.4f) {
        int ax = (int)(g->player.px + dx * d);
        int ay = (int)(g->player.py + dy * d);
        uint8_t t = fr_level_at(lv, ax, ay);
        switch (t) {
        case FR_T_CONSOLE_NAV: return FR_INTERACT_NAV;
        case FR_T_CONSOLE_ENG: return FR_INTERACT_ENG;
        case FR_T_CONSOLE_CARGO: return FR_INTERACT_CARGO;
        case FR_T_TERMINAL_MARKET: return FR_INTERACT_MARKET;
        case FR_T_TERMINAL_CONTRACT: return FR_INTERACT_CONTRACT;
        case FR_T_TERMINAL_FUEL: return FR_INTERACT_FUEL;
        default: break;
        }
        /* Airlock ahead too (walking up the ramp). */
        if (t == FR_T_AIRLOCK) return FR_INTERACT_AIRLOCK;
    }
    return FR_INTERACT_NONE;
}

static void enter_station(FrGame *g)
{
    if (!g->station_generated && g->world.port >= 0) {
        fr_station_generate(&g->station_map, g->world.seed + (uint64_t)g->world.port + 77,
                            fr_ports()[g->world.port].name);
        g->station_generated = 1;
    }
    g->player.on_station = 1;
    const FrLevel *lv = &g->station_map;
    g->player.px = (float)lv->spawn_x + 0.5f;
    g->player.py = (float)lv->spawn_y + 0.5f;
    g->player.angle = (float)M_PI; /* facing north into the hall */
    fr_game_log(g, "You step through the airlock into the port concourse.");
}

static void return_to_ship(FrGame *g)
{
    g->player.on_station = 0;
    g->player.deck = 2; /* cargo deck where the ramp is */
    const FrLevel *lv = &g->ship_map.decks[2];
    g->player.px = (float)(lv->airlock_x - 2) + 0.5f;
    g->player.py = (float)lv->airlock_y + 0.5f;
    fr_game_log(g, "You return aboard the freighter.");
}

/* ---------- per-frame update ---------- */

static void handle_tick_events(FrGame *g, int ev)
{
    if (ev & FR_EV_DAY) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Day %d: wages, insurance and interest charged.",
                 fr_world_day(&g->world));
        fr_game_log(g, msg);
    }
    if (ev & FR_EV_ARRIVED) {
        /* Begin the docking sequence. */
        g->ui = FR_UI_DOCKING;
        g->dock.x = 30.0f;
        g->dock.y = 25.0f;
        g->dock.vx = 0.6f;
        g->dock.vy = 0.0f;
        g->dock.angle = 0.0f;
        g->dock.pad_x = 80.0f;
        g->dock.pad_y = 37.0f;
        g->dock.ticks = 0;
        g->dock.failed = 0;
        fr_game_log(g, "Orbit reached. Take the helm: dock the freighter.");
    }
    if (ev & FR_EV_BANKRUPT) {
        g->ui = FR_UI_GAMEOVER;
    }
}

static void advance_ticks(FrGame *g, float dt)
{
    if (!g->world.travel.active || g->speed <= 0) return;
    g->tick_acc += dt * (float)g->speed;
    while (g->tick_acc >= 1.0f) {
        g->tick_acc -= 1.0f;
        int ev = fr_world_tick(&g->world);
        handle_tick_events(g, ev);
        if (g->ui == FR_UI_DOCKING) {
            g->tick_acc = 0.0f;
            return;
        }
    }
}

void fr_game_tick_n(FrGame *g, int n)
{
    for (int i = 0; i < n; i++) {
        int ev = fr_world_tick(&g->world);
        handle_tick_events(g, ev);
        if (g->ui == FR_UI_DOCKING) return;
    }
}

static void update_walk(FrGame *g, const FrInput *in, float dt)
{
    build_frame_map(g);
    float turn = 0.0f;
    if (in->turn_l) turn -= 1.0f;
    if (in->turn_r) turn += 1.0f;
    g->player.angle += turn * TURN_SPEED * dt + in->mouse_dx * 0.0032f;
    g->cam.pitch -= in->mouse_dy * 0.55f;
    if (g->cam.pitch < -38.0f) g->cam.pitch = -38.0f;
    if (g->cam.pitch > 38.0f) g->cam.pitch = 38.0f;

    float mx = 0.0f, my = 0.0f;
    if (in->fwd) { mx += cosf(g->player.angle); my += sinf(g->player.angle); }
    if (in->back) { mx -= cosf(g->player.angle); my -= sinf(g->player.angle); }
    if (in->strafe_l) { mx += sinf(g->player.angle); my -= cosf(g->player.angle); }
    if (in->strafe_r) { mx -= sinf(g->player.angle); my += cosf(g->player.angle); }
    float len = sqrtf(mx * mx + my * my);
    if (len > 0.001f) {
        mx = mx / len * WALK_SPEED * dt;
        my = my / len * WALK_SPEED * dt;
        if (!try_move(g, g->player.px + mx, g->player.py)) try_move(g, g->player.px, g->player.py + my);
        else if (!try_move(g, g->player.px, g->player.py + my)) { /* x moved only */ }
    }

    if (in->escape) {
        g->ui = FR_UI_MENU;
        g->ui_cursor = 0;
    }
    if (in->interact) {
        FrInteract it = find_interact(g);
        switch (it) {
        case FR_INTERACT_NAV:
            if (g->world.travel.active) { g->ui = FR_UI_TRAVEL; g->ui_cursor = 0; }
            else { g->ui = FR_UI_NAV; g->ui_cursor = 0; }
            break;
        case FR_INTERACT_ENG: g->ui = FR_UI_ENG; g->ui_cursor = 0; break;
        case FR_INTERACT_CARGO:
        case FR_INTERACT_MARKET:
            if (g->player.docked) {
                g->ui = FR_UI_MARKET;
                g->ui_cursor = 0;
                g->ui_sub = 0;
            } else {
                fr_game_log(g, "Markets are only reachable while docked at a port.");
            }
            break;
        case FR_INTERACT_CONTRACT:
            if (g->player.docked) {
                if (g->world.port >= 0) fr_contracts_refresh(&g->world, g->world.port);
                g->ui = FR_UI_CONTRACTS;
                g->ui_cursor = 0;
            }
            break;
        case FR_INTERACT_FUEL:
            g->ui = FR_UI_FUEL;
            g->ui_cursor = 0;
            break;
        case FR_INTERACT_TURBO:
            g->ui = FR_UI_TURBO;
            g->ui_cursor = g->player.deck;
            break;
        case FR_INTERACT_AIRLOCK:
            if (g->player.on_station) {
                return_to_ship(g);
            } else if (g->player.docked) {
                enter_station(g);
            } else {
                fr_game_log(g, "The outer airlock is sealed while underway.");
            }
            break;
        default: break;
        }
    }
}

static void dock_complete(FrGame *g)
{
    g->player.docked = 1;
    g->ui = FR_UI_WALK;
    int port = g->world.port;
    fr_contracts_refresh(&g->world, port);
    fr_contracts_deliver(&g->world, port);
    char msg[96];
    snprintf(msg, sizeof(msg), "Docked at %s. Contracts settled.", fr_ports()[port].name);
    fr_game_log(g, msg);
    if (g->save_path[0]) fr_game_save(g, g->save_path);
}

static void update_docking(FrGame *g, const FrInput *in, float dt)
{
    g->dock_acc += dt;
    const float STEP = 1.0f / 60.0f;
    while (g->dock_acc >= STEP) {
        g->dock_acc -= STEP;
        FrDockState *d = &g->dock;
        d->ticks++;
        if (in->rot_l) d->angle -= 1.8f * STEP;
        if (in->rot_r) d->angle += 1.8f * STEP;
        float ax = 0.0f, ay = 0.0f;
        if (in->fwd) { ax += cosf(d->angle); ay += sinf(d->angle); }
        if (in->back) { ax -= cosf(d->angle); ay -= sinf(d->angle); }
        d->vx += ax * 6.0f * STEP;
        d->vy += ay * 6.0f * STEP;
        d->vx *= (1.0f - 0.25f * STEP);
        d->vy *= (1.0f - 0.25f * STEP);
        d->x += d->vx;
        d->y += d->vy;
        float dx = d->x - d->pad_x, dy = d->y - d->pad_y;
        float dist = sqrtf(dx * dx + dy * dy);
        float speed = sqrtf(d->vx * d->vx + d->vy * d->vy);
        if (dist < 3.0f && speed < 1.2f) {
            dock_complete(g);
            return;
        }
        if (d->x < 4 || d->x > 156 || d->y < 6 || d->y > 74) {
            d->x = 30.0f;
            d->y = 25.0f;
            d->vx = 0.6f;
            d->vy = 0.0f;
            d->failed++;
            g->world.ship.wear += 4;
            if (g->world.ship.wear > 100) g->world.ship.wear = 100;
            fr_game_log(g, "Hull scrape! Thrusters reset to approach start.");
        }
    }
}

/* Generic list menu helper: returns 1 when enter pressed. */
static bool menu_nav(const FrInput *in, int *cursor, int rows)
{
    if (in->up) (*cursor)--;
    if (in->down) (*cursor)++;
    if (*cursor < 0) *cursor = rows - 1;
    if (*cursor >= rows) *cursor = 0;
    return in->enter != 0;
}

static void update_menu(FrGame *g, const FrInput *in)
{
    if (menu_nav(in, &g->ui_cursor, 3)) {
        switch (g->ui_cursor) {
        case 0:
            fr_game_new(g, DEFAULT_SEED);
            break;
        case 1:
            if (g->save_path[0] && fr_game_load(g, g->save_path) == 0) {
                /* loaded */
            } else {
                fr_game_log(g, "No save game found.");
            }
            break;
        case 2:
            g->running = 0;
            break;
        }
    }
}

static void update_turbo(FrGame *g, const FrInput *in)
{
    if (menu_nav(in, &g->ui_cursor, g->ship_map.deck_count)) {
        const FrLevel *lv = &g->ship_map.decks[g->ui_cursor];
        g->player.deck = g->ui_cursor;
        g->player.px = (float)lv->turbo_x + 1.5f;
        g->player.py = (float)lv->turbo_y + 0.5f;
        g->player.angle = 0.0f;
        char msg[64];
        snprintf(msg, sizeof(msg), "Turbolift arrives: %s.", lv->name);
        fr_game_log(g, msg);
        g->ui = FR_UI_WALK;
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_nav(FrGame *g, const FrInput *in)
{
    int n = fr_port_count();
    if (menu_nav(in, &g->ui_cursor, n)) {
        int dest = g->ui_cursor;
        if (dest == g->world.port) {
            fr_game_log(g, "Already docked here.");
        } else {
            int rc = fr_travel_begin(&g->world, g->world.port, dest);
            if (rc == 0) {
                g->player.docked = 0;
                g->player.on_station = 0;
                g->ui = FR_UI_TRAVEL;
                g->ui_cursor = 0;
                fr_game_log(g, "Course plotted. Thrusters engaged.");
            } else if (rc == -3) {
                fr_game_log(g, "Not enough fuel for that leg. Refuel first.");
            }
        }
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_market(FrGame *g, const FrInput *in)
{
    int n = fr_commodity_count();
    if (in->up) g->ui_cursor = (g->ui_cursor + n - 1) % n;
    if (in->down) g->ui_cursor = (g->ui_cursor + 1) % n;
    int c = g->ui_cursor;
    if (in->buy) {
        int bought = fr_market_buy(&g->world, g->world.port, c, 10);
        if (bought == 0) fr_game_log(g, "Purchase failed (no stock, money or hold space).");
        else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Bought %d units.", bought);
            fr_game_log(g, msg);
        }
    }
    if (in->sell) {
        int earned = fr_market_sell(&g->world, g->world.port, c, 10);
        if (earned == 0) fr_game_log(g, "Nothing to sell.");
        else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Sold for %d cr.", earned);
            fr_game_log(g, msg);
        }
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_contracts(FrGame *g, const FrInput *in)
{
    int rows = 0;
    int map[FR_MAX_CONTRACTS];
    for (int i = 0; i < g->world.contract_count; i++) {
        if (g->world.contracts[i].state == 0 || g->world.contracts[i].state == 1) {
            map[rows++] = i;
        }
    }
    if (rows == 0) {
        if (in->escape || in->enter) g->ui = FR_UI_WALK;
        return;
    }
    if (menu_nav(in, &g->ui_cursor, rows)) {
        FrContract *c = &g->world.contracts[map[g->ui_cursor]];
        if (c->state == 1) {
            fr_game_log(g, "Already under way.");
        } else {
            int rc = fr_contract_accept(&g->world, c->id);
            if (rc == 0) fr_game_log(g, "Contract signed; cargo loaded.");
            else fr_game_log(g, "Not enough hold space for this contract.");
        }
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_fuel(FrGame *g, const FrInput *in)
{
    if (menu_nav(in, &g->ui_cursor, 3)) {
        int units = g->ui_cursor == 0 ? 50 : (g->ui_cursor == 1 ? 200 : FR_FUEL_MAX);
        int spent = fr_refuel(&g->world, units);
        if (spent > 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Refueled: -%d cr.", spent);
            fr_game_log(g, msg);
        } else {
            fr_game_log(g, "Unable to refuel (no money or tanks full).");
        }
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_eng(FrGame *g, const FrInput *in)
{
    if (menu_nav(in, &g->ui_cursor, 3)) {
        switch (g->ui_cursor) {
        case 0: {
            int cost = fr_repair(&g->world, g->world.ship.wear);
            if (cost > 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Full refit: -%d cr.", cost);
                fr_game_log(g, msg);
            } else fr_game_log(g, "Nothing to repair.");
            break;
        }
        case 1: {
            int pts = g->world.ship.wear < 10 ? g->world.ship.wear : 10;
            int cost = fr_repair(&g->world, pts);
            if (cost > 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Partial repair: -%d cr.", cost);
                fr_game_log(g, msg);
            } else fr_game_log(g, "Nothing to repair.");
            break;
        }
        case 2: {
            int lvl = g->world.ship.engine_level;
            if (lvl < 4) {
                int cost = 8000 * lvl;
                if (g->world.ship.money >= cost) {
                    g->world.ship.money -= cost;
                    g->world.ship.engine_level = lvl + 1;
                    fr_game_log(g, "Engine refit complete. Thrust increased.");
                } else fr_game_log(g, "Engine refit costs too much.");
            } else fr_game_log(g, "Engines are already max refit.");
            break;
        }
        }
    }
    if (in->escape) g->ui = FR_UI_WALK;
}

static void update_travel(FrGame *g, const FrInput *in)
{
    if (in->key_1) g->speed = 0;
    if (in->key_2) g->speed = 1;
    if (in->key_3) g->speed = 4;
    if (in->key_4) g->speed = 16;
    if (in->key_5) g->speed = 64;
    if (in->escape) g->ui = FR_UI_WALK;
}

void fr_game_update(FrGame *g, const FrInput *in, float dt)
{
    switch (g->ui) {
    case FR_UI_MENU: update_menu(g, in); break;
    case FR_UI_WALK: update_walk(g, in, dt); break;
    case FR_UI_TURBO: update_turbo(g, in); break;
    case FR_UI_NAV: update_nav(g, in); break;
    case FR_UI_MARKET: update_market(g, in); break;
    case FR_UI_CONTRACTS: update_contracts(g, in); break;
    case FR_UI_FUEL: update_fuel(g, in); break;
    case FR_UI_ENG: update_eng(g, in); break;
    case FR_UI_TRAVEL: update_travel(g, in); break;
    case FR_UI_DOCKING: update_docking(g, in, dt); break;
    case FR_UI_GAMEOVER:
        if (in->enter) {
            g->ui = FR_UI_MENU;
            g->ui_cursor = 0;
        }
        break;
    }
    /* Sim always advances while a leg is under way (unless docking). */
    if (g->ui != FR_UI_DOCKING) advance_ticks(g, dt);
}

/* ---------- rendering ---------- */

static void render_walk(FrGame *g)
{
    const FrLevel *lv = fr_game_current_level(g);
    FrCamera cam;
    cam.x = g->player.px;
    cam.y = g->player.py;
    cam.angle = g->player.angle;
    cam.pitch = g->cam.pitch;
    cam.eye_z = 0.5f;
    FrViewStyle style;
    style.floor_color = FR_DGRAY;
    style.ceil_color = FR_DGRAY;
    style.max_dist = 22.0f;
    fr_raster_world(&g->grid, lv, &cam, &style);

    /* HUD: top bar. */
    char line[96];
    const FrWorld *w = &g->world;
    snprintf(line, sizeof(line), " %s | Day %d %02d:%02d", lv->name,
             fr_world_day(w), fr_world_hour(w), fr_world_minute(w));
    fr_grid_fill_rect(&g->grid, 0, 0, g->grid.w, 1, ' ', FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 0, 0, line, FR_LCYAN, FR_BLACK);
    if (w->travel.active) {
        snprintf(line, sizeof(line), "IN TRANSIT -> %s ", fr_ports()[w->travel.to_port].name);
        fr_grid_puts(&g->grid, g->grid.w - 40, 0, line, FR_LYELLOW, FR_BLACK);
    } else if (w->port >= 0) {
        snprintf(line, sizeof(line), "DOCKED: %s ", fr_ports()[w->port].name);
        fr_grid_puts(&g->grid, g->grid.w - 40, 0, line, FR_LGREEN, FR_BLACK);
    }

    /* Bottom status bar. */
    int by = g->grid.h - 1;
    fr_grid_fill_rect(&g->grid, 0, by, g->grid.w, 1, ' ', FR_WHITE, FR_BLACK);
    snprintf(line, sizeof(line), " CR %d | Fuel %d | Wear %d%% | Hold %d/%d | Loan %d ",
             w->ship.money, w->ship.fuel, w->ship.wear, fr_hold_used(&w->ship),
             FR_HOLD_CAP, w->ship.loan);
    fr_grid_puts(&g->grid, 0, by, line, FR_WHITE, FR_BLACK);

    /* Message log above the status bar. */
    for (int i = 0; i < g->log_count; i++) {
        int ly = by - 1 - (g->log_count - 1 - i);
        fr_grid_puts(&g->grid, 1, ly, g->log[i], FR_LGRAY, FR_BLACK);
    }

    /* Interact prompt. */
    FrInteract it = find_interact(g);
    if (it != FR_INTERACT_NONE && it != FR_INTERACT_AIRLOCK) {
        const char *n = "?";
        switch (it) {
        case FR_INTERACT_NAV: n = "Navigation console"; break;
        case FR_INTERACT_ENG: n = "Engineering console"; break;
        case FR_INTERACT_CARGO: n = "Cargo control console"; break;
        case FR_INTERACT_MARKET: n = "Commodity exchange"; break;
        case FR_INTERACT_CONTRACT: n = "Contract board"; break;
        case FR_INTERACT_FUEL: n = "Fuel depot"; break;
        case FR_INTERACT_TURBO: n = "Turbolift"; break;
        default: break;
        }
        char prompt[80];
        snprintf(prompt, sizeof(prompt), "[E] %s", n);
        fr_grid_puts(&g->grid, g->grid.w / 2 - 20, by - 3, prompt, FR_LYELLOW, FR_BLACK);
    } else if (it == FR_INTERACT_AIRLOCK) {
        const char *n = g->player.on_station ? "Board the freighter" : "Pass through airlock";
        char prompt[80];
        snprintf(prompt, sizeof(prompt), "[E] %s", n);
        fr_grid_puts(&g->grid, g->grid.w / 2 - 20, by - 3, prompt, FR_LYELLOW, FR_BLACK);
    }
    fr_grid_puts(&g->grid, 2, 1, "WASD move  arrows turn  mouse look  E interact  Esc menu",
                 FR_DGRAY, FR_BLACK);
}

static void panel(FrGame *g, int x, int y, int w, int h, const char *title)
{
    fr_grid_fill_rect(&g->grid, x, y, w, h, ' ', FR_WHITE, FR_BLACK);
    fr_grid_box(&g->grid, x, y, w, h, FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, x + 2, y, title, FR_LCYAN, FR_BLACK);
}

static void render_nav(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 4, 2, 120, 40, "NAVIGATION - SYSTEM CHART");
    int y = 5;
    fr_grid_puts(&g->grid, 6, y, "PORT                 TYPE      POS        DIST   DAYS   FUEL   STATUS",
                 FR_LGRAY, FR_BLACK);
    y += 2;
    for (int i = 0; i < fr_port_count(); i++) {
        const FrPortDef *p = &fr_ports()[i];
        int dist = i == g->world.port ? 0 : fr_port_distance(g->world.port, i);
        int days = dist * 100 / fr_ship_speed(&g->world.ship);
        int fuel = fr_travel_fuel(&g->world, g->world.port, i);
        const char *status = "available";
        if (i == g->world.port) status = "DOCKED HERE";
        char row[110];
        snprintf(row, sizeof(row), "%-20s %-9s (%4d,%4d)  %2d.%d AU %3d.%d  %4d   %s",
                 p->name, p->type == FR_PORT_PLANET ? "planet" : "orbital",
                 p->x, p->y, dist / 10, dist % 10, days / 10, days % 10, fuel, status);
        uint8_t fg = i == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = i == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 5, y + i * 2, 118, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 6, y + i * 2, row, fg, bg);
    }
    int base = y + fr_port_count() * 2 + 2;
    fr_grid_puts(&g->grid, 6, base, "Engines at refit level 1; travel burns fuel per AU and accrues wear.",
                 FR_DGRAY, FR_BLACK);
    fr_grid_puts(&g->grid, 6, base + 2, "[UP/DOWN] select port   [ENTER] plot course & depart   [ESC] close",
                 FR_LGRAY, FR_BLACK);
    char stat[96];
    snprintf(stat, sizeof(stat), "CR %d | Fuel %d/%d", g->world.ship.money,
             g->world.ship.fuel, FR_FUEL_MAX);
    fr_grid_puts(&g->grid, 6, base + 4, stat, FR_WHITE, FR_BLACK);
}

static void render_travel(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    const FrTravel *t = &g->world.travel;
    panel(g, 20, 20, 120, 30, "BRIDGE - UNDER WAY");
    int pct = t->ticks_total > 0 ? t->ticks_done * 100 / t->ticks_total : 0;
    char bar[64];
    int nb = 40 * pct / 100;
    for (int i = 0; i < 40; i++) bar[i] = (i < nb) ? '#' : '-';
    bar[40] = 0;
    char line[96];
    snprintf(line, sizeof(line), "Destination: %s", fr_ports()[t->to_port].name);
    fr_grid_puts(&g->grid, 24, 24, line, FR_LCYAN, FR_BLACK);
    snprintf(line, sizeof(line), "Progress    : [%s] %d%%", bar, pct);
    fr_grid_puts(&g->grid, 24, 26, line, FR_LGREEN, FR_BLACK);
    int rem = t->ticks_total - t->ticks_done;
    snprintf(line, sizeof(line), "Remaining   : %d.%d days (%d ticks)", rem / 240, (rem % 240) / 24,
             rem);
    fr_grid_puts(&g->grid, 24, 28, line, FR_LGRAY, FR_BLACK);
    snprintf(line, sizeof(line), "Fuel %d | Wear %d%% | CR %d", g->world.ship.fuel,
             g->world.ship.wear, g->world.ship.money);
    fr_grid_puts(&g->grid, 24, 30, line, FR_LGRAY, FR_BLACK);
    const char *spd = g->speed == 0 ? "PAUSED" : (g->speed == 1 ? "1x (real time)" :
                        (g->speed == 4 ? "4x" : (g->speed == 16 ? "16x" : "64x")));
    snprintf(line, sizeof(line), "Time        : %s", spd);
    fr_grid_puts(&g->grid, 24, 32, line, FR_LYELLOW, FR_BLACK);
    fr_grid_puts(&g->grid, 24, 36, "[1..5] time warp   [ESC] leave bridge (ship keeps flying)",
                 FR_LGRAY, FR_BLACK);
}

static void render_market(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 4, 2, 130, 44, "COMMODITY EXCHANGE");
    int port = g->world.port;
    char line[110];
    snprintf(line, sizeof(line), "Port: %s", fr_ports()[port].name);
    fr_grid_puts(&g->grid, 6, 4, line, FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 6, 6, "COMMODITY    PRICE  STOCK   HOLD   TREND", FR_LGRAY, FR_BLACK);
    for (int c = 0; c < fr_commodity_count(); c++) {
        int price = fr_market_price(&g->world, port, c);
        int base = fr_commodities()[c].base_price;
        const char *trend = price > base ? "rising" : (price < base ? "falling" : "steady");
        uint8_t fg = c == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = c == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        snprintf(line, sizeof(line), "%-12s %4d   %4d   %4d   %s",
                 fr_commodities()[c].name, price, g->world.market_stock[port][c],
                 g->world.ship.hold[c], trend);
        fr_grid_fill_rect(&g->grid, 5, 8 + c * 2, 128, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 6, 8 + c * 2, line, fg, bg);
    }
    snprintf(line, sizeof(line), "CR %d | Hold %d/%d", g->world.ship.money,
             fr_hold_used(&g->world.ship), FR_HOLD_CAP);
    fr_grid_puts(&g->grid, 6, 26, line, FR_WHITE, FR_BLACK);
    fr_grid_puts(&g->grid, 6, 29, "[UP/DOWN] commodity   [B] buy 10   [S] sell 10   [ESC] close",
                 FR_LGRAY, FR_BLACK);
}

static void render_contracts(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 4, 2, 140, 46, "CONTRACT BOARD");
    char line[130];
    fr_grid_puts(&g->grid, 6, 5, "COMMODITY    UNITS  DEST               REWARD   DEADLINE      STATUS",
                 FR_LGRAY, FR_BLACK);
    int row = 0;
    for (int i = 0; i < g->world.contract_count; i++) {
        const FrContract *c = &g->world.contracts[i];
        if (c->state != 0 && c->state != 1) continue;
        const char *st = c->state == 1 ? "ACCEPTED" : "offered";
        snprintf(line, sizeof(line), "%-12s %5d  %-18s %6d   day %3d       %s",
                 fr_commodities()[c->commodity].name, c->units,
                 fr_ports()[c->dest_port].name, c->reward, c->deadline_day, st);
        uint8_t fg = row == g->ui_cursor ? FR_LYELLOW : (c->state == 1 ? FR_LGREEN : FR_LGRAY);
        uint8_t bg = row == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 5, 7 + row * 2, 138, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 6, 7 + row * 2, line, fg, bg);
        row++;
    }
    fr_grid_puts(&g->grid, 6, 7 + row * 2 + 2,
                 "[UP/DOWN] select   [ENTER] accept (cargo loads here)   [ESC] close",
                 FR_LGRAY, FR_BLACK);
}

static void render_fuel(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 30, 25, 70, 16, "FUEL DEPOT");
    char line[80];
    snprintf(line, sizeof(line), "Tanks: %d / %d units    Price: %d cr/unit", g->world.ship.fuel,
             FR_FUEL_MAX, FR_FUEL_PRICE);
    fr_grid_puts(&g->grid, 33, 29, line, FR_LGRAY, FR_BLACK);
    const char *opts[3] = {"Refuel 50 units", "Refuel 200 units", "Fill tanks"};
    for (int i = 0; i < 3; i++) {
        uint8_t fg = i == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = i == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 32, 32 + i, 50, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 33, 32 + i, opts[i], fg, bg);
    }
    fr_grid_puts(&g->grid, 33, 37, "[UP/DOWN] select   [ENTER] refuel   [ESC] close", FR_LGRAY,
                 FR_BLACK);
}

static void render_eng(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 20, 15, 90, 26, "ENGINEERING");
    char line[96];
    snprintf(line, sizeof(line), "Hull wear      : %d%%", g->world.ship.wear);
    fr_grid_puts(&g->grid, 23, 19, line, FR_LGRAY, FR_BLACK);
    snprintf(line, sizeof(line), "Engine refit   : level %d (%d AU/day)", g->world.ship.engine_level,
             fr_ship_speed(&g->world.ship) / 100);
    fr_grid_puts(&g->grid, 23, 21, line, FR_LGRAY, FR_BLACK);
    snprintf(line, sizeof(line), "Daily upkeep   : %d cr + wear maintenance",
             FR_DAILY_WAGES + FR_DAILY_INSURANCE);
    fr_grid_puts(&g->grid, 23, 23, line, FR_LGRAY, FR_BLACK);
    const char *opts[3];
    opts[0] = "Full refit (repair all wear)";
    opts[1] = "Partial repair (10 pts)";
    opts[2] = g->world.ship.engine_level < 4 ?
        "Engine refit +0.6 AU/day (8000 cr x level)" : "Engines maxed";
    for (int i = 0; i < 3; i++) {
        uint8_t fg = i == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = i == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 22, 26 + i, 86, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 23, 26 + i, opts[i], fg, bg);
    }
    snprintf(line, sizeof(line), "CR %d", g->world.ship.money);
    fr_grid_puts(&g->grid, 23, 31, line, FR_WHITE, FR_BLACK);
    fr_grid_puts(&g->grid, 23, 33, "[UP/DOWN] select   [ENTER] order   [ESC] close", FR_LGRAY,
                 FR_BLACK);
}

static void render_turbo(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    panel(g, 45, 30, 60, 14, "TURBOLIFT");
    for (int i = 0; i < g->ship_map.deck_count; i++) {
        uint8_t fg = i == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = i == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 47, 33 + i * 2, 56, 1, ' ', fg, bg);
        char row[60];
        snprintf(row, sizeof(row), "Deck %d: %s", i, g->ship_map.decks[i].name);
        fr_grid_puts(&g->grid, 48, 33 + i * 2, row, fg, bg);
    }
    fr_grid_puts(&g->grid, 48, 33 + g->ship_map.deck_count * 2 + 1,
                 "[UP/DOWN] deck   [ENTER] travel   [ESC] close", FR_LGRAY, FR_BLACK);
}

static void render_docking(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    const FrDockState *d = &g->dock;
    /* Starfield. */
    for (int i = 0; i < 300; i++) {
        int x = (i * 7919) % g->grid.w;
        int y = (i * 104729) % (g->grid.h - 10) + 4;
        fr_grid_putc(&g->grid, x, y, '.', FR_DGRAY, FR_BLACK);
    }
    /* Field border. */
    fr_grid_box(&g->grid, 2, 4, 156, 72, FR_DGRAY, FR_BLACK);
    /* Dock pad with guide rings. */
    int px = (int)d->pad_x, py = (int)d->pad_y;
    fr_grid_putc(&g->grid, px, py, 'O', FR_LGREEN, FR_BLACK);
    fr_grid_putc(&g->grid, px - 1, py, '[', FR_GREEN, FR_BLACK);
    fr_grid_putc(&g->grid, px + 1, py, ']', FR_GREEN, FR_BLACK);
    fr_grid_putc(&g->grid, px, py - 1, 'I', FR_GREEN, FR_BLACK);
    fr_grid_putc(&g->grid, px, py + 1, 'I', FR_GREEN, FR_BLACK);
    for (int i = 0; i < 20; i++) {
        float a = (float)i / 20.0f * (float)M_PI * 2.0f;
        int gx = px + (int)(cosf(a) * 8.0f);
        int gy = py + (int)(sinf(a) * 8.0f * 0.9f);
        fr_grid_putc(&g->grid, gx, gy, '.', FR_DGRAY, FR_BLACK);
    }
    /* Ship: glyph pointing. */
    int sx = (int)d->x, sy = (int)d->y;
    fr_grid_putc(&g->grid, sx, sy, '@', FR_LYELLOW, FR_BLACK);
    float ang = d->angle;
    for (float r = 1.0f; r <= 3.0f; r += 1.0f) {
        int hx = sx + (int)(cosf(ang) * r);
        int hy = sy + (int)(sinf(ang) * r * 0.9f);
        fr_grid_putc(&g->grid, hx, hy, '-', FR_LGRAY, FR_BLACK);
    }
    /* HUD. */
    char line[96];
    float dx = d->x - d->pad_x, dy = d->y - d->pad_y;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = sqrtf(d->vx * d->vx + d->vy * d->vy);
    snprintf(line, sizeof(line), " DOCKING AT %s", fr_ports()[g->world.travel.to_port].name);
    fr_grid_fill_rect(&g->grid, 0, 0, g->grid.w, 1, ' ', FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 0, 0, line, FR_LCYAN, FR_BLACK);
    snprintf(line, sizeof(line), " Range %4.1f   Speed %4.1f   Align %3d deg", dist, speed,
             (int)(ang * 180.0f / (float)M_PI) % 360);
    fr_grid_fill_rect(&g->grid, 0, g->grid.h - 2, g->grid.w, 1, ' ', FR_WHITE, FR_BLACK);
    fr_grid_puts(&g->grid, 0, g->grid.h - 2, line, FR_WHITE, FR_BLACK);
    fr_grid_puts(&g->grid, 0, g->grid.h - 1,
                 " W/S thrust   A/D rotate   ease onto pad O at speed < 1.2", FR_LGRAY, FR_BLACK);
}

static void render_menu(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    for (int i = 0; i < 500; i++) {
        int x = (i * 7919) % g->grid.w;
        int y = (i * 104729) % g->grid.h;
        fr_grid_putc(&g->grid, x, y, '.', FR_DGRAY, FR_BLACK);
    }
    fr_grid_puts(&g->grid, 40, 20, "  ____  ____ ____    ____ ____ _  _ ____ _  _ ___", FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 40, 21, " [  \\/ |___ |___    |___ | __ |\\/| |  | |  |  / ", FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 40, 22, " [_/\\| |___ |___    |    |__] |  | |__| |__| /_ ", FR_LCYAN, FR_BLACK);
    fr_grid_puts(&g->grid, 40, 24, "  a captain's life aboard a city-sized freighter", FR_LGRAY, FR_BLACK);
    const char *opts[3] = {"New Game", "Load Game", "Quit"};
    for (int i = 0; i < 3; i++) {
        uint8_t fg = i == g->ui_cursor ? FR_LYELLOW : FR_LGRAY;
        uint8_t bg = i == g->ui_cursor ? FR_DGRAY : FR_BLACK;
        fr_grid_fill_rect(&g->grid, 70, 30 + i * 2, 20, 1, ' ', fg, bg);
        fr_grid_puts(&g->grid, 71, 30 + i * 2, opts[i], fg, bg);
    }
    fr_grid_puts(&g->grid, 58, 40, "[UP/DOWN] select   [ENTER] confirm", FR_DGRAY, FR_BLACK);
}

static void render_gameover(FrGame *g)
{
    fr_grid_clear(&g->grid, FR_BLACK);
    fr_grid_puts(&g->grid, 60, 30, "BANKRUPTCY", FR_LRED, FR_BLACK);
    char line[96];
    snprintf(line, sizeof(line), "The crew is unpaid and the banks have seized the freighter on day %d.",
             fr_world_day(&g->world));
    fr_grid_puts(&g->grid, 34, 34, line, FR_LGRAY, FR_BLACK);
    snprintf(line, sizeof(line), "Final net worth: %d credits.", fr_money_total(&g->world));
    fr_grid_puts(&g->grid, 34, 36, line, FR_LGRAY, FR_BLACK);
    fr_grid_puts(&g->grid, 60, 40, "[ENTER] main menu", FR_LGRAY, FR_BLACK);
}

void fr_game_render(FrGame *g)
{
    switch (g->ui) {
    case FR_UI_MENU: render_menu(g); break;
    case FR_UI_WALK: render_walk(g); break;
    case FR_UI_TURBO: render_turbo(g); break;
    case FR_UI_NAV: render_nav(g); break;
    case FR_UI_MARKET: render_market(g); break;
    case FR_UI_CONTRACTS: render_contracts(g); break;
    case FR_UI_FUEL: render_fuel(g); break;
    case FR_UI_ENG: render_eng(g); break;
    case FR_UI_TRAVEL: render_travel(g); break;
    case FR_UI_DOCKING: render_docking(g); break;
    case FR_UI_GAMEOVER: render_gameover(g); break;
    }
}
