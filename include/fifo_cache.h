#ifndef FIFO_CACHE_H
#define FIFO_CACHE_H

/**
 * ============================================================================
 * @file fifo_cache.h
 * @brief O(1) First-In, First-Out (FIFO) Cache Implementation
 *
 * @details
 * Models a hardware/software FIFO cache with strict O(1) time complexity.
 * Implements two custom data structures from scratch:
 *   1. Custom Circular Buffer (Array-based Queue) to track insertion/eviction order.
 *   2. Custom Hash Set (Separate Chaining with Power-of-Two Bitmasking)
 *      for O(1) tag lookup without direct-mapped address space constraints.
 *
 * Target: Systems & Performance Engineering Portfolio
 * ============================================================================
 */

#include "cache_types.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Factory function to allocate and initialize a new FIFO Cache.
 *
 * @param capacity Maximum number of entries (cache lines) the cache can hold.
 * @return Pointer to a fully initialized polymorphic Cache structure, or NULL on error.
 */
Cache *fifo_cache_create(size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_CACHE_H */
