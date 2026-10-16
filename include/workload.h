#ifndef WORKLOAD_H
#define WORKLOAD_H

/**
 * ============================================================================
 * @file workload.h
 * @brief Algorithmic Memory Access Workload Generators
 *
 * @details
 * Generates synthetic memory access address traces to evaluate cache policies
 * under distinct spatial and temporal patterns:
 *   1. Sequential Streaming: Simulates vector operations / streaming access loops.
 *   2. Uniform Random: Simulates random pointer chasing (hash table lookups, graphs).
 *   3. Temporal Locality (80/20 Pareto): Simulates real-world workloads where
 *      a hot working set (20% of addresses) generates 80% of memory traffic.
 *
 * Target: Systems & Performance Engineering Portfolio
 * ============================================================================
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generates a sequential looping memory access trace.
 *
 * Memory addresses monotonically increase from 0 to (loop_range - 1),
 * then wrap around repeatedly.
 *
 * @param size Number of memory accesses to generate.
 * @param loop_range Size of the address stride cycle.
 * @return Dynamically allocated array of integers (caller must free), or NULL.
 */
int *generate_sequential(int size, int loop_range);

/**
 * @brief Generates a uniform random memory access trace.
 *
 * Each access is uniformly distributed across [0, max_address - 1].
 *
 * @param size Number of memory accesses to generate.
 * @param max_address Upper bound (exclusive) of the address space.
 * @return Dynamically allocated array of integers (caller must free), or NULL.
 */
int *generate_random(int size, int max_address);

/**
 * @brief Generates an access trace exhibiting strong temporal locality (80/20 rule).
 *
 * 80% of memory references target the "hot" working set (first 20% of addresses).
 * 20% of memory references target the "cold" remaining address space (80% of addresses).
 *
 * @param size Number of memory accesses to generate.
 * @param max_address Upper bound (exclusive) of the address space.
 * @return Dynamically allocated array of integers (caller must free), or NULL.
 */
int *generate_temporal_locality(int size, int max_address);

/**
 * @brief Safely deallocates a generated memory trace array.
 * @param trace Pointer to the array allocated by generator functions.
 */
void workload_free(int *trace);

#ifdef __cplusplus
}
#endif

#endif /* WORKLOAD_H */
