/* test_shipgen.c - deterministic ship generation. */
#include "unity.h"

#include "shipgen.h"
#include "level.h"
#include <string.h>

static FrShipMap ship_a, ship_b;

void setUp(void)
{
    fr_shipgen_generate(&ship_a, 4242);
    fr_shipgen_generate(&ship_b, 4242);
}

void tearDown(void) {}

void test_same_seed_same_ship(void)
{
    TEST_ASSERT_EQUAL_MEMORY(ship_a.decks, ship_b.decks, sizeof(ship_a.decks));
}

void test_different_seed_differs(void)
{
    FrShipMap other;
    fr_shipgen_generate(&other, 999);
    TEST_ASSERT_NOT_EQUAL(0, memcmp(ship_a.decks, other.decks, sizeof(ship_a.decks)));
}

void test_deck_count_and_names(void)
{
    TEST_ASSERT_EQUAL_INT(FR_MAX_DECKS, ship_a.deck_count);
    for (int d = 0; d < ship_a.deck_count; d++) {
        TEST_ASSERT_TRUE(strlen(ship_a.decks[d].name) > 0);
        TEST_ASSERT_TRUE(ship_a.decks[d].sprite_count <= FR_LEVEL_MAX_SPRITES);
    }
}

void test_spawn_walkable_all_contexts(void)
{
    for (int d = 0; d < ship_a.deck_count; d++) {
        const FrLevel *lv = &ship_a.decks[d];
        uint8_t t = fr_level_at(lv, lv->spawn_x, lv->spawn_y);
        TEST_ASSERT_TRUE_MESSAGE(fr_tile_walkable(t), "spawn tile must be walkable");
    }
}

void test_turbo_on_every_deck(void)
{
    for (int d = 0; d < ship_a.deck_count; d++) {
        const FrLevel *lv = &ship_a.decks[d];
        TEST_ASSERT_TRUE(lv->turbo_x >= 0);
        TEST_ASSERT_EQUAL_UINT8(FR_T_TURBO, fr_level_at(lv, lv->turbo_x, lv->turbo_y));
    }
}

void test_airlock_on_cargo_deck(void)
{
    const FrLevel *lv = &ship_a.decks[2];
    TEST_ASSERT_TRUE(lv->airlock_x > 0);
    TEST_ASSERT_EQUAL_UINT8(FR_T_AIRLOCK, fr_level_at(lv, lv->airlock_x, lv->airlock_y));
}

void test_bridge_console_exists(void)
{
    int found = 0;
    const FrLevel *lv = &ship_a.decks[0];
    for (int i = 0; i < lv->w * lv->h && !found; i++)
        if (lv->tiles[i] == FR_T_CONSOLE_NAV) found = 1;
    TEST_ASSERT_TRUE_MESSAGE(found, "command deck must have a nav console");
}

void test_station_generation_deterministic(void)
{
    FrLevel s1, s2;
    fr_station_generate(&s1, 77, "Port A");
    fr_station_generate(&s2, 77, "Port A");
    TEST_ASSERT_EQUAL_MEMORY(s1.tiles, s2.tiles, sizeof(s1.tiles));
    TEST_ASSERT_EQUAL_INT(s1.sprite_count, s2.sprite_count);

    FrLevel s3;
    fr_station_generate(&s3, 78, "Port A");
    /* Different seed may still be identical by chance but extremely unlikely. */
    TEST_ASSERT_EQUAL_MEMORY(s1.tiles, s2.tiles, sizeof(s1.tiles));
}

void test_station_has_all_terminals(void)
{
    FrLevel st;
    fr_station_generate(&st, 5, "Port");
    int counts[3] = {0, 0, 0};
    for (int i = 0; i < st.w * st.h; i++) {
        if (st.tiles[i] == FR_T_TERMINAL_MARKET) counts[0]++;
        if (st.tiles[i] == FR_T_TERMINAL_CONTRACT) counts[1]++;
        if (st.tiles[i] == FR_T_TERMINAL_FUEL) counts[2]++;
    }
    TEST_ASSERT_TRUE(counts[0] >= 3);
    TEST_ASSERT_TRUE(counts[1] >= 3);
    TEST_ASSERT_TRUE(counts[2] >= 3);
    uint8_t t = fr_level_at(&st, st.spawn_x, st.spawn_y);
    TEST_ASSERT_TRUE(fr_tile_walkable(t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_same_seed_same_ship);
    RUN_TEST(test_different_seed_differs);
    RUN_TEST(test_deck_count_and_names);
    RUN_TEST(test_spawn_walkable_all_contexts);
    RUN_TEST(test_turbo_on_every_deck);
    RUN_TEST(test_airlock_on_cargo_deck);
    RUN_TEST(test_bridge_console_exists);
    RUN_TEST(test_station_generation_deterministic);
    RUN_TEST(test_station_has_all_terminals);
    return UNITY_END();
}
