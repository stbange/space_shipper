/* shipgen.c - deterministic deck/district generation.

   The freighter is generated as a grid of corridor lanes with carved rooms
   per district plus handcrafted anchor areas (bridge, engineering, cargo
   bays, turbolift, airlock). Everything is driven by the seeded RNG so the
   ship is identical for a given seed, which makes it testable.
*/
#include "shipgen.h"

#include <stdio.h>
#include <string.h>

const char *fr_deck_names[FR_MAX_DECKS] = {
    "Command", "Habitation", "Cargo", "Engineering", "Hydroponics"
};

#define DECK_W 112
#define DECK_H 64

static void init_level(FrLevel *lv, int w, int h, const char *name)
{
    memset(lv, 0, sizeof(*lv));
    lv->w = w;
    lv->h = h;
    memset(lv->tiles, FR_T_WALL, (size_t)(w * h));
    lv->spawn_x = -1;
    lv->spawn_y = -1;
    lv->turbo_x = -1;
    lv->turbo_y = -1;
    lv->airlock_x = -1;
    lv->airlock_y = -1;
    snprintf(lv->name, sizeof(lv->name), "%s", name);
}

static void fill_rect(FrLevel *lv, int x, int y, int w, int h, uint8_t t)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            fr_level_set(lv, x + i, y + j, t);
}

static void add_sprite(FrLevel *lv, float x, float y, char glyph, uint8_t color, float scale)
{
    if (lv->sprite_count >= FR_LEVEL_MAX_SPRITES) return;
    FrSprite *s = &lv->sprites[lv->sprite_count++];
    s->x = x;
    s->y = y;
    s->glyph = glyph;
    s->color = color;
    s->scale = scale;
    s->v_offset = 0.0f;
}

/* Carve the common corridor skeleton shared by all decks: horizontal spine
 * plus vertical lanes, plus the turbolift room at the west end. */
static void carve_corridors(FrLevel *lv)
{
    fill_rect(lv, 12, 30, DECK_W - 18, 2, FR_T_FLOOR_HALL);
    static const int lanes[] = {20, 44, 68, 92};
    for (size_t i = 0; i < sizeof(lanes) / sizeof(lanes[0]); i++) {
        fill_rect(lv, lanes[i], 6, 2, DECK_H - 12, FR_T_FLOOR_HALL);
    }
    /* Turbolift room adjacent to spine west end. */
    fill_rect(lv, 6, 27, 6, 8, FR_T_FLOOR_ROOM);
    fr_level_set(lv, 8, 30, FR_T_TURBO);
    fr_level_set(lv, 8, 31, FR_T_TURBO);
    lv->turbo_x = 8;
    lv->turbo_y = 30;
    /* Hallway from lift room to spine. */
    fill_rect(lv, 11, 30, 1, 2, FR_T_FLOOR_HALL);
    /* Default spawn: just east of the lift on the spine. */
    lv->spawn_x = 12;
    lv->spawn_y = 31;
}

/* Block rectangles between the corridor lanes. */
typedef struct {
    int x, y, w, h;
} Rect;

static int room_blocks(Rect *out, int max)
{
    static const int xs[] = {7, 23, 47, 71, 95};
    static const int ws[] = {12, 20, 20, 20, 10};
    static const int ys[] = {7, 33};
    static const int hs[] = {22, 24};
    int n = 0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 5; i++) {
            if (n >= max) return n;
            out[n].x = xs[i];
            out[n].y = ys[j];
            out[n].w = ws[i];
            out[n].h = hs[j];
            n++;
        }
    return n;
}

/* Carve a room into a block: floor inset + door toward a corridor. Returns
 * the carved room rect or w==0 if skipped. */
static Rect carve_room(FrLevel *lv, FrRng *rng, const Rect *blk)
{
    Rect room = {0, 0, 0, 0};
    int pad = fr_rng_range(rng, 2, 3);
    int rx = blk->x + pad;
    int ry = blk->y + pad;
    int rw = blk->w - 2 * pad;
    int rh = blk->h - 2 * pad;
    if (rw < 3 || rh < 3) return room;
    fill_rect(lv, rx, ry, rw, rh, FR_T_FLOOR_ROOM);
    room.x = rx;
    room.y = ry;
    room.w = rw;
    room.h = rh;
    /* Door on a random side, at a position adjacent to a corridor. */
    int side = fr_rng_range(rng, 0, 3);
    if (side == 0) fr_level_set(lv, rx + rw / 2, ry - 1, FR_T_DOOR);
    else if (side == 1) fr_level_set(lv, rx + rw / 2, ry + rh, FR_T_DOOR);
    else if (side == 2) fr_level_set(lv, rx - 1, ry + rh / 2, FR_T_DOOR);
    else fr_level_set(lv, rx + rw, ry + rh / 2, FR_T_DOOR);
    return room;
}

static void sprinkle(FrLevel *lv, FrRng *rng, const Rect *room, char glyph, uint8_t color,
                     float scale, int max_n)
{
    int n = fr_rng_range(rng, 0, max_n);
    for (int i = 0; i < n; i++) {
        float px = (float)(room->x + fr_rng_range(rng, 0, room->w - 1)) + 0.5f;
        float py = (float)(room->y + fr_rng_range(rng, 0, room->h - 1)) + 0.5f;
        add_sprite(lv, px, py, glyph, color, scale);
    }
}

static void add_windows(FrLevel *lv, FrRng *rng)
{
    for (int x = 8; x < DECK_W - 8; x += 2) {
        if (fr_rng_next(rng) & 1) fr_level_set(lv, x, 2, FR_T_WINDOW);
        if (fr_rng_next(rng) & 1) fr_level_set(lv, x, DECK_H - 3, FR_T_WINDOW);
    }
}

/* ---- Deck 0: Command ---- */
static void gen_bridge_deck(FrLevel *lv, FrRng *rng)
{
    carve_corridors(lv);
    add_windows(lv, rng);
    /* Bridge chamber at the east end: wide room with viewport wall. */
    fill_rect(lv, DECK_W - 34, 10, 28, 44, FR_T_FLOOR_ROOM);
    fill_rect(lv, DECK_W - 7, 10, 4, 44, FR_T_WINDOW);   /* viewport strip */
    fill_rect(lv, DECK_W - 34, 8, 28, 2, FR_T_WALL_TECH);
    fill_rect(lv, DECK_W - 34, 54, 28, 2, FR_T_WALL_TECH);
    fill_rect(lv, DECK_W - 36, 10, 2, 44, FR_T_WALL_TECH);
    /* Connecting hall from spine into the bridge. */
    fill_rect(lv, DECK_W - 36, 30, 2, 2, FR_T_FLOOR_HALL);
    fill_rect(lv, DECK_W - 34, 30, 2, 2, FR_T_FLOOR_HALL);
    /* Console banks (solid, interact when near). */
    fill_rect(lv, DECK_W - 30, 14, 2, 6, FR_T_CONSOLE_NAV);
    fill_rect(lv, DECK_W - 30, 44, 2, 6, FR_T_CONSOLE_NAV);
    fill_rect(lv, DECK_W - 30, 27, 2, 10, FR_T_CONSOLE_NAV);
    /* Captain's chair + stations as sprites. */
    add_sprite(lv, DECK_W - 20 + 0.5f, 31.5f, 'T', FR_LMAGENTA, 1.2f);
    add_sprite(lv, DECK_W - 26 + 0.5f, 18.5f, 'o', FR_CYAN, 1.0f);
    add_sprite(lv, DECK_W - 26 + 0.5f, 44.5f, 'o', FR_CYAN, 1.0f);
    add_sprite(lv, DECK_W - 14 + 0.5f, 20.5f, 'o', FR_LBLUE, 1.0f);
    add_sprite(lv, DECK_W - 14 + 0.5f, 42.5f, 'o', FR_LBLUE, 1.0f);
    /* Officer rooms in remaining blocks. */
    Rect blocks[10];
    int n = room_blocks(blocks, 10);
    for (int i = 0; i < n; i++) {
        if (fr_rng_float01(rng) < 0.7f) {
            Rect room = carve_room(lv, rng, &blocks[i]);
            if (room.w > 0) sprinkle(lv, rng, &room, 'H', FR_BROWN, 1.0f, 2);
        }
    }
    lv->spawn_x = 8;
    lv->spawn_y = 29; /* next to the turbolift on the command deck */
}

/* ---- Deck 1: Habitation ---- */
static void gen_hab_deck(FrLevel *lv, FrRng *rng)
{
    carve_corridors(lv);
    add_windows(lv, rng);
    Rect blocks[10];
    int n = room_blocks(blocks, 10);
    int captain_done = 0;
    for (int i = 0; i < n; i++) {
        if (fr_rng_float01(rng) < 0.9f) {
            Rect room = carve_room(lv, rng, &blocks[i]);
            if (room.w <= 0) continue;
            if (!captain_done) {
                /* Captain's quarters: spawn point + furnishing. */
                lv->spawn_x = room.x + room.w / 2;
                lv->spawn_y = room.y + room.h / 2;
                add_sprite(lv, (float)room.x + 0.5f, (float)room.y + 0.5f, 'H', FR_LMAGENTA, 1.1f);
                add_sprite(lv, room.x + room.w - 0.5f, room.y + 0.5f, '=', FR_DGRAY, 0.8f);
                add_sprite(lv, room.x + room.w / 2.0f, room.y + 0.5f, 'i', FR_YELLOW, 0.6f);
                captain_done = 1;
            } else {
                sprinkle(lv, rng, &room, 'H', FR_BROWN, 1.0f, 3);
                sprinkle(lv, rng, &room, 'i', FR_YELLOW, 0.6f, 2);
            }
        }
    }
    if (!captain_done) {
        lv->spawn_x = 13;
        lv->spawn_y = 31;
    }
}

/* ---- Deck 2: Cargo ---- */
static void gen_cargo_deck(FrLevel *lv, FrRng *rng)
{
    carve_corridors(lv);
    add_windows(lv, rng);
    /* Two big open cargo bays. */
    fill_rect(lv, 24, 8, 20, 20, FR_T_FLOOR);
    fill_rect(lv, 24, 36, 20, 20, FR_T_FLOOR);
    fill_rect(lv, 48, 8, 18, 20, FR_T_FLOOR);
    fill_rect(lv, 48, 36, 18, 20, FR_T_FLOOR);
    /* Cargo control console bank. */
    fill_rect(lv, 70, 28, 2, 8, FR_T_CONSOLE_CARGO);
    /* Crate rows in the bays. */
    for (int bay = 0; bay < 4; bay++) {
        int bx = bay % 2 == 0 ? 26 : 50;
        int by = bay < 2 ? 10 : 38;
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 7; c++) {
                if (fr_rng_next(rng) & 3) {
                    add_sprite(lv, bx + c * 2.5f + 0.5f, by + r * 3.5f + 0.5f,
                               (char)(fr_rng_next(rng) & 1 ? 'H' : 'X'),
                               fr_rng_next(rng) & 1 ? FR_BROWN : FR_DGRAY, 1.4f);
                }
            }
    }
    /* Airlock / docking ramp at the east end of the spine. */
    fill_rect(lv, DECK_W - 12, 26, 8, 10, FR_T_FLOOR_ROOM);
    fill_rect(lv, DECK_W - 6, 28, 4, 6, FR_T_WALL_TECH);
    fr_level_set(lv, DECK_W - 10, 30, FR_T_AIRLOCK);
    fr_level_set(lv, DECK_W - 10, 31, FR_T_AIRLOCK);
    lv->airlock_x = DECK_W - 10;
    lv->airlock_y = 30;
    fill_rect(lv, DECK_W - 14, 30, 2, 2, FR_T_FLOOR_HALL);
    /* A few side rooms. */
    Rect blocks[10];
    int n = room_blocks(blocks, 10);
    for (int i = 0; i < n; i++) {
        if (fr_rng_float01(rng) < 0.5f) {
            Rect room = carve_room(lv, rng, &blocks[i]);
            if (room.w > 0) sprinkle(lv, rng, &room, 'X', FR_DGRAY, 1.2f, 4);
        }
    }
}

/* ---- Deck 3: Engineering ---- */
static void gen_eng_deck(FrLevel *lv, FrRng *rng)
{
    carve_corridors(lv);
    add_windows(lv, rng);
    /* Engine room: large central chamber with machinery. */
    fill_rect(lv, 40, 18, 32, 28, FR_T_FLOOR);
    fill_rect(lv, 38, 16, 2, 32, FR_T_WALL_TECH);
    fill_rect(lv, 72, 16, 2, 32, FR_T_WALL_TECH);
    fill_rect(lv, 40, 16, 32, 2, FR_T_WALL_TECH);
    fill_rect(lv, 40, 46, 32, 2, FR_T_WALL_TECH);
    /* Doorways into the engine room. */
    fr_level_set(lv, 54, 16, FR_T_DOOR);
    fr_level_set(lv, 58, 16, FR_T_DOOR);
    fr_level_set(lv, 54, 47, FR_T_DOOR);
    fr_level_set(lv, 58, 47, FR_T_DOOR);
    fr_level_set(lv, 39, 31, FR_T_DOOR);
    fr_level_set(lv, 72, 31, FR_T_DOOR);
    /* Engineering console banks. */
    fill_rect(lv, 44, 20, 2, 4, FR_T_CONSOLE_ENG);
    fill_rect(lv, 44, 40, 2, 4, FR_T_CONSOLE_ENG);
    /* Reactor columns. */
    for (int i = 0; i < 3; i++) {
        add_sprite(lv, 54 + 0.5f, 22 + i * 8 + 0.5f, 'R', FR_ORANGE, 2.0f);
        add_sprite(lv, 62 + 0.5f, 22 + i * 8 + 0.5f, 'R', FR_ORANGE, 2.0f);
    }
    for (int i = 0; i < 6; i++)
        add_sprite(lv, 46 + (i % 3) * 8 + 0.5f, 30 + (i / 3) * 6 + 0.5f, '#', FR_DGRAY, 1.5f);
    /* Maintenance rooms. */
    Rect blocks[10];
    int n = room_blocks(blocks, 10);
    for (int i = 0; i < n; i++) {
        if (fr_rng_float01(rng) < 0.6f) {
            Rect room = carve_room(lv, rng, &blocks[i]);
            if (room.w > 0) sprinkle(lv, rng, &room, '#', FR_DGRAY, 1.2f, 3);
        }
    }
}

/* ---- Deck 4: Hydroponics ---- */
static void gen_hydro_deck(FrLevel *lv, FrRng *rng)
{
    carve_corridors(lv);
    add_windows(lv, rng);
    Rect blocks[10];
    int n = room_blocks(blocks, 10);
    for (int i = 0; i < n; i++) {
        Rect room = carve_room(lv, rng, &blocks[i]);
        if (room.w <= 0) continue;
        sprinkle(lv, rng, &room, ';', FR_GREEN, 0.9f, 8);
        sprinkle(lv, rng, &room, 'Y', FR_LGREEN, 1.8f, 2);
    }
    /* Central orchard. */
    fill_rect(lv, 44, 20, 24, 24, FR_T_FLOOR);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            add_sprite(lv, 46 + c * 6 + 0.5f, 22 + r * 6 + 0.5f, 'Y', FR_GREEN, 1.8f);
}

void fr_shipgen_generate(FrShipMap *ship, uint64_t seed)
{
    ship->deck_count = FR_MAX_DECKS;
    init_level(&ship->decks[0], DECK_W, DECK_H, "Command Deck");
    init_level(&ship->decks[1], DECK_W, DECK_H, "Habitation Deck");
    init_level(&ship->decks[2], DECK_W, DECK_H, "Cargo Deck");
    init_level(&ship->decks[3], DECK_W, DECK_H, "Engineering Deck");
    init_level(&ship->decks[4], DECK_W, DECK_H, "Hydroponics Deck");

    for (int d = 0; d < FR_MAX_DECKS; d++) {
        FrRng rng;
        fr_rng_seed(&rng, seed * 1000 + (uint64_t)d + 1);
        switch (d) {
        case 0: gen_bridge_deck(&ship->decks[0], &rng); break;
        case 1: gen_hab_deck(&ship->decks[1], &rng); break;
        case 2: gen_cargo_deck(&ship->decks[2], &rng); break;
        case 3: gen_eng_deck(&ship->decks[3], &rng); break;
        case 4: gen_hydro_deck(&ship->decks[4], &rng); break;
        }
    }
}

void fr_station_generate(FrLevel *lv, uint64_t seed, const char *port_name)
{
    init_level(lv, 48, 32, "station");
    snprintf(lv->name, sizeof(lv->name), "%s", port_name);
    FrRng rng;
    fr_rng_seed(&rng, seed);
    /* Concourse hall. */
    fill_rect(lv, 5, 8, 38, 18, FR_T_FLOOR_HALL);
    /* Terminal banks along the north wall. */
    fill_rect(lv, 9, 7, 4, 1, FR_T_TERMINAL_MARKET);
    fill_rect(lv, 21, 7, 4, 1, FR_T_TERMINAL_CONTRACT);
    fill_rect(lv, 33, 7, 4, 1, FR_T_TERMINAL_FUEL);
    /* Arrival ramp room at the south (back to the ship). */
    fill_rect(lv, 20, 26, 8, 4, FR_T_FLOOR_ROOM);
    fr_level_set(lv, 23, 29, FR_T_AIRLOCK);
    fr_level_set(lv, 24, 29, FR_T_AIRLOCK);
    fill_rect(lv, 23, 25, 2, 1, FR_T_FLOOR_HALL);
    lv->airlock_x = 23;
    lv->airlock_y = 29;
    lv->spawn_x = 23;
    lv->spawn_y = 27;
    /* Crowds and benches. */
    for (int i = 0; i < 10; i++) {
        add_sprite(lv, (float)fr_rng_range(&rng, 7, 40) + 0.5f,
                   (float)fr_rng_range(&rng, 10, 23) + 0.5f,
                   fr_rng_next(&rng) & 1 ? 'o' : 'n',
                   fr_rng_next(&rng) & 1 ? FR_CYAN : FR_LMAGENTA, 1.0f);
    }
    add_sprite(lv, 14.5f, 18.5f, '=', FR_DGRAY, 1.0f);
    add_sprite(lv, 30.5f, 18.5f, '=', FR_DGRAY, 1.0f);
}
