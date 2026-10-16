/**
 * ============================================================================
 * @file workload.c
 * @brief Algorithmic Memory Access Trace Generators
 * ============================================================================
 */

#include "workload.h"
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief 32-bit random number generator using standard C rand() math.
 *
 * @details
 * On Windows platforms (MSVCRT), `RAND_MAX` is often restricted to 32,767 (15 bits).
 * Directly computing `rand() % max_address` would fail to address memory ranges
 * exceeding 32,767 and introduces severe modulo bias.
 *
 * To guarantee platform-agnostic 32-bit integer generation, we concatenate
 * multiple standard `rand()` calls.
 */
static inline uint32_t rand_u32(void) {
    uint32_t r1 = (uint32_t)(rand() & 0x7FFF);
    uint32_t r2 = (uint32_t)(rand() & 0x7FFF);
    uint32_t r3 = (uint32_t)(rand() & 0x0003);
    return (r3 << 30) | (r2 << 15) | r1;
}

int *generate_sequential(int size, int loop_range) {
    if (size <= 0 || loop_range <= 0) {
        return NULL;
    }

    int *trace = (int *)malloc((size_t)size * sizeof(int));
    if (!trace) {
        return NULL;
    }

    for (int i = 0; i < size; ++i) {
        trace[i] = i % loop_range;
    }

    return trace;
}

int *generate_random(int size, int max_address) {
    if (size <= 0 || max_address <= 0) {
        return NULL;
    }

    int *trace = (int *)malloc((size_t)size * sizeof(int));
    if (!trace) {
        return NULL;
    }

    uint32_t bound = (uint32_t)max_address;
    for (int i = 0; i < size; ++i) {
        trace[i] = (int)(rand_u32() % bound);
    }

    return trace;
}

int *generate_temporal_locality(int size, int max_address) {
    if (size <= 0 || max_address <= 0) {
        return NULL;
    }

    int *trace = (int *)malloc((size_t)size * sizeof(int));
    if (!trace) {
        return NULL;
    }

    /*
     * Pareto 80/20 Rule:
     * 20% of the unique address space forms the "hot" partition.
     * The remaining 80% forms the "cold" background address space.
     */
    int hot_count = (int)((double)max_address * 0.20);
    if (hot_count < 1) {
        hot_count = 1;
    }
    int cold_count = max_address - hot_count;
    if (cold_count < 1) {
        cold_count = 1;
    }

    uint32_t hot_bound = (uint32_t)hot_count;
    uint32_t cold_bound = (uint32_t)cold_count;

    for (int i = 0; i < size; ++i) {
        int chance = rand() % 100;

        if (chance < 80) {
            /* 80% probability: Access the hot working set [0, hot_count - 1] */
            trace[i] = (int)(rand_u32() % hot_bound);
        } else {
            /* 20% probability: Access the cold address space [hot_count, max_address - 1] */
            trace[i] = hot_count + (int)(rand_u32() % cold_bound);
        }
    }

    return trace;
}

void workload_free(int *trace) {
    if (trace) {
        free(trace);
    }
}
