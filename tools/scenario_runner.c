/* scenario_runner.c - headless automated game test runner.

   Executes scenario scripts against the real game logic without any
   windowing/graphics dependency. Renders go into the in-memory ASCII grid,
   which can be dumped or compared against golden frames.

   Script commands (one per line, '#' comments):
     seed N                  set default seed
     newgame                 start a new game
     frame / frames N        run N update+render frames (dt = 1/60)
     input KEY on|off        hold a key (W,S,A,D,LEFT,RIGHT,UP,DOWN,ROT_L,...)
     press KEY               one-frame key press (E,ESC,ENTER,UP,DOWN,B,S,1..5)
     tick N                  advance the world simulation N ticks directly
     navigate P              plot course & depart to port P (as the nav console)
     dock complete           finish the docking sequence (as if flown by hand)
     market buy C N / market sell C N
     contract accept I       accept offered contract by index
     refuel N / repair N / loan N
     expect FIELD OP VALUE   assertion (==, !=, >=, <=)
     dumpframe FILE          write the current grid as text
     golden FILE             compare current grid with a golden frame file
     save FILE / load FILE
     msg                     print the last world message
     quit
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/game/game.h"

static FrGame game;
static FrInput input;
static int failures;
static int game_started;

static int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int set_key(const char *name, int on)
{
    uint8_t *f = NULL;
    if (str_ieq(name, "W")) f = &input.fwd;
    else if (str_ieq(name, "S")) f = &input.back;
    else if (str_ieq(name, "A")) { input.strafe_l = (uint8_t)on; input.rot_l = (uint8_t)on; return 1; }
    else if (str_ieq(name, "D")) { input.strafe_r = (uint8_t)on; input.rot_r = (uint8_t)on; return 1; }
    else if (str_ieq(name, "LEFT")) f = &input.turn_l;
    else if (str_ieq(name, "RIGHT")) f = &input.turn_r;
    else if (str_ieq(name, "UP")) f = &input.up;
    else if (str_ieq(name, "DOWN")) f = &input.down;
    else if (str_ieq(name, "E")) f = &input.interact;
    else if (str_ieq(name, "ESC")) f = &input.escape;
    else if (str_ieq(name, "ENTER")) f = &input.enter;
    else if (str_ieq(name, "B")) f = &input.buy;
    else if (str_ieq(name, "KEY1")) f = &input.key_1;
    else if (str_ieq(name, "KEY2")) f = &input.key_2;
    else if (str_ieq(name, "KEY3")) f = &input.key_3;
    else if (str_ieq(name, "KEY4")) f = &input.key_4;
    else if (str_ieq(name, "KEY5")) f = &input.key_5;
    if (f) { *f = (uint8_t)on; return 1; }
    return 0;
}

static double field_value(const char *field, double *out, char *sout, size_t soutsz)
{
    const FrWorld *w = &game.world;
    *out = 0;
    sout[0] = 0;
    if (str_ieq(field, "state")) { snprintf(sout, soutsz, "%s", fr_ui_state_name(game.ui)); return 1; }
    if (str_ieq(field, "money")) { *out = w->ship.money; return 1; }
    if (str_ieq(field, "fuel")) { *out = w->ship.fuel; return 1; }
    if (str_ieq(field, "wear")) { *out = w->ship.wear; return 1; }
    if (str_ieq(field, "loan")) { *out = w->ship.loan; return 1; }
    if (str_ieq(field, "port")) { *out = w->port; return 1; }
    if (str_ieq(field, "docked")) { *out = game.player.docked; return 1; }
    if (str_ieq(field, "day")) { *out = fr_world_day(w); return 1; }
    if (str_ieq(field, "tick")) { *out = w->tick; return 1; }
    if (str_ieq(field, "hold_used")) { *out = fr_hold_used(&w->ship); return 1; }
    if (str_ieq(field, "pos_x")) { *out = game.player.px; return 1; }
    if (str_ieq(field, "pos_y")) { *out = game.player.py; return 1; }
    if (str_ieq(field, "travel_active")) { *out = w->travel.active; return 1; }
    if (str_ieq(field, "travel_to")) { *out = w->travel.to_port; return 1; }
    if (str_ieq(field, "contract_count")) { *out = w->contract_count; return 1; }
    if (str_ieq(field, "dock_x")) { *out = game.dock.x; return 1; }
    if (str_ieq(field, "dock_y")) { *out = game.dock.y; return 1; }
    if (str_ieq(field, "dock_angle")) { *out = game.dock.angle; return 1; }
    if (str_ieq(field, "speed")) { *out = game.speed; return 1; }
    if (str_ieq(field, "on_station")) { *out = game.player.on_station; return 1; }
    if (str_ieq(field, "deck")) { *out = game.player.deck; return 1; }
    if (strncmp(field, "hold_", 5) == 0) {
        *out = w->ship.hold[atoi(field + 5)];
        return 1;
    }
    if (strncmp(field, "contract_", 9) == 0) {
        char *end = NULL;
        long idx = strtol(field + 9, &end, 10);
        if (end && *end == '_' && idx >= 0 && idx < w->contract_count) {
            const char *sub = end + 1;
            if (str_ieq(sub, "state")) { *out = w->contracts[idx].state; return 1; }
            if (str_ieq(sub, "reward")) { *out = w->contracts[idx].reward; return 1; }
            if (str_ieq(sub, "dest")) { *out = w->contracts[idx].dest_port; return 1; }
            if (str_ieq(sub, "units")) { *out = w->contracts[idx].units; return 1; }
        }
    }
    return 0;
}

static void run_frame(void)
{
    fr_game_update(&game, &input, 1.0f / 60.0f);
    fr_game_render(&game);
    /* clear one-frame fields */
    input.interact = input.escape = input.up = input.down = 0;
    input.enter = input.buy = input.sell = 0;
    input.key_1 = input.key_2 = input.key_3 = input.key_4 = input.key_5 = 0;
}

static void dock_complete_cmd(void)
{
    /* Simulate the successful end of the manual docking sequence. */
    game.world.travel.active = 0;
    game.dock.x = game.dock.pad_x;
    game.dock.y = game.dock.pad_y;
    game.dock.vx = 0;
    game.dock.vy = 0;
    /* reuse the game's completion logic via a benign frame */
    game.ui = FR_UI_DOCKING;
    FrInput in;
    memset(&in, 0, sizeof(in));
    fr_game_update(&game, &in, 1.0f / 60.0f);
}

static int cmp_double(double a, const char *op, double b)
{
    if (strcmp(op, "==") == 0) return a == b;
    if (strcmp(op, "!=") == 0) return a != b;
    if (strcmp(op, ">=") == 0) return a >= b;
    if (strcmp(op, "<=") == 0) return a <= b;
    if (strcmp(op, ">") == 0) return a > b;
    if (strcmp(op, "<") == 0) return a < b;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <script> [seed]\n", argv[0]);
        return 2;
    }
    uint64_t seed = 20261104ULL;
    if (argc > 2) seed = strtoull(argv[2], NULL, 10);
    fr_game_init(&game, seed);

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "cannot open script: %s\n", argv[1]);
        return 2;
    }

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        /* strip comments and newline */
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char cmd[32], a1[128], a2[128], a3[128];
        int n = sscanf(line, "%31s %127s %127s %127s", cmd, a1, a2, a3);
        if (n <= 0) continue;

        if (strcmp(cmd, "seed") == 0) {
            seed = strtoull(a1, NULL, 10);
        } else if (strcmp(cmd, "newgame") == 0) {
            fr_game_new(&game, seed);
            game_started = 1;
        } else if (strcmp(cmd, "frame") == 0) {
            if (game_started) run_frame();
        } else if (strcmp(cmd, "frames") == 0) {
            int nf = atoi(a1);
            for (int i = 0; i < nf && game_started; i++) run_frame();
        } else if (strcmp(cmd, "input") == 0) {
            if (!set_key(a1, str_ieq(a2, "on") || atoi(a2) != 0)) {
                printf("line %d: unknown key '%s'\n", lineno, a1);
                failures++;
            }
        } else if (strcmp(cmd, "press") == 0) {
            if (!set_key(a1, 1)) {
                printf("line %d: unknown key '%s'\n", lineno, a1);
                failures++;
            } else {
                if (game_started) run_frame();
                set_key(a1, 0);
            }
        } else if (strcmp(cmd, "tick") == 0) {
            int nt = atoi(a1);
            fr_game_tick_n(&game, nt);
        } else if (strcmp(cmd, "teleport") == 0 && n >= 4) {
            game.player.px = (float)atof(a1);
            game.player.py = (float)atof(a2);
            game.player.angle = (float)atof(a3);
        } else if (strcmp(cmd, "deck") == 0) {
            game.player.deck = atoi(a1);
            game.player.on_station = 0;
        } else if (strcmp(cmd, "navigate") == 0) {
            int dest = atoi(a1);
            if (fr_travel_begin(&game.world, game.world.port, dest) == 0) {
                game.player.docked = 0;
                game.player.on_station = 0;
            } else {
                printf("line %d: navigate failed (fuel?)\n", lineno);
                failures++;
            }
        } else if (strcmp(cmd, "dock") == 0 && n >= 2 && strcmp(a1, "complete") == 0) {
            dock_complete_cmd();
        } else if (strcmp(cmd, "speed") == 0) {
            game.speed = atoi(a1);
        } else if (strcmp(cmd, "market") == 0 && n >= 4) {
            int c = atoi(a2), units = atoi(a3);
            if (strcmp(a1, "buy") == 0) fr_market_buy(&game.world, game.world.port, c, units);
            else fr_market_sell(&game.world, game.world.port, c, units);
        } else if (strcmp(cmd, "contract") == 0 && n >= 3) {
            if (strcmp(a1, "accept") == 0) {
                int idx = atoi(a2);
                if (idx < game.world.contract_count) {
                    if (fr_contract_accept(&game.world, game.world.contracts[idx].id) != 0) {
                        printf("line %d: contract accept failed\n", lineno);
                        failures++;
                    }
                }
            }
        } else if (strcmp(cmd, "refuel") == 0) {
            fr_refuel(&game.world, atoi(a1));
        } else if (strcmp(cmd, "repair") == 0) {
            fr_repair(&game.world, atoi(a1));
        } else if (strcmp(cmd, "loan") == 0) {
            fr_loan_take(&game.world, atoi(a1));
        } else if (strcmp(cmd, "expect") == 0 && n >= 3) {
            double v;
            char sv[64];
            if (!game_started) {
                printf("line %d: expect before newgame\n", lineno);
                failures++;
            } else if (field_value(a1, &v, sv, sizeof(sv))) {
                if (sv[0]) {
                    /* string field: expect FIELD VALUE */
                    const char *want = n >= 4 ? a3 : a2;
                    if (!str_ieq(sv, want)) {
                        printf("line %d: expect %s: got '%s', want '%s'\n", lineno, a1, sv, want);
                        failures++;
                    }
                } else if (n >= 4) {
                    double want = atof(a3);
                    if (!cmp_double(v, a2, want)) {
                        printf("line %d: expect %s %s %s: got %g\n", lineno, a1, a2, a3, v);
                        failures++;
                    }
                } else {
                    double want = atof(a2);
                    if (v != want) {
                        printf("line %d: expect %s == %s: got %g\n", lineno, a1, a2, v);
                        failures++;
                    }
                }
            } else {
                printf("line %d: unknown field '%s'\n", lineno, a1);
                failures++;
            }
        } else if (strcmp(cmd, "dumpframe") == 0) {
            fr_game_render(&game);
            static char buf[FR_GRID_W * (FR_GRID_H + 1) + 16];
            fr_grid_dump(&game.grid, buf, sizeof(buf));
            FILE *o = fopen(a1, "wb");
            if (o) {
                fputs(buf, o);
                fclose(o);
            } else {
                printf("line %d: cannot write %s\n", lineno, a1);
                failures++;
            }
        } else if (strcmp(cmd, "golden") == 0) {
            fr_game_render(&game);
            static char buf[FR_GRID_W * (FR_GRID_H + 1) + 16];
            fr_grid_dump(&game.grid, buf, sizeof(buf));
            FILE *o = fopen(a1, "rb");
            if (!o) {
                printf("line %d: golden file missing: %s\n", lineno, a1);
                failures++;
            } else {
                static char ref[FR_GRID_W * (FR_GRID_H + 1) + 16];
                size_t rn = fread(ref, 1, sizeof(ref) - 1, o);
                ref[rn] = 0;
                fclose(o);
                if (strcmp(ref, buf) != 0) {
                    printf("line %d: golden frame mismatch: %s\n", lineno, a1);
                    failures++;
                }
            }
        } else if (strcmp(cmd, "save") == 0) {
            fr_game_save(&game, a1);
        } else if (strcmp(cmd, "load") == 0) {
            if (fr_game_load(&game, a1) != 0) {
                printf("line %d: load failed: %s\n", lineno, a1);
                failures++;
            }
            game_started = 1;
        } else if (strcmp(cmd, "msg") == 0) {
            printf("msg: %s\n", game.world.message);
        } else if (strcmp(cmd, "print") == 0 && n >= 2) {
            double v;
            char sv[64];
            if (field_value(a1, &v, sv, sizeof(sv))) {
                if (sv[0]) printf("%s = %s\n", a1, sv);
                else printf("%s = %g\n", a1, v);
            } else {
                printf("%s = ?\n", a1);
            }
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else {
            printf("line %d: unknown command '%s'\n", lineno, cmd);
            failures++;
        }
    }
    fclose(f);

    if (failures) {
        printf("FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("OK\n");
    return 0;
}
