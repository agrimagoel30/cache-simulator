/**
 * ============================================================================
 * @file main.c
 * @brief Custom Memory Cache Simulator Benchmark Driver
 *
 * @details
 * Simulation Engine for Systems & Performance Portfolio.
 * Evaluates FIFO vs. LRU cache replacement policies under identical memory
 * traces.
 *
 * Methodology:
 *   1. Hardware Cache Model: 1,000 cache lines (slots).
 *   2. Trace Generation: 1,000,000 accesses generated via Temporal Locality (80/20).
 *   3. Identical Trace Feed: Both caches consume the exact same address sequence.
 *   4. Timing: POSIX clock_gettime(CLOCK_MONOTONIC) for nanosecond precision.
 *   5. Comparative Reporting: Hit Rate (%), Miss Rate (%), Latency (ms),
 *      Latency per access (ns), and Throughput (Mops/s).
 *   6. Comprehensive Multi-Workload Suite: Evaluates Temporal Locality,
 *      Sequential Streaming, and Uniform Random to illustrate architectural trade-offs.
 *
 * Target: Systems & Performance Engineering Portfolio
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "cache_types.h"
#include "fifo_cache.h"
#include "lru_cache.h"
#include "workload.h"

/* Simulation Parameters specified by prompt */
#define SIM_CACHE_CAPACITY       1000
#define SIM_TRACE_SIZE           1000000
#define SIM_ADDRESS_SPACE        10000

/**
 * @struct BenchmarkResult
 * @brief Metrics recorded during a benchmark pass.
 */
typedef struct {
    const char *policy_name;
    uint64_t    total_accesses;
    uint64_t    hits;
    uint64_t    misses;
    double      hit_rate;
    double      miss_rate;
    double      elapsed_ms;
    double      ns_per_access;
    double      throughput_mops;
} BenchmarkResult;

/**
 * @brief Executes a timed simulation run on a cache using a given trace.
 */
static BenchmarkResult run_benchmark(Cache *cache, const int *trace, int trace_size) {
    BenchmarkResult res;
    res.policy_name = cache->name;
    res.total_accesses = (uint64_t)trace_size;

    /* Flush cache contents and reset counters */
    cache_reset(cache);

    struct timespec start, end;

    /* High-resolution timer start */
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < trace_size; ++i) {
        cache_access(cache, trace[i]);
    }

    /* High-resolution timer end */
    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Calculate elapsed time */
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    res.elapsed_ms = (sec * 1000.0) + (nsec / 1000000.0);

    res.hits = cache->hits;
    res.misses = cache->misses;
    res.hit_rate = cache_get_hit_rate(cache);
    res.miss_rate = cache_get_miss_rate(cache);

    /* Performance indices */
    double total_ns = (sec * 1e9) + nsec;
    res.ns_per_access = total_ns / (double)trace_size;
    res.throughput_mops = ((double)trace_size / (res.elapsed_ms / 1000.0)) / 1e6;

    return res;
}

/**
 * @brief Prints an ASCII results table comparing two benchmark results.
 */
static void print_comparison_table(const char *workload_name,
                                   const BenchmarkResult *fifo_res,
                                   const BenchmarkResult *lru_res) {
    printf("\n========================================================================================\n");
    printf(" WORKLOAD: %s\n", workload_name);
    printf("========================================================================================\n");
    printf("%-28s | %-12s | %-12s | %-10s | %-11s | %-10s\n",
           "Cache Replacement Policy", "Hits", "Misses", "Hit Rate", "Latency", "Throughput");
    printf("-----------------------------+--------------+--------------+------------+-------------+-----------\n");

    printf("%-28s | %12llu | %12llu | %9.2f%% | %8.2f ms | %6.2f Mops\n",
           fifo_res->policy_name,
           (unsigned long long)fifo_res->hits,
           (unsigned long long)fifo_res->misses,
           fifo_res->hit_rate,
           fifo_res->elapsed_ms,
           fifo_res->throughput_mops);

    printf("%-28s | %12llu | %12llu | %9.2f%% | %8.2f ms | %6.2f Mops\n",
           lru_res->policy_name,
           (unsigned long long)lru_res->hits,
           (unsigned long long)lru_res->misses,
           lru_res->hit_rate,
           lru_res->elapsed_ms,
           lru_res->throughput_mops);

    printf("-----------------------------+--------------+--------------+------------+-------------+-----------\n");

    double hit_rate_diff = lru_res->hit_rate - fifo_res->hit_rate;
    printf(" Delta (LRU vs FIFO):         | Hit Rate: %+6.2f%% | Latency per access: FIFO = %.1f ns, LRU = %.1f ns\n",
           hit_rate_diff, fifo_res->ns_per_access, lru_res->ns_per_access);
    printf("========================================================================================\n");
}

int main(void) {
    printf("========================================================================================\n");
    printf("               High-Performance Memory Cache Simulator (C11)                   \n");
    printf("========================================================================================\n");
    printf(" Configuration:\n");
    printf("   - Cache Capacity       : %d slots\n", SIM_CACHE_CAPACITY);
    printf("   - Trace Access Count   : %d memory references\n", SIM_TRACE_SIZE);
    printf("   - Address Space Size   : %d unique addresses\n", SIM_ADDRESS_SPACE);
    printf("   - High-Res Timer       : POSIX clock_gettime(CLOCK_MONOTONIC)\n");
    printf("========================================================================================\n");

    /* Seed PRNG for reproducible runs (or use time(NULL) for stochastic) */
    srand(42);

    /* Allocate Cache Instances */
    Cache *fifo = fifo_cache_create(SIM_CACHE_CAPACITY);
    Cache *lru  = lru_cache_create(SIM_CACHE_CAPACITY);

    if (!fifo || !lru) {
        fprintf(stderr, "FATAL: Failed to initialize cache instances.\n");
        if (fifo) cache_destroy(fifo);
        if (lru) cache_destroy(lru);
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------------------
     * Primary Benchmark: Temporal Locality (80/20 Pareto Distribution)
     * ------------------------------------------------------------------------
     * Models real-world compute and graphics kernels where hot buffers/textures
     * dominate memory traffic.
     */
    printf("\n[1/3] Generating Temporal Locality Trace (80/20 Rule)...\n");
    int *temporal_trace = generate_temporal_locality(SIM_TRACE_SIZE, SIM_ADDRESS_SPACE);
    if (!temporal_trace) {
        fprintf(stderr, "FATAL: Memory allocation failed for temporal trace.\n");
        cache_destroy(fifo);
        cache_destroy(lru);
        return EXIT_FAILURE;
    }

    printf("      Benchmarking FIFO on Temporal Locality Trace...\n");
    BenchmarkResult fifo_temp = run_benchmark(fifo, temporal_trace, SIM_TRACE_SIZE);

    printf("      Benchmarking LRU on Temporal Locality Trace...\n");
    BenchmarkResult lru_temp = run_benchmark(lru, temporal_trace, SIM_TRACE_SIZE);

    print_comparison_table("Temporal Locality (80/20 Pareto: 20% addresses -> 80% traffic)",
                           &fifo_temp, &lru_temp);

    /* Free primary trace */
    workload_free(temporal_trace);
    temporal_trace = NULL;

    /* ------------------------------------------------------------------------
     * Secondary Benchmark: Uniform Random Access
     * ------------------------------------------------------------------------
     * Models pointer chasing, graph traversal, and hash lookups where temporal
     * correlation is minimal.
     */
    printf("\n[2/3] Generating Uniform Random Trace...\n");
    int *random_trace = generate_random(SIM_TRACE_SIZE, SIM_ADDRESS_SPACE);
    if (random_trace) {
        printf("      Benchmarking FIFO on Uniform Random Trace...\n");
        BenchmarkResult fifo_rand = run_benchmark(fifo, random_trace, SIM_TRACE_SIZE);

        printf("      Benchmarking LRU on Uniform Random Trace...\n");
        BenchmarkResult lru_rand = run_benchmark(lru, random_trace, SIM_TRACE_SIZE);

        print_comparison_table("Uniform Random Access (Zero Locality)",
                               &fifo_rand, &lru_rand);

        workload_free(random_trace);
        random_trace = NULL;
    }

    /* ------------------------------------------------------------------------
     * Tertiary Benchmark: Sequential Streaming Loop (Working Set > Capacity)
     * ------------------------------------------------------------------------
     * Loop range = 1,200 elements, while cache capacity = 1,000 slots.
     * Demonstrates cache thrashing when loop bounds exceed cache size.
     */
    printf("\n[3/3] Generating Sequential Streaming Trace (Range = 1,200 > Capacity = 1,000)...\n");
    int *seq_trace = generate_sequential(SIM_TRACE_SIZE, 1200);
    if (seq_trace) {
        printf("      Benchmarking FIFO on Sequential Trace...\n");
        BenchmarkResult fifo_seq = run_benchmark(fifo, seq_trace, SIM_TRACE_SIZE);

        printf("      Benchmarking LRU on Sequential Trace...\n");
        BenchmarkResult lru_seq = run_benchmark(lru, seq_trace, SIM_TRACE_SIZE);

        print_comparison_table("Sequential Streaming Loop (Working Set > Capacity -> Thrashing)",
                               &fifo_seq, &lru_seq);

        workload_free(seq_trace);
        seq_trace = NULL;
    }

    /* ------------------------------------------------------------------------
     * Systems Architectural Summary & Technical Discussion
     * ------------------------------------------------------------------------
     */
    printf("\n========================================================================================\n");
    printf("                           TECHNICAL ARCHITECTURE ANALYSIS                              \n");
    printf("========================================================================================\n");
    printf(" 1. Temporal Locality Advantage:\n");
    printf("    LRU retains frequently accessed 'hot' lines at the MRU head, achieving\n");
    printf("    substantially higher hit rates than FIFO under skewed workloads.\n");
    printf("    FIFO suffers because hot lines that entered earlier are blindly evicted.\n\n");
    printf(" 2. Throughput & Splicing Overhead:\n");
    printf("    FIFO achieves slightly higher throughput (Mops) because a cache HIT requires\n");
    printf("    only an O(1) hash lookup with zero data structure mutation. In contrast, LRU\n");
    printf("    must mutate 4 pointers on EVERY hit to splice the node to the MRU position.\n\n");
    printf(" 3. GPU / Hardware Cache Parallels (Systems & Architecture Context):\n");
    printf("    In GPU L1/L2 caches, true LRU tracking across hundreds of warps requires\n");
    printf("    prohibitive hardware state bits and wiring. Modern GPUs frequently deploy\n");
    printf("    Pseudo-LRU (Tree-PLRU, Bit-PLRU) or streaming bypass modes (e.g. LDG.CS)\n");
    printf("    to balance LRU hit rates with FIFO-like hardware simplicity.\n");
    printf("========================================================================================\n\n");

    /* ------------------------------------------------------------------------
     * Strict Memory Cleanup (Valgrind-Clean Invariant)
     * ------------------------------------------------------------------------
     */
    cache_destroy(fifo);
    cache_destroy(lru);

    printf("Simulation completed successfully. All allocated memory cleanly freed.\n");
    return EXIT_SUCCESS;
}
