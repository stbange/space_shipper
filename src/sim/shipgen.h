/* shipgen.h - seeded deterministic ship & station layout generation. */
#ifndef FR_SHIPGEN_H
#define FR_SHIPGEN_H

#include "level.h"
#include "rng.h"

typedef struct {
    int deck_count;
    FrLevel decks[FR_MAX_DECKS];
} FrShipMap;

/* Generate the freighter. Same seed => identical layout. */
void fr_shipgen_generate(FrShipMap *ship, uint64_t seed);

/* Generate a station/port interior map. Same seed => identical layout. */
void fr_station_generate(FrLevel *lv, uint64_t seed, const char *port_name);

/* Deck names for UI. */
extern const char *fr_deck_names[FR_MAX_DECKS];

#endif
