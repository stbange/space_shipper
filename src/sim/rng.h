/* rng.h - deterministic PCG32 random number generator. */
#ifndef FR_RNG_H
#define FR_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
    uint64_t inc;
} FrRng;

void fr_rng_seed(FrRng *r, uint64_t seed);
uint32_t fr_rng_next(FrRng *r);
/* Uniform in [min, max] inclusive. */
int fr_rng_range(FrRng *r, int min, int max);
/* Uniform float in [0,1). */
float fr_rng_float01(FrRng *r);

#endif
