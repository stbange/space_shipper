/* world.c - deterministic world simulation. */
#include "world.h"

#include <stdio.h>
#include <string.h>

static const FrPortDef PORTS[FR_MAX_PORTS] = {
    {"Meridian Station", "Orbital trade hub of the Meridian belt", FR_PORT_ORBITAL, 0, 0},
    {"Novapolis", "Terraced city on the green world Verdance", FR_PORT_PLANET, 120, 40},
    {"Harkonnen Reach", "Deep-space refit yard and smelter", FR_PORT_ORBITAL, -150, 90},
    {"Port Kestrel", "Free-port at the system's cold rim", FR_PORT_PLANET, 60, -200},
};

static const FrCommodityDef COMMODITIES[FR_MAX_COMMODITIES] = {
    {"Ore", 40},
    {"Alloys", 95},
    {"Foodstuffs", 26},
    {"Medicine", 180},
    {"Machinery", 140},
    {"Polymers", 55},
    {"Luxuries", 260},
    {"Hydrocells", 70},
};

const FrPortDef *fr_ports(void) { return PORTS; }
const FrCommodityDef *fr_commodities(void) { return COMMODITIES; }
int fr_port_count(void) { return FR_MAX_PORTS; }
int fr_commodity_count(void) { return FR_MAX_COMMODITIES; }

int fr_world_day(const FrWorld *w) { return w->tick / FR_TICKS_PER_DAY; }
int fr_world_hour(const FrWorld *w) { return (w->tick % FR_TICKS_PER_DAY) / 10; }
int fr_world_minute(const FrWorld *w) { return (w->tick % 10) * 6; }

int fr_port_distance(int a, int b)
{
    const FrPortDef *p = fr_ports();
    int dx = p[a].x - p[b].x, dy = p[a].y - p[b].y;
    /* Fixed-point sqrt: inputs are AU*10; result AU*10. */
    int d2 = dx * dx + dy * dy; /* up to ~(350^2+...) fits int */
    int lo = 0, hi = d2;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (mid <= d2 / mid || (long long)mid * mid <= d2) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

int fr_ship_speed(const FrShip *s)
{
    return FR_SHIP_SPEED_AU_DAY + (s->engine_level - 1) * 60;
}

int fr_travel_ticks(const FrWorld *w, int from, int to)
{
    int dist = fr_port_distance(from, to);
    int speed = fr_ship_speed(&w->ship);
    /* dist is AU*10, speed AU*100/day => days*100 = dist*1000/speed. */
    long long days_x100 = (long long)dist * 1000 / speed;
    long long ticks = days_x100 * FR_TICKS_PER_DAY / 100;
    if (ticks < 1) ticks = 1;
    return (int)ticks;
}

int fr_travel_fuel(const FrWorld *w, int from, int to)
{
    int dist = fr_port_distance(from, to);
    /* dist AU*10, fuel per AU in hundredths => fuel = dist*FR_FUEL_PER_AU/1000. */
    long long fuel = (long long)dist * FR_FUEL_PER_AU / 1000;
    if (fuel < 1) fuel = 1;
    return (int)fuel;
}

int fr_travel_begin(FrWorld *w, int from, int to)
{
    if (from == to) return -1;
    if (w->travel.active) return -2;
    int fuel = fr_travel_fuel(w, from, to);
    if (fuel > w->ship.fuel) return -3;
    w->ship.fuel -= fuel;
    w->travel.active = 1;
    w->travel.from_port = from;
    w->travel.to_port = to;
    w->travel.ticks_total = fr_travel_ticks(w, from, to);
    w->travel.ticks_done = 0;
    w->port = -1;
    snprintf(w->message, sizeof(w->message), "Departed %s for %s",
             fr_ports()[from].name, fr_ports()[to].name);
    return 0;
}

void fr_world_init(FrWorld *w, uint64_t seed)
{
    memset(w, 0, sizeof(*w));
    w->seed = seed;
    fr_rng_seed(&w->rng, seed ^ 0x5752F4C5ULL);
    w->ship.money = 25000;
    w->ship.loan = 0;
    w->ship.fuel = FR_FUEL_MAX;
    w->ship.wear = 0;
    w->ship.engine_level = 1;
    w->port = 0;
    w->tick = 0;
    for (int p = 0; p < FR_MAX_PORTS; p++)
        for (int c = 0; c < FR_MAX_COMMODITIES; c++) {
            int base = FR_STOCK_CAP / 2;
            w->market_stock[p][c] = base + fr_rng_range(&w->rng, -80, 80);
        }
    fr_contracts_refresh(w, 0);
    snprintf(w->message, sizeof(w->message), "Docked at %s", fr_ports()[0].name);
}

int fr_market_price(const FrWorld *w, int port, int commodity)
{
    int stock = w->market_stock[port][commodity];
    /* mult: 1.0 at half stock, 1.5 at empty, 0.5 at full (x100 fixed point). */
    int mult = 100 + (FR_STOCK_CAP / 2 - stock) * 100 / FR_STOCK_CAP;
    if (mult < 40) mult = 40;
    if (mult > 160) mult = 160;
    int base = fr_commodities()[commodity].base_price;
    int price = base * mult / 100;
    if (price < 1) price = 1;
    return price;
}

int fr_hold_used(const FrShip *s)
{
    int total = 0;
    for (int c = 0; c < FR_MAX_COMMODITIES; c++) total += s->hold[c];
    return total;
}

int fr_market_buy(FrWorld *w, int port, int commodity, int units)
{
    if (units <= 0) return 0;
    if (units > w->market_stock[port][commodity])
        units = w->market_stock[port][commodity];
    int hold_space = FR_HOLD_CAP - fr_hold_used(&w->ship);
    if (units > hold_space) units = hold_space;
    int bought = 0;
    while (bought < units) {
        int step = units - bought;
        if (step > 10) step = 10;
        int cost = 0;
        for (int i = 0; i < step; i++) {
            cost += fr_market_price(w, port, commodity);
            w->market_stock[port][commodity]--;
        }
        if (cost > w->ship.money) {
            for (int i = 0; i < step; i++) w->market_stock[port][commodity]++;
            break;
        }
        w->ship.money -= cost;
        w->ship.hold[commodity] += step;
        bought += step;
    }
    return bought;
}

int fr_market_sell(FrWorld *w, int port, int commodity, int units)
{
    if (units <= 0) return 0;
    if (units > w->ship.hold[commodity]) units = w->ship.hold[commodity];
    int earned = 0;
    int sold = 0;
    while (sold < units) {
        int step = units - sold;
        if (step > 10) step = 10;
        int revenue = 0;
        for (int i = 0; i < step; i++) {
            w->market_stock[port][commodity]++;
            int price = fr_market_price(w, port, commodity);
            revenue += price * 9 / 10; /* exchange spread: 10% */
        }
        w->ship.hold[commodity] -= step;
        w->ship.money += revenue;
        earned += revenue;
        sold += step;
    }
    return earned;
}

void fr_contracts_refresh(FrWorld *w, int port)
{
    /* Keep accepted contracts; regenerate offers for this port. */
    FrContract kept[FR_MAX_CONTRACTS];
    int kept_n = 0;
    for (int i = 0; i < w->contract_count; i++) {
        if (w->contracts[i].state == 1) {
            kept[kept_n++] = w->contracts[i];
        }
    }
    int day = fr_world_day(w);
    int n_offers = 2 + fr_rng_range(&w->rng, 0, 2);
    int total = kept_n;
    for (int i = 0; i < n_offers && total < FR_MAX_CONTRACTS; i++) {
        FrContract *c = &kept[total++];
        c->id = (int)(fr_rng_next(&w->rng) & 0x7fffffff);
        c->dest_port = fr_rng_range(&w->rng, 0, fr_port_count() - 1);
        if (c->dest_port == port) c->dest_port = (port + 1) % fr_port_count();
        c->commodity = fr_rng_range(&w->rng, 0, fr_commodity_count() - 1);
        c->units = 20 + fr_rng_range(&w->rng, 0, 60);
        c->state = 0;
        int dist = fr_port_distance(port, c->dest_port);
        long long reward = (long long)dist * c->units *
                           fr_commodities()[c->commodity].base_price / 100;
        reward += fr_rng_range(&w->rng, 200, 800);
        if (reward < 500) reward = 500;
        c->reward = (int)reward;
        int travel_days = dist * 10 / fr_ship_speed(&w->ship);
        c->deadline_day = day + 2 + travel_days * 2 + fr_rng_range(&w->rng, 0, 3);
    }
    memcpy(w->contracts, kept, sizeof(FrContract) * (size_t)total);
    w->contract_count = total;
}

int fr_contract_accept(FrWorld *w, int contract_id)
{
    for (int i = 0; i < w->contract_count; i++) {
        FrContract *c = &w->contracts[i];
        if (c->id == contract_id && c->state == 0) {
            if (fr_hold_used(&w->ship) + c->units > FR_HOLD_CAP) return -1;
            c->state = 1;
            /* Cargo is loaded immediately at the pickup port. */
            w->ship.hold[c->commodity] += c->units;
            snprintf(w->message, sizeof(w->message), "Contract accepted: %d units %s",
                     c->units, fr_commodities()[c->commodity].name);
            return 0;
        }
    }
    return -2;
}

int fr_contracts_deliver(FrWorld *w, int port)
{
    int payout = 0;
    for (int i = 0; i < w->contract_count; i++) {
        FrContract *c = &w->contracts[i];
        if (c->state == 1 && c->dest_port == port) {
            if (w->ship.hold[c->commodity] >= c->units) {
                w->ship.hold[c->commodity] -= c->units;
                w->ship.money += c->reward;
                payout += c->reward;
                c->state = 2;
                snprintf(w->message, sizeof(w->message), "Delivered: +%d cr", c->reward);
            }
        }
    }
    return payout;
}

int fr_contracts_expire(FrWorld *w)
{
    int day = fr_world_day(w);
    int penalties = 0;
    for (int i = 0; i < w->contract_count; i++) {
        FrContract *c = &w->contracts[i];
        if (c->state == 1 && day > c->deadline_day) {
            int penalty = c->reward / 3;
            if (penalty > w->ship.money) penalty = w->ship.money;
            w->ship.money -= penalty;
            penalties += penalty;
            c->state = 3;
            snprintf(w->message, sizeof(w->message), "Contract expired: -%d cr", penalty);
        }
    }
    return penalties;
}

int fr_refuel(FrWorld *w, int units)
{
    if (units <= 0) return 0;
    int space = FR_FUEL_MAX - w->ship.fuel;
    if (units > space) units = space;
    int cost = units * FR_FUEL_PRICE;
    if (cost > w->ship.money) {
        units = w->ship.money / FR_FUEL_PRICE;
        cost = units * FR_FUEL_PRICE;
        if (units <= 0) return 0;
    }
    w->ship.money -= cost;
    w->ship.fuel += units;
    return cost;
}

int fr_repair_cost(int points)
{
    return points * 60;
}

int fr_repair(FrWorld *w, int points)
{
    if (points <= 0) return 0;
    if (points > w->ship.wear) points = w->ship.wear;
    int cost = fr_repair_cost(points);
    if (cost > w->ship.money) {
        points = w->ship.money / 60;
        cost = points * 60;
        if (points <= 0) return 0;
    }
    w->ship.money -= cost;
    w->ship.wear -= points;
    return cost;
}

int fr_loan_take(FrWorld *w, int amount)
{
    if (amount <= 0) return -1;
    if (w->ship.loan + amount > 60000) return -2;
    w->ship.loan += amount;
    w->ship.money += amount;
    return 0;
}

int fr_loan_repay(FrWorld *w, int amount)
{
    if (amount <= 0) return -1;
    if (amount > w->ship.loan) amount = w->ship.loan;
    if (amount > w->ship.money) amount = w->ship.money;
    if (amount <= 0) return -2;
    w->ship.money -= amount;
    w->ship.loan -= amount;
    return 0;
}

int fr_money_total(const FrWorld *w)
{
    return w->ship.money - w->ship.loan;
}

/* Daily economy: drift stock toward equilibrium, roll costs, expire
 * contracts, refresh offers at the current port. */
static void daily_update(FrWorld *w)
{
    for (int p = 0; p < FR_MAX_PORTS; p++) {
        for (int c = 0; c < FR_MAX_COMMODITIES; c++) {
            int s = w->market_stock[p][c];
            int target = FR_STOCK_CAP / 2;
            s += (target - s) / 8 + fr_rng_range(&w->rng, -6, 6);
            if (s < 0) s = 0;
            if (s > FR_STOCK_CAP) s = FR_STOCK_CAP;
            w->market_stock[p][c] = s;
        }
    }
    w->ship.money -= FR_DAILY_WAGES + FR_DAILY_INSURANCE;
    w->ship.money -= w->ship.wear * 8; /* maintenance scales with wear */
    int interest = w->ship.loan * FR_DAILY_LOAN_RATE / 1000;
    w->ship.loan += interest;
    if (w->travel.active) {
        w->ship.wear += 1;
        if (w->ship.wear > 100) w->ship.wear = 100;
    }
    fr_contracts_expire(w);
    if (w->port >= 0) fr_contracts_refresh(w, w->port);
}

int fr_world_tick(FrWorld *w)
{
    int events = 0;
    w->tick++;
    if (w->travel.active) {
        w->travel.ticks_done++;
        if (w->travel.ticks_done >= w->travel.ticks_total) {
            w->travel.active = 0;
            w->port = w->travel.to_port;
            events |= FR_EV_ARRIVED;
            snprintf(w->message, sizeof(w->message), "Arrived at %s",
                     fr_ports()[w->port].name);
        }
    }
    if (w->tick % FR_TICKS_PER_DAY == 0) {
        daily_update(w);
        events |= FR_EV_DAY;
    }
    if (w->ship.money < 0) events |= FR_EV_BANKRUPT;
    return events;
}
