/* test_world.c - economy, travel and tick simulation. */
#include "unity.h"

#include "world.h"

static FrWorld w;

void setUp(void)
{
    fr_world_init(&w, 777);
}

void tearDown(void) {}

void test_initial_state(void)
{
    TEST_ASSERT_EQUAL_INT(0, fr_world_day(&w));
    TEST_ASSERT_EQUAL_INT(0, w.port);
    TEST_ASSERT_EQUAL_INT(25000, w.ship.money);
    TEST_ASSERT_EQUAL_INT(FR_FUEL_MAX, w.ship.fuel);
    TEST_ASSERT_TRUE(w.travel.active == 0);
}

void test_distances_positive_and_sane(void)
{
    for (int i = 0; i < fr_port_count(); i++)
        for (int j = 0; j < fr_port_count(); j++) {
            int d = fr_port_distance(i, j);
            if (i == j) {
                TEST_ASSERT_EQUAL_INT(0, d);
            } else {
                TEST_ASSERT_TRUE(d > 20); /* at least ~2 AU apart */
            }
        }
}

void test_travel_math_consistent(void)
{
    int dist = fr_port_distance(0, 1);
    int ticks = fr_travel_ticks(&w, 0, 1);
    /* ticks = days*100 = dist*1000/speed; ticks = days_x100 * 240 / 100 */
    int expect_days_x100 = dist * 1000 / fr_ship_speed(&w.ship);
    int expect_ticks = expect_days_x100 * FR_TICKS_PER_DAY / 100;
    TEST_ASSERT_EQUAL_INT(expect_ticks, ticks);
    TEST_ASSERT_TRUE(ticks > 0);
}

void test_travel_begin_burns_fuel(void)
{
    int fuel0 = w.ship.fuel;
    int rc = fr_travel_begin(&w, 0, 1);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(w.travel.active);
    TEST_ASSERT_TRUE(w.ship.fuel < fuel0);
    TEST_ASSERT_EQUAL_INT(-1, w.port);
    /* second leg while under way refused */
    TEST_ASSERT_EQUAL_INT(-2, fr_travel_begin(&w, 1, 2));
}

void test_travel_insufficient_fuel_refused(void)
{
    w.ship.fuel = 1;
    TEST_ASSERT_EQUAL_INT(-3, fr_travel_begin(&w, 0, 1));
    TEST_ASSERT_TRUE(w.travel.active == 0);
}

void test_travel_arrival(void)
{
    int dest = 1;
    fr_travel_begin(&w, 0, dest);
    int ticks = w.travel.ticks_total;
    int ev = 0;
    for (int i = 0; i < ticks; i++) ev |= fr_world_tick(&w);
    TEST_ASSERT_TRUE(ev & FR_EV_ARRIVED);
    TEST_ASSERT_EQUAL_INT(dest, w.port);
    TEST_ASSERT_TRUE(w.travel.active == 0);
}

void test_market_price_bounds(void)
{
    for (int p = 0; p < fr_port_count(); p++)
        for (int c = 0; c < fr_commodity_count(); c++) {
            int price = fr_market_price(&w, p, c);
            int base = fr_commodities()[c].base_price;
            TEST_ASSERT_TRUE(price >= base * 40 / 100);
            TEST_ASSERT_TRUE(price <= base * 160 / 100);
        }
}

void test_buy_sell_money_conservation(void)
{
    int money0 = w.ship.money;
    int bought = fr_market_buy(&w, w.port, 0, 30);
    TEST_ASSERT_TRUE(bought > 0);
    TEST_ASSERT_EQUAL_INT(30, w.ship.hold[0]);
    /* selling everything back loses to the spread but money stays sane */
    int earned = fr_market_sell(&w, w.port, 0, 30);
    TEST_ASSERT_TRUE(earned > 0);
    TEST_ASSERT_EQUAL_INT(0, w.ship.hold[0]);
    TEST_ASSERT_TRUE(w.ship.money > money0 - 400);
    TEST_ASSERT_TRUE(w.ship.money < money0 + 400);
}

void test_buy_respects_hold_capacity(void)
{
    w.ship.money = 1000000;
    for (int c = 0; c < FR_MAX_COMMODITIES; c++)
        w.market_stock[w.port][c] = FR_STOCK_CAP;
    int total = 0;
    for (int c = 0; c < FR_MAX_COMMODITIES; c++)
        total += fr_market_buy(&w, w.port, c, FR_HOLD_CAP);
    TEST_ASSERT_EQUAL_INT(FR_HOLD_CAP, fr_hold_used(&w.ship));
    TEST_ASSERT_TRUE(total == FR_HOLD_CAP);
}

void test_contract_accept_loads_cargo(void)
{
    /* find an offered contract */
    int idx = -1;
    for (int i = 0; i < w.contract_count; i++)
        if (w.contracts[i].state == 0) { idx = i; break; }
    TEST_ASSERT_TRUE(idx >= 0);
    int units = w.contracts[idx].units;
    int rc = fr_contract_accept(&w, w.contracts[idx].id);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, w.contracts[idx].state);
    TEST_ASSERT_EQUAL_INT(units, fr_hold_used(&w.ship));
}

void test_contract_delivery_pays(void)
{
    int idx = -1;
    for (int i = 0; i < w.contract_count; i++)
        if (w.contracts[i].state == 0) { idx = i; break; }
    TEST_ASSERT_TRUE(idx >= 0);
    FrContract *c = &w.contracts[idx];
    fr_contract_accept(&w, c->id);
    /* travel to the destination, then deliver */
    fr_travel_begin(&w, w.port, c->dest_port);
    for (int i = 0; i < w.travel.ticks_total; i++) fr_world_tick(&w);
    int money0 = w.ship.money; /* daily costs already applied during travel */
    int payout = fr_contracts_deliver(&w, w.port);
    TEST_ASSERT_EQUAL_INT(c->reward, payout);
    TEST_ASSERT_EQUAL_INT(money0 + c->reward, w.ship.money);
    TEST_ASSERT_EQUAL_INT(2, c->state);
    TEST_ASSERT_EQUAL_INT(0, fr_hold_used(&w.ship));
}

void test_daily_costs_applied(void)
{
    int money0 = w.ship.money;
    for (int i = 0; i < FR_TICKS_PER_DAY; i++) fr_world_tick(&w);
    TEST_ASSERT_EQUAL_INT(1, fr_world_day(&w));
    int spent = money0 - w.ship.money;
    TEST_ASSERT_TRUE(spent >= FR_DAILY_WAGES + FR_DAILY_INSURANCE);
}

void test_loan_interest_accrues(void)
{
    fr_loan_take(&w, 10000);
    TEST_ASSERT_EQUAL_INT(10000, w.ship.loan);
    TEST_ASSERT_EQUAL_INT(35000, w.ship.money);
    for (int i = 0; i < FR_TICKS_PER_DAY; i++) fr_world_tick(&w);
    TEST_ASSERT_TRUE(w.ship.loan > 10000);
}

void test_loan_repay(void)
{
    fr_loan_take(&w, 10000);
    fr_loan_repay(&w, 5000);
    TEST_ASSERT_EQUAL_INT(5000, w.ship.loan);
    TEST_ASSERT_EQUAL_INT(30000, w.ship.money);
}

void test_refuel_costs(void)
{
    w.ship.fuel = 100;
    int spent = fr_refuel(&w, 500);
    TEST_ASSERT_TRUE(spent > 0);
    TEST_ASSERT_EQUAL_INT(600, w.ship.fuel);
    TEST_ASSERT_EQUAL_INT(25000 - spent, w.ship.money);
}

void test_repair(void)
{
    w.ship.wear = 20;
    int cost = fr_repair(&w, 10);
    TEST_ASSERT_EQUAL_INT(fr_repair_cost(10), cost);
    TEST_ASSERT_EQUAL_INT(10, w.ship.wear);
}

void test_deterministic_world_evolution(void)
{
    FrWorld x, y;
    fr_world_init(&x, 42);
    fr_world_init(&y, 42);
    fr_travel_begin(&x, 0, 2);
    fr_travel_begin(&y, 0, 2);
    for (int i = 0; i < 2000; i++) {
        fr_world_tick(&x);
        fr_world_tick(&y);
    }
    TEST_ASSERT_EQUAL_INT(x.tick, y.tick);
    TEST_ASSERT_EQUAL_INT(x.ship.money, y.ship.money);
    TEST_ASSERT_EQUAL_MEMORY(x.market_stock, y.market_stock, sizeof(x.market_stock));
    TEST_ASSERT_EQUAL_INT(x.contract_count, y.contract_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_distances_positive_and_sane);
    RUN_TEST(test_travel_math_consistent);
    RUN_TEST(test_travel_begin_burns_fuel);
    RUN_TEST(test_travel_insufficient_fuel_refused);
    RUN_TEST(test_travel_arrival);
    RUN_TEST(test_market_price_bounds);
    RUN_TEST(test_buy_sell_money_conservation);
    RUN_TEST(test_buy_respects_hold_capacity);
    RUN_TEST(test_contract_accept_loads_cargo);
    RUN_TEST(test_contract_delivery_pays);
    RUN_TEST(test_daily_costs_applied);
    RUN_TEST(test_loan_interest_accrues);
    RUN_TEST(test_loan_repay);
    RUN_TEST(test_refuel_costs);
    RUN_TEST(test_repair);
    RUN_TEST(test_deterministic_world_evolution);
    return UNITY_END();
}
