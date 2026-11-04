/* level.c - wall appearance table. */
#include "level.h"

uint8_t fr_tile_wall_color(uint8_t t)
{
    switch (t) {
    case FR_T_WALL: return FR_GRAY;
    case FR_T_WALL_TECH: return FR_DGRAY;
    case FR_T_DOOR: return FR_YELLOW;
    case FR_T_WINDOW: return FR_CYAN;
    case FR_T_CONSOLE_NAV: return FR_LBLUE;
    case FR_T_CONSOLE_ENG: return FR_ORANGE;
    case FR_T_CONSOLE_CARGO: return FR_BROWN;
    case FR_T_TERMINAL_MARKET: return FR_LGREEN;
    case FR_T_TERMINAL_CONTRACT: return FR_LMAGENTA;
    case FR_T_TERMINAL_FUEL: return FR_LRED;
    default: return FR_GRAY;
    }
}

float fr_tile_wall_boost(uint8_t t)
{
    switch (t) {
    case FR_T_WINDOW: return 1.35f;
    case FR_T_DOOR: return 1.15f;
    case FR_T_CONSOLE_NAV:
    case FR_T_CONSOLE_ENG:
    case FR_T_CONSOLE_CARGO:
    case FR_T_TERMINAL_MARKET:
    case FR_T_TERMINAL_CONTRACT:
    case FR_T_TERMINAL_FUEL: return 1.25f;
    default: return 1.0f;
    }
}
