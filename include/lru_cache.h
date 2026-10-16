#ifndef LRU_CACHE_H
#define LRU_CACHE_H

/**
 * ============================================================================
 * @file lru_cache.h
 * @brief O(1) Least Recently Used (LRU) Cache Implementation
 *
 * @details
 * Models a hardware/software LRU cache with strict O(1) time complexity.
 * Implements two tightly-coupled custom data structures from scratch:
 *   1. Custom Sentinel-headed Doubly Linked List (DLL) to track temporal recency.
 *   2. Custom Hash Table (Separate Chaining with Power-of-Two Bitmasking)
 *      mapping memory addresses directly to DLLNode pointers.
 *
 * This direct pointer mapping allows O(1) node splicing (detach + move to MRU)
 * on cache hits without scanning the list.
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
 * @brief Factory function to allocate and initialize a new LRU Cache.
 *
 * @param capacity Maximum number of entries (cache lines) the cache can hold.
 * @return Pointer to a fully initialized polymorphic Cache structure, or NULL on error.
 */
Cache *lru_cache_create(size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* LRU_CACHE_H */
