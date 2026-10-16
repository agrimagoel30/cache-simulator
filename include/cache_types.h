#ifndef CACHE_TYPES_H
#define CACHE_TYPES_H

/**
 * ============================================================================
 * @file cache_types.h
 * @brief Core Cache Polymorphic Interface and Statistics Definitions
 *
 * @details
 * Defines the abstract interface and unified statistics tracking for memory
 * cache simulator implementations (FIFO, LRU, etc.).
 *
 * In pure C (C11), polymorphism is achieved using an object-oriented pattern
 * with function pointers (VTable-like structure). Simulation harnesses can
 * interact with any cache implementation interchangeably without coupling
 * to the underlying replacement policy or internal data structures.
 *
 * Target: Systems & Performance Engineering Portfolio
 * ============================================================================
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct Cache
 * @brief Forward declaration of the polymorphic Cache handle.
 */
typedef struct Cache Cache;

/**
 * @typedef CacheAccessFn
 * @brief Function pointer signature for probing/inserting an address into the cache.
 * @param self Pointer to the Cache instance.
 * @param address Simulated memory address (tag/index) to access.
 * @return true if access was a cache HIT, false if a cache MISS.
 */
typedef bool (*CacheAccessFn)(Cache *self, int address);

/**
 * @typedef CacheResetFn
 * @brief Function pointer signature for flushing cache lines and resetting statistics.
 * @param self Pointer to the Cache instance.
 */
typedef void (*CacheResetFn)(Cache *self);

/**
 * @typedef CacheDestroyFn
 * @brief Function pointer signature for releasing all dynamically allocated memory.
 * @param self Pointer to the Cache instance.
 */
typedef void (*CacheDestroyFn)(Cache *self);

/**
 * @struct Cache
 * @brief Unified Cache container providing public metadata and dispatch table.
 */
struct Cache {
    const char     *name;        /**< Human-readable algorithm identifier ("FIFO", "LRU") */
    size_t          capacity;    /**< Maximum number of cache lines/slots */
    uint64_t        hits;        /**< Total cumulative cache hits */
    uint64_t        misses;      /**< Total cumulative cache misses */
    CacheAccessFn   access;      /**< Virtual function pointer for cache lookup/insert */
    CacheResetFn    reset;       /**< Virtual function pointer to clear cache contents */
    CacheDestroyFn  destroy;     /**< Virtual function pointer to destruct cache */
    void           *impl;        /**< Opaque pointer to policy-specific internal state */
};

/* ============================================================================
 * Inline Wrapper Helpers for Cache Interface
 * ============================================================================
 */

/**
 * @brief Access an address in the cache. Dispatches to policy-specific implementation.
 * @param cache Pointer to Cache instance.
 * @param address Memory address to access.
 * @return true on cache hit, false on cache miss.
 */
static inline bool cache_access(Cache *cache, int address) {
    return cache->access(cache, address);
}

/**
 * @brief Flush all elements from the cache and reset hit/miss counters to zero.
 * @param cache Pointer to Cache instance.
 */
static inline void cache_reset(Cache *cache) {
    if (cache && cache->reset) {
        cache->reset(cache);
    }
}

/**
 * @brief Release all resources associated with the cache instance.
 * @param cache Pointer to Cache instance.
 */
static inline void cache_destroy(Cache *cache) {
    if (cache && cache->destroy) {
        cache->destroy(cache);
    }
}

/**
 * @brief Calculate the cumulative hit rate as a percentage.
 * @param cache Pointer to Cache instance.
 * @return Hit rate in range [0.0, 100.0]%.
 */
static inline double cache_get_hit_rate(const Cache *cache) {
    uint64_t total = cache->hits + cache->misses;
    return (total == 0) ? 0.0 : ((double)cache->hits / (double)total) * 100.0;
}

/**
 * @brief Calculate the cumulative miss rate as a percentage.
 * @param cache Pointer to Cache instance.
 * @return Miss rate in range [0.0, 100.0]%.
 */
static inline double cache_get_miss_rate(const Cache *cache) {
    uint64_t total = cache->hits + cache->misses;
    return (total == 0) ? 0.0 : ((double)cache->misses / (double)total) * 100.0;
}

/**
 * @brief Retrieve total accesses processed by the cache.
 * @param cache Pointer to Cache instance.
 * @return Total number of accesses (hits + misses).
 */
static inline uint64_t cache_get_total_accesses(const Cache *cache) {
    return cache->hits + cache->misses;
}

#ifdef __cplusplus
}
#endif

#endif /* CACHE_TYPES_H */
