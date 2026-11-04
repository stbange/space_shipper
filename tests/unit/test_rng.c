/* test_rng.c - PCG32 determinism and distribution bounds. */
#include "unity.h"

#include "rng.h"

static FrRng a, b;

void setUp(void)
{
    fr_rng_seed(&a, 12345);
    fr_rng_seed(&b, 12345);
}

void tearDown(void) {}

void test_same_seed_same_sequence(void)
{
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_EQUAL_UINT32(fr_rng_next(&a), fr_rng_next(&b));
    }
}

void test_different_seed_differs(void)
{
    FrRng c;
    fr_rng_seed(&c, 54321);
    int same = 0;
    for (int i = 0; i < 100; i++) {
        if (fr_rng_next(&c) == fr_rng_next(&b)) same++;
    }
    TEST_ASSERT_LESS_THAN(20, same);
}

void test_range_bounds(void)
{
    for (int i = 0; i < 1000; i++) {
        int v = fr_rng_range(&a, 3, 7);
        TEST_ASSERT_INT_WITHIN_MESSAGE(2, 5, v, "value within [3,7]");
        TEST_ASSERT_TRUE(v >= 3 && v <= 7);
    }
}

void test_range_degenerate(void)
{
    TEST_ASSERT_EQUAL_INT(5, fr_rng_range(&a, 5, 5));
    TEST_ASSERT_EQUAL_INT(9, fr_rng_range(&a, 9, 2));
}

void test_float01_bounds(void)
{
    for (int i = 0; i < 1000; i++) {
        float f = fr_rng_float01(&a);
        TEST_ASSERT_TRUE(f >= 0.0f && f < 1.0f);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_same_seed_same_sequence);
    RUN_TEST(test_different_seed_differs);
    RUN_TEST(test_range_bounds);
    RUN_TEST(test_range_degenerate);
    RUN_TEST(test_float01_bounds);
    return UNITY_END();
}
