/* test_save.c - save/load round-trip. */
#include "unity.h"

#include "save.h"
#include "../../src/game/game.h"
#include <stdlib.h>
#include <string.h>

static FrWorld w;
static FrPlayerState p;

void setUp(void)
{
    fr_world_init(&w, 99);
    p.deck = 2;
    p.on_station = 0;
    p.docked = 1;
    p.px = 42.5f;
    p.py = 17.5f;
    p.angle = 1.25f;
    p.ui_state = FR_UI_WALK;
}

void tearDown(void) {}

void test_round_trip(void)
{
    fr_travel_begin(&w, 0, 3);
    w.ship.money = 12345;
    w.ship.loan = 500;
    w.ship.wear = 17;
    w.ship.hold[2] = 44;
    w.market_stock[1][3] = 123;

    char *s = fr_save_serialize(&w, &p);
    TEST_ASSERT_NOT_NULL(s);

    FrWorld w2;
    FrPlayerState p2;
    TEST_ASSERT_EQUAL_INT(0, fr_save_parse(s, &w2, &p2));
    free(s);

    TEST_ASSERT_EQUAL_UINT64(w.seed, w2.seed);
    TEST_ASSERT_EQUAL_INT(w.tick, w2.tick);
    TEST_ASSERT_EQUAL_INT(w.ship.money, w2.ship.money);
    TEST_ASSERT_EQUAL_INT(w.ship.loan, w2.ship.loan);
    TEST_ASSERT_EQUAL_INT(w.ship.wear, w2.ship.wear);
    TEST_ASSERT_EQUAL_INT(w.ship.fuel, w2.ship.fuel);
    TEST_ASSERT_EQUAL_INT(w.ship.hold[2], w2.ship.hold[2]);
    TEST_ASSERT_EQUAL_INT(w.port, w2.port);
    TEST_ASSERT_EQUAL_INT(w.travel.active, w2.travel.active);
    TEST_ASSERT_EQUAL_INT(w.travel.to_port, w2.travel.to_port);
    TEST_ASSERT_EQUAL_INT(w.travel.ticks_done, w2.travel.ticks_done);
    TEST_ASSERT_EQUAL_MEMORY(w.market_stock, w2.market_stock, sizeof(w.market_stock));
    TEST_ASSERT_EQUAL_INT(w.contract_count, w2.contract_count);
    TEST_ASSERT_EQUAL_INT(w.rng.state, w2.rng.state);
    TEST_ASSERT_EQUAL_INT(p.deck, p2.deck);
    TEST_ASSERT_EQUAL_INT(p.on_station, p2.on_station);
    TEST_ASSERT_EQUAL_INT(p.docked, p2.docked);
    TEST_ASSERT_EQUAL_FLOAT(p.px, p2.px);
    TEST_ASSERT_EQUAL_FLOAT(p.py, p2.py);
    TEST_ASSERT_EQUAL_FLOAT(p.angle, p2.angle);
    TEST_ASSERT_EQUAL_INT(p.ui_state, p2.ui_state);
}

void test_resume_deterministic_after_load(void)
{
    char *s = fr_save_serialize(&w, &p);
    FrWorld w2;
    FrPlayerState p2;
    fr_save_parse(s, &w2, &p2);
    free(s);
    for (int i = 0; i < 100; i++) {
        fr_world_tick(&w);
        fr_world_tick(&w2);
    }
    TEST_ASSERT_EQUAL_INT(w.tick, w2.tick);
    TEST_ASSERT_EQUAL_INT(w.ship.money, w2.ship.money);
}

void test_file_round_trip(void)
{
    const char *path = "fr_save_test.json";
    TEST_ASSERT_EQUAL_INT(0, fr_save_write_file(path, &w, &p));
    FrWorld w2;
    FrPlayerState p2;
    TEST_ASSERT_EQUAL_INT(0, fr_save_read_file(path, &w2, &p2));
    TEST_ASSERT_EQUAL_INT(w.ship.money, w2.ship.money);
    remove(path);
}

void test_bad_json_rejected(void)
{
    FrWorld w2;
    FrPlayerState p2;
    TEST_ASSERT_NOT_EQUAL(0, fr_save_parse("not json", &w2, &p2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_trip);
    RUN_TEST(test_resume_deterministic_after_load);
    RUN_TEST(test_file_round_trip);
    RUN_TEST(test_bad_json_rejected);
    return UNITY_END();
}
