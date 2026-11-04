/* world.h - game world simulation: ports, markets, contracts, travel, costs.

   Pure simulation, no platform dependencies. All state lives in FrWorld so
   save/load and headless tests operate on one struct. Deterministic: all
   randomness flows through the seeded PCG32 stored in the world state.
*/
#ifndef FR_WORLD_H
#define FR_WORLD_H

#include "rng.h"

#define FR_MAX_PORTS 4
#define FR_MAX_COMMODITIES 8
#define FR_MAX_CONTRACTS 24
#define FR_HOLD_CAP 1200

/* Fixed simulation constants. */
#define FR_TICKS_PER_DAY 240
#define FR_DAILY_WAGES 1200
#define FR_DAILY_INSURANCE 300
#define FR_DAILY_LOAN_RATE 2 /* percent of loan per day, in tenths: 0.2% => 2 */
#define FR_SHIP_SPEED_AU_DAY 250   /* AU per day, in hundredths: 2.50 */
#define FR_FUEL_PER_AU 3000        /* fuel units per AU, in hundredths: 30 */
#define FR_FUEL_PRICE 9            /* credits per fuel unit */
#define FR_FUEL_MAX 1200
#define FR_STOCK_CAP 400

enum { FR_PORT_PLANET = 0, FR_PORT_ORBITAL = 1 };

typedef struct {
    char name[24];
    char desc[64];
    int type;
    int x, y; /* position in AU*10 (fixed-point to keep sim deterministic) */
} FrPortDef;

typedef struct {
    char name[16];
    int base_price; /* credits per unit */
} FrCommodityDef;

typedef struct {
    int id;
    int dest_port;
    int commodity;
    int units;
    int reward;
    int deadline_day;
    int state; /* 0 offered, 1 accepted, 2 delivered, 3 expired */
} FrContract;

typedef struct {
    int money;    /* credits */
    int loan;     /* outstanding loan */
    int fuel;     /* fuel units */
    int wear;     /* 0..100 condition loss */
    int hold[FR_MAX_COMMODITIES];
    int engine_level; /* 1 base; upgrades increase speed */
} FrShip;

typedef struct {
    int active;
    int from_port, to_port;
    int ticks_total, ticks_done;
} FrTravel;

typedef struct {
    uint64_t seed;
    int tick; /* total ticks elapsed */
    FrRng rng;
    FrShip ship;
    int port; /* port index when docked; -1 when in transit */
    FrTravel travel;
    int market_stock[FR_MAX_PORTS][FR_MAX_COMMODITIES];
    FrContract contracts[FR_MAX_CONTRACTS];
    int contract_count;
    int day_event; /* log of last event day, for reports */
    char message[96]; /* last sim message for HUD */
} FrWorld;

/* Static data. */
const FrPortDef *fr_ports(void);
const FrCommodityDef *fr_commodities(void);
int fr_port_count(void);
int fr_commodity_count(void);

/* New game at port 0. */
void fr_world_init(FrWorld *w, uint64_t seed);

/* Advance exactly one tick. Returns event flags:
   bit 0: travel leg completed, bit 1: day rolled over, bit 2: bankruptcy. */
#define FR_EV_ARRIVED 1
#define FR_EV_DAY 2
#define FR_EV_BANKRUPT 4
int fr_world_tick(FrWorld *w);

/* Day number and time-of-day helpers. */
int fr_world_day(const FrWorld *w);
int fr_world_hour(const FrWorld *w);
int fr_world_minute(const FrWorld *w);

/* Distances (AU*10) and travel planning. */
int fr_port_distance(int a, int b);
/* Estimated travel ticks from a to b with current engines. */
int fr_travel_ticks(const FrWorld *w, int from, int to);
/* Fuel needed (units) for the leg. */
int fr_travel_fuel(const FrWorld *w, int from, int to);
/* Start a travel leg. Returns 0 on success. */
int fr_travel_begin(FrWorld *w, int from, int to);
/* Speed in AU/day, hundredths. Engine refits add 0.6 AU/day each. */
int fr_ship_speed(const FrShip *s);

/* Market: current unit price of a commodity at a port. */
int fr_market_price(const FrWorld *w, int port, int commodity);
/* Buy/sell units at a port; returns credits spent/earned (0 on failure). */
int fr_market_buy(FrWorld *w, int port, int commodity, int units);
int fr_market_sell(FrWorld *w, int port, int commodity, int units);
/* Used hold volume. */
int fr_hold_used(const FrShip *s);

/* Contracts: refresh offers at a port for the current day. */
void fr_contracts_refresh(FrWorld *w, int port);
int fr_contract_accept(FrWorld *w, int contract_id);
/* Deliver accepted contracts whose destination is this port. Returns payout. */
int fr_contracts_deliver(FrWorld *w, int port);
/* Expire overdue accepted contracts (penalty). Returns penalties paid. */
int fr_contracts_expire(FrWorld *w);

/* Refuel: buy fuel at port up to amount; returns credits spent. */
int fr_refuel(FrWorld *w, int units);
/* Repair wear: cost per point 60 credits. Returns credits spent. */
int fr_repair(FrWorld *w, int points);
int fr_repair_cost(int points);

/* Bank: take loan (max total 60000), repay. */
int fr_loan_take(FrWorld *w, int amount);
int fr_loan_repay(FrWorld *w, int amount);

/* Money invariant check helper for tests. */
int fr_money_total(const FrWorld *w);

#endif
