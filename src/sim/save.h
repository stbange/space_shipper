/* save.h - JSON save/load of the full game state. */
#ifndef FR_SAVE_H
#define FR_SAVE_H

#include "world.h"
#include "shipgen.h"

/* Player-side state that must persist. */
typedef struct {
    int deck;       /* current deck index */
    int on_station; /* 1 if walking a station map */
    int docked;     /* 1 when safely docked at a port */
    float px, py;   /* player position */
    float angle;
    int ui_state;   /* game UI state enum value */
} FrPlayerState;

#define FR_SAVE_VERSION 1

/* Serialize to a JSON string (malloc'd; caller frees). NULL on error. */
char *fr_save_serialize(const FrWorld *w, const FrPlayerState *p);
/* Parse and load. Returns 0 on success. */
int fr_save_parse(const char *json, FrWorld *w, FrPlayerState *p);
/* Write to file. Returns 0 on success. */
int fr_save_write_file(const char *path, const FrWorld *w, const FrPlayerState *p);
/* Read from file into state. Returns 0 on success. */
int fr_save_read_file(const char *path, FrWorld *w, FrPlayerState *p);

#endif
