/**
 * ============================================================================
 * @file fifo_cache.c
 * @brief O(1) FIFO Cache with Custom Circular Queue and Hash Set
 *
 * @details
 * Architectural Design:
 *   - Eviction Order: Tracked via a contiguous array-based Circular Buffer.
 *     Enqueues at tail, dequeues at head. Both operations are O(1) with zero
 *     pointer indirection overhead and optimal spatial cache locality.
 *   - Membership Lookup: Handled by a custom Hash Set using separate chaining.
 *     Bucket array size is sized to the next power-of-two >= 2 * capacity to
 *     guarantee load factor alpha <= 0.5. Bucket index calculation uses fast
 *     bitwise AND masking rather than expensive integer division (modulo).
 *
 * Systems Engineering Note:
 *   FIFO ignores access recency on hits. An address accessed 1,000 times will
 *   still be evicted when its FIFO turn arrives. This makes FIFO computationally
 *   simpler than LRU (no list manipulation on hits), but prone to Belady's
 *   Anomaly and severe cache pollution under skewed temporal locality.
 * ============================================================================
 */

#include "fifo_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Hash Function (Thomas Wang 32-bit Integer Mixer)
 * ============================================================================
 * Scrambles input bits to provide uniform distribution across hash buckets,
 * preventing clustering when memory addresses have structured strides (e.g.
 * 4-byte int, 64-byte cache lines, or 256-byte GPU alignment).
 */
static inline uint32_t fifo_hash_int32(int key) {
    uint32_t x = (uint32_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = (x >> 16) ^ x;
    return x;
}

/* ============================================================================
 * Custom Hash Set Data Structure (Separate Chaining)
 * ============================================================================
 */

typedef struct HashSetNode {
    int key;
    struct HashSetNode *next;
} HashSetNode;

typedef struct {
    HashSetNode **buckets;
    size_t bucket_count;
    size_t mask;
    size_t element_count;
} HashSet;

/**
 * @brief Computes the smallest power of 2 greater than or equal to n.
 */
static size_t next_power_of_two(size_t n) {
    if (n == 0) return 1;
    size_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

static bool hashset_init(HashSet *set, size_t min_capacity) {
    /* Target load factor <= 0.5 */
    size_t buckets = next_power_of_two(min_capacity * 2);
    if (buckets < 16) {
        buckets = 16;
    }

    set->buckets = (HashSetNode **)calloc(buckets, sizeof(HashSetNode *));
    if (!set->buckets) {
        return false;
    }

    set->bucket_count = buckets;
    set->mask = buckets - 1;
    set->element_count = 0;
    return true;
}

static bool hashset_contains(const HashSet *set, int key) {
    uint32_t idx = fifo_hash_int32(key) & (uint32_t)set->mask;
    HashSetNode *curr = set->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}

static bool hashset_insert(HashSet *set, int key) {
    uint32_t idx = fifo_hash_int32(key) & (uint32_t)set->mask;

    /* Verify uniqueness */
    HashSetNode *curr = set->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return false; /* Already present */
        }
        curr = curr->next;
    }

    /* Allocate and prepend new node to bucket chain (O(1)) */
    HashSetNode *new_node = (HashSetNode *)malloc(sizeof(HashSetNode));
    if (!new_node) {
        return false;
    }
    new_node->key = key;
    new_node->next = set->buckets[idx];
    set->buckets[idx] = new_node;
    set->element_count++;
    return true;
}

static bool hashset_remove(HashSet *set, int key) {
    uint32_t idx = fifo_hash_int32(key) & (uint32_t)set->mask;
    HashSetNode *curr = set->buckets[idx];
    HashSetNode *prev = NULL;

    while (curr) {
        if (curr->key == key) {
            if (prev) {
                prev->next = curr->next;
            } else {
                set->buckets[idx] = curr->next;
            }
            free(curr);
            set->element_count--;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

static void hashset_clear(HashSet *set) {
    for (size_t i = 0; i < set->bucket_count; ++i) {
        HashSetNode *curr = set->buckets[i];
        while (curr) {
            HashSetNode *next = curr->next;
            free(curr);
            curr = next;
        }
        set->buckets[i] = NULL;
    }
    set->element_count = 0;
}

static void hashset_destroy(HashSet *set) {
    if (set && set->buckets) {
        hashset_clear(set);
        free(set->buckets);
        set->buckets = NULL;
        set->bucket_count = 0;
        set->mask = 0;
        set->element_count = 0;
    }
}

/* ============================================================================
 * Custom Circular Buffer (Array-based Queue)
 * ============================================================================
 */

typedef struct {
    int    *data;
    size_t  capacity;
    size_t  head;
    size_t  tail;
    size_t  count;
} CircularQueue;

static bool circular_queue_init(CircularQueue *q, size_t capacity) {
    q->data = (int *)malloc(capacity * sizeof(int));
    if (!q->data) {
        return false;
    }
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    return true;
}

static inline void circular_queue_enqueue(CircularQueue *q, int address) {
    q->data[q->tail] = address;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
}

static inline int circular_queue_dequeue(CircularQueue *q) {
    int item = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return item;
}

static void circular_queue_clear(CircularQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

static void circular_queue_destroy(CircularQueue *q) {
    if (q && q->data) {
        free(q->data);
        q->data = NULL;
        q->capacity = 0;
        q->head = 0;
        q->tail = 0;
        q->count = 0;
    }
}

/* ============================================================================
 * FIFO Cache Internal State
 * ============================================================================
 */

typedef struct {
    CircularQueue queue;  /**< Tracks FIFO arrival order */
    HashSet       set;    /**< Provides O(1) membership lookup */
} FIFOCacheImpl;

/* ============================================================================
 * Polymorphic Cache Callbacks for FIFO
 * ============================================================================
 */

static bool fifo_cache_access_impl(Cache *self, int address) {
    FIFOCacheImpl *impl = (FIFOCacheImpl *)self->impl;

    /* Step 1: O(1) lookup in custom Hash Set */
    if (hashset_contains(&impl->set, address)) {
        self->hits++;
        /* FIFO invariant: Hits do NOT modify queue position or eviction order */
        return true;
    }

    /* Cache Miss */
    self->misses++;

    /* Step 2: Evict oldest entry if cache is at capacity */
    if (impl->queue.count == self->capacity) {
        int evicted_address = circular_queue_dequeue(&impl->queue);
        hashset_remove(&impl->set, evicted_address);
    }

    /* Step 3: Insert new entry into circular queue and hash set */
    circular_queue_enqueue(&impl->queue, address);
    hashset_insert(&impl->set, address);

    return false;
}

static void fifo_cache_reset_impl(Cache *self) {
    FIFOCacheImpl *impl = (FIFOCacheImpl *)self->impl;
    circular_queue_clear(&impl->queue);
    hashset_clear(&impl->set);
    self->hits = 0;
    self->misses = 0;
}

static void fifo_cache_destroy_impl(Cache *self) {
    if (!self) return;

    if (self->impl) {
        FIFOCacheImpl *impl = (FIFOCacheImpl *)self->impl;
        circular_queue_destroy(&impl->queue);
        hashset_destroy(&impl->set);
        free(impl);
        self->impl = NULL;
    }
    free(self);
}

/* ============================================================================
 * Public Factory Function
 * ============================================================================
 */

Cache *fifo_cache_create(size_t capacity) {
    if (capacity == 0) {
        return NULL;
    }

    Cache *cache = (Cache *)malloc(sizeof(Cache));
    if (!cache) {
        return NULL;
    }

    FIFOCacheImpl *impl = (FIFOCacheImpl *)malloc(sizeof(FIFOCacheImpl));
    if (!impl) {
        free(cache);
        return NULL;
    }

    if (!circular_queue_init(&impl->queue, capacity)) {
        free(impl);
        free(cache);
        return NULL;
    }

    if (!hashset_init(&impl->set, capacity)) {
        circular_queue_destroy(&impl->queue);
        free(impl);
        free(cache);
        return NULL;
    }

    cache->name = "FIFO (First-In, First-Out)";
    cache->capacity = capacity;
    cache->hits = 0;
    cache->misses = 0;
    cache->access = fifo_cache_access_impl;
    cache->reset = fifo_cache_reset_impl;
    cache->destroy = fifo_cache_destroy_impl;
    cache->impl = impl;

    return cache;
}
