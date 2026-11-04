/* rng.c - PCG32 implementation (deterministic across platforms). */
#include "rng.h"

void fr_rng_seed(FrRng *r, uint64_t seed)
{
    r->state = 0;
    r->inc = (seed << 1) | 1u;
    (void)fr_rng_next(r);
    r->state += 0x853c49e6748fea9bULL;
    (void)fr_rng_next(r);
}

uint32_t fr_rng_next(FrRng *r)
{
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
    uint32_t rot = (uint32_t)(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}

int fr_rng_range(FrRng *r, int min, int max)
{
    if (max <= min) return min;
    uint32_t span = (uint32_t)(max - min + 1);
    return min + (int)(fr_rng_next(r) % span);
}

float fr_rng_float01(FrRng *r)
{
    return (float)(fr_rng_next(r) >> 8) * (1.0f / 16777216.0f);
}
