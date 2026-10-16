/**
 * ============================================================================
 * @file lru_cache.c
 * @brief O(1) LRU Cache with Custom Sentinel Doubly Linked List & Hash Table
 *
 * @details
 * Architectural Design & Technical Discussion Points:
 *
 * 1. The Fundamental Problem in LRU:
 *    A standard doubly linked list allows O(1) insertion, deletion, and splicing
 *    IF AND ONLY IF a direct pointer to the target node is already held. If one
 *    must search the list for an address, hit latency degrades to O(N), which is
 *    unacceptable in hardware memory controllers and high-throughput software.
 *
 * 2. Hash Table to Node Pointer Mapping:
 *    To solve this, we construct a custom separate-chaining Hash Table that maps:
 *        Address (int)  --->  Doubly Linked List Node (DLLNode *)
 *    When a memory access occurs:
 *      a. Hash Table lookup locates the `DLLNode *` in O(1) average time.
 *      b. If found (HIT), the node is spliced out of its current position in
 *         the Doubly Linked List in O(1) time and prepended to the MRU position.
 *      c. If missing (MISS), an eviction victim is selected from the tail (LRU),
 *         its old mapping is purged from the Hash Table, and the node is updated
 *         and inserted at MRU.
 *
 * 3. Sentinel Head and Tail Nodes (Branchless List Invariants):
 *    We allocate two persistent dummy sentinel nodes: `dummy_head` (MRU side)
 *    and `dummy_tail` (LRU side). Sentinels eliminate special-case checks for
 *    `NULL` head/tail pointers during insertion, detachment, and list-empty
 *    states. This eliminates branch mispredictions in CPU instruction pipelines
 *    on the critical cache access path.
 *
 * 4. Node Recycling Optimization:
 *    Once the cache reaches capacity, evicting an entry and allocating a new one
 *    via `free()` and `malloc()` introduces significant heap allocator locking
 *    and metadata overhead. Instead, we recycle the evicted `victim` node
 *    in-place by updating its address tag and moving it to the MRU position.
 *
 * Memory Safety:
 *    Every dynamically allocated `HashEntry`, `DLLNode`, and bucket array is
 *    exhaustively tracked and deallocated on `lru_cache_destroy()`, guaranteeing
 *    zero memory leaks under Valgrind and AddressSanitizer.
 * ============================================================================
 */

#include "lru_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Hash Function (Thomas Wang 32-bit Integer Mixer)
 * ============================================================================
 */
static inline uint32_t lru_hash_int32(int key) {
    uint32_t x = (uint32_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = (x >> 16) ^ x;
    return x;
}

/* ============================================================================
 * Custom Doubly Linked List (Recency Tracker)
 * ============================================================================
 */

typedef struct DLLNode {
    int             address;  /**< Tag/Address stored in this cache line */
    struct DLLNode *prev;     /**< Pointer toward MRU direction (head) */
    struct DLLNode *next;     /**< Pointer toward LRU direction (tail) */
} DLLNode;

/**
 * @brief Allocates a new DLL node with the specified address.
 */
static DLLNode *dll_node_create(int address) {
    DLLNode *node = (DLLNode *)malloc(sizeof(DLLNode));
    if (!node) return NULL;
    node->address = address;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

/**
 * @brief Splices a node out of its current position in the Doubly Linked List.
 * @note O(1) branchless pointer manipulation guaranteed by sentinel boundaries.
 */
static inline void dll_detach(DLLNode *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

/**
 * @brief Inserts a node directly after the sentinel head (Most Recently Used).
 * @note O(1) branchless pointer manipulation.
 */
static inline void dll_push_mru(DLLNode *head, DLLNode *node) {
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

/* ============================================================================
 * Custom Hash Table (Address -> DLLNode* Direct Pointer Index)
 * ============================================================================
 */

typedef struct LRUHashEntry {
    int                  key;   /**< Memory address key */
    DLLNode             *node;  /**< Direct pointer to DLL node for O(1) splicing */
    struct LRUHashEntry *next;  /**< Separate chaining collision link */
} LRUHashEntry;

typedef struct {
    LRUHashEntry **buckets;
    size_t         bucket_count;
    size_t         mask;
    size_t         count;
} LRUHashTable;

static size_t next_power_of_two(size_t n) {
    if (n == 0) return 1;
    size_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

static bool lru_hashtable_init(LRUHashTable *table, size_t min_capacity) {
    /* Maintain load factor <= 0.5 to keep average bucket chain length ~ 1 */
    size_t buckets = next_power_of_two(min_capacity * 2);
    if (buckets < 16) {
        buckets = 16;
    }

    table->buckets = (LRUHashEntry **)calloc(buckets, sizeof(LRUHashEntry *));
    if (!table->buckets) {
        return false;
    }

    table->bucket_count = buckets;
    table->mask = buckets - 1;
    table->count = 0;
    return true;
}

static inline DLLNode *lru_hashtable_find(const LRUHashTable *table, int key) {
    uint32_t idx = lru_hash_int32(key) & (uint32_t)table->mask;
    LRUHashEntry *curr = table->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return curr->node;
        }
        curr = curr->next;
    }
    return NULL;
}

static bool lru_hashtable_insert(LRUHashTable *table, int key, DLLNode *node) {
    uint32_t idx = lru_hash_int32(key) & (uint32_t)table->mask;

    LRUHashEntry *entry = (LRUHashEntry *)malloc(sizeof(LRUHashEntry));
    if (!entry) {
        return false;
    }
    entry->key = key;
    entry->node = node;
    entry->next = table->buckets[idx];
    table->buckets[idx] = entry;
    table->count++;
    return true;
}

static bool lru_hashtable_remove(LRUHashTable *table, int key) {
    uint32_t idx = lru_hash_int32(key) & (uint32_t)table->mask;
    LRUHashEntry *curr = table->buckets[idx];
    LRUHashEntry *prev = NULL;

    while (curr) {
        if (curr->key == key) {
            if (prev) {
                prev->next = curr->next;
            } else {
                table->buckets[idx] = curr->next;
            }
            free(curr);
            table->count--;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

static void lru_hashtable_clear(LRUHashTable *table) {
    for (size_t i = 0; i < table->bucket_count; ++i) {
        LRUHashEntry *curr = table->buckets[i];
        while (curr) {
            LRUHashEntry *next = curr->next;
            free(curr);
            curr = next;
        }
        table->buckets[i] = NULL;
    }
    table->count = 0;
}

static void lru_hashtable_destroy(LRUHashTable *table) {
    if (table && table->buckets) {
        lru_hashtable_clear(table);
        free(table->buckets);
        table->buckets = NULL;
        table->bucket_count = 0;
        table->mask = 0;
        table->count = 0;
    }
}

/* ============================================================================
 * LRU Cache Internal State
 * ============================================================================
 */

typedef struct {
    DLLNode      *head;      /**< Sentinel head (dummy node adjacent to MRU) */
    DLLNode      *tail;      /**< Sentinel tail (dummy node adjacent to LRU) */
    LRUHashTable  table;     /**< Hash table mapping address -> DLLNode* */
    size_t        count;     /**< Current number of valid cache lines */
} LRUCacheImpl;

/* ============================================================================
 * Polymorphic Cache Callbacks for LRU
 * ============================================================================
 */

static bool lru_cache_access_impl(Cache *self, int address) {
    LRUCacheImpl *impl = (LRUCacheImpl *)self->impl;

    /*
     * Step 1: O(1) Tag Lookup
     * Query the hash table to check if the requested address is cached.
     * The hash table yields the exact DLLNode* without scanning the list.
     */
    DLLNode *node = lru_hashtable_find(&impl->table, address);

    if (node) {
        /* ---------------- CACHE HIT ---------------- */
        self->hits++;

        /*
         * Critical O(1) Node Splicing:
         * 1. Detach node from its current position in the list.
         * 2. Prepend it directly behind the sentinel head (MRU).
         */
        dll_detach(node);
        dll_push_mru(impl->head, node);
        return true;
    }

    /* ---------------- CACHE MISS ---------------- */
    self->misses++;

    if (impl->count == self->capacity) {
        /*
         * Eviction Required:
         * The Least Recently Used (LRU) node is always the predecessor of
         * the sentinel tail: `impl->tail->prev`.
         */
        DLLNode *victim = impl->tail->prev;

        /* Remove the victim's old address from the hash table */
        lru_hashtable_remove(&impl->table, victim->address);

        /* Detach victim from the LRU position */
        dll_detach(victim);

        /*
         * Systems Optimization: Node Recycling
         * Instead of calling free(victim) followed by malloc(new_node),
         * we repurpose the allocated memory of `victim`, update its address,
         * promote it to MRU, and register it in the hash table.
         * This eliminates heap allocator overhead during steady-state churn.
         */
        victim->address = address;
        dll_push_mru(impl->head, victim);
        lru_hashtable_insert(&impl->table, address, victim);

    } else {
        /* Cache has remaining capacity: allocate a new node */
        DLLNode *new_node = dll_node_create(address);
        if (!new_node) {
            /* Fallback in extreme OOM scenarios */
            return false;
        }

        dll_push_mru(impl->head, new_node);
        lru_hashtable_insert(&impl->table, address, new_node);
        impl->count++;
    }

    return false;
}

static void lru_cache_reset_impl(Cache *self) {
    LRUCacheImpl *impl = (LRUCacheImpl *)self->impl;

    /* Free all active DLL nodes between head and tail sentinels */
    DLLNode *curr = impl->head->next;
    while (curr != impl->tail) {
        DLLNode *next = curr->next;
        free(curr);
        curr = next;
    }

    /* Reset sentinel pointers to empty state */
    impl->head->next = impl->tail;
    impl->tail->prev = impl->head;
    impl->count = 0;

    /* Clear all hash table entries */
    lru_hashtable_clear(&impl->table);

    self->hits = 0;
    self->misses = 0;
}

static void lru_cache_destroy_impl(Cache *self) {
    if (!self) return;

    if (self->impl) {
        LRUCacheImpl *impl = (LRUCacheImpl *)self->impl;

        /* Free all active DLL nodes */
        DLLNode *curr = impl->head->next;
        while (curr != impl->tail) {
            DLLNode *next = curr->next;
            free(curr);
            curr = next;
        }

        /* Free sentinel nodes */
        free(impl->head);
        free(impl->tail);

        /* Free hash table and bucket memory */
        lru_hashtable_destroy(&impl->table);

        free(impl);
        self->impl = NULL;
    }
    free(self);
}

/* ============================================================================
 * Public Factory Function
 * ============================================================================
 */

Cache *lru_cache_create(size_t capacity) {
    if (capacity == 0) {
        return NULL;
    }

    Cache *cache = (Cache *)malloc(sizeof(Cache));
    if (!cache) {
        return NULL;
    }

    LRUCacheImpl *impl = (LRUCacheImpl *)malloc(sizeof(LRUCacheImpl));
    if (!impl) {
        free(cache);
        return NULL;
    }

    /* Initialize Sentinel Nodes */
    impl->head = dll_node_create(-1);
    impl->tail = dll_node_create(-1);
    if (!impl->head || !impl->tail) {
        if (impl->head) free(impl->head);
        if (impl->tail) free(impl->tail);
        free(impl);
        free(cache);
        return NULL;
    }

    impl->head->next = impl->tail;
    impl->tail->prev = impl->head;
    impl->count = 0;

    /* Initialize Hash Table */
    if (!lru_hashtable_init(&impl->table, capacity)) {
        free(impl->head);
        free(impl->tail);
        free(impl);
        free(cache);
        return NULL;
    }

    cache->name = "LRU (Least Recently Used)";
    cache->capacity = capacity;
    cache->hits = 0;
    cache->misses = 0;
    cache->access = lru_cache_access_impl;
    cache->reset = lru_cache_reset_impl;
    cache->destroy = lru_cache_destroy_impl;
    cache->impl = impl;

    return cache;
}
