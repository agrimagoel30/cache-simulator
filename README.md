# Custom Memory Cache Simulator (C11)
### High-Performance $O(1)$ FIFO vs. LRU Evaluation Under Algorithmic Workloads
**Target Focus:** Systems, Computer Architecture & Performance Engineering

---

## 1. Executive Summary

In high-performance computing, GPU kernel optimization, and systems software, memory hierarchy efficiency dictates overall application throughput. This project implements an **architecturally authentic, $O(1)$ Memory Cache Simulator in pure C (C11 standard)** to evaluate the performance trade-offs between **First-In, First-Out (FIFO)** and **Least Recently Used (LRU)** cache replacement policies.

Because pure C lacks a Standard Template Library (STL), all foundational data structures—including **Separate-Chaining Hash Sets**, **Sentinel Doubly Linked Lists**, **Hash Tables with Direct Node-Pointer Indexing**, and **Contiguous Circular Queues**—are engineered from scratch with zero external dependencies and strict memory safety.

---

## 2. Architectural Design & Polymorphic Interface

To simulate production hardware caches cleanly, the simulator employs an object-oriented polymorphic pattern in C using function pointers (VTable abstraction). This decouples benchmark drivers from cache replacement mechanics.

```
+-----------------------------------------------------------------------------------+
|                                    Cache                                          |
|  - name: const char*                                                              |
|  - capacity: size_t                                                               |
|  - hits: uint64_t                                                                 |
|  - misses: uint64_t                                                               |
|  - access(): bool (*)(Cache*, int address)                                        |
|  - reset():  void (*)(Cache*)                                                     |
|  - destroy(): void (*)(Cache*)                                                    |
|  - impl: void*                                                                    |
+-----------------------------------------------------------------------------------+
                  ▲                                                 ▲
                  │                                                 │
                  │ (impl)                                          │ (impl)
+------------------------------------+           +------------------------------------+
|           FIFOCacheImpl            |           |            LRUCacheImpl            |
|  - queue: CircularQueue (Array)    |           |  - head, tail: Sentinel DLLNode*   |
|  - set:   HashSet (Chaining)       |           |  - table: LRUHashTable (Key->Node*)|
|  - Eviction: Head of Circular Q    |           |  - Eviction: Predecessor of Tail   |
+------------------------------------+           +------------------------------------+
```

---

## 3. Data Structures & $O(1)$ Algorithms

### A. LRU Cache: $O(1)$ Hit Splicing via Hash-to-Node Pointer Mapping

A standard Doubly Linked List can insert and delete nodes in $O(1)$ time **only if a direct pointer to the target node is already held**. Without an index, finding an address in a linked list degrades to an unacceptable $O(N)$ linear traversal.

#### The Core Solution:
1. **Custom Hash Table (`LRUHashTable`)**: Maps `int address` $\rightarrow$ `DLLNode*`.
2. **Sentinel Doubly Linked List**: Maintains temporal recency order.
   - `dummy_head`: Adjacent to the Most Recently Used (MRU) line.
   - `dummy_tail`: Adjacent to the Least Recently Used (LRU) line.

```
       Hash Table Bucket Array
      +------------------------+
 [0]  | Key: 0x1A -> DLLNode*  | -----------------------+
      +------------------------+                        |
 [1]  | Key: 0x4F -> DLLNode*  | ----------+             |
      +------------------------+           |             |
                                           ▼             ▼
       Sentinel Head                                           Sentinel Tail
      +-------------+        +-------------+        +-------------+        +-------------+
      |  dummy_head | <----> |  DLLNode A  | <----> |  DLLNode B  | <----> |  dummy_tail |
      +-------------+        | (Addr 0x4F) |        | (Addr 0x1A) |        +-------------+
            (MRU)            +-------------+        +-------------+             (LRU)
```

#### Cache Hit Protocol ($O(1)$):
1. **Index Lookup**: Query the hash table with the memory address. Returns `DLLNode *node` in $O(1)$ average time.
2. **Branchless Splicing**: Detach `node` from its current position in the list:
   ```c
   node->prev->next = node->next;
   node->next->prev = node->prev;
   ```
3. **MRU Promotion**: Insert `node` immediately behind `dummy_head`:
   ```c
   node->next = head->next;
   node->prev = head;
   head->next->prev = node;
   head->next = node;
   ```
4. **No Loop, No Reallocation**: Entire operation completes in single-digit CPU clock cycles.

#### Cache Miss & Node Recycling Optimization ($O(1)$):
When capacity is reached:
1. The eviction candidate is always `dummy_tail->prev` (LRU).
2. The old address tag is purged from the hash table.
3. **Zero-Allocation Node Recycling**: Instead of invoking `free(victim)` and `malloc(new_node)`—which triggers heap allocator lock contention and memory fragmentation—the simulator overwrites `victim->address = new_address`, relinks it to MRU, and registers the new mapping in the hash table.

---

### B. FIFO Cache: Circular Queue + Hash Set ($O(1)$)

FIFO ignores access recency on hits. An entry's eviction eligibility depends solely on when it first entered the cache.

1. **Circular Buffer Queue (`CircularQueue`)**: A contiguous array of integers of size `capacity`.
   - `head`: Dequeue index (oldest address to evict).
   - `tail`: Enqueue index (new address entry).
   - $O(1)$ enqueue/dequeue with zero pointer-chasing and optimal CPU cache line locality.
2. **Hash Set (`HashSet`)**: Separate-chaining hash set with power-of-two bucket count ($M = 2^{\lceil \log_2(2C) \rceil}$) ensuring load factor $\alpha \le 0.5$. Provides $O(1)$ existence checks.
3. **Hit Invariant**: On a hit, FIFO does **not** mutate the queue or hash set, yielding slightly lower hit latency than LRU at the cost of vulnerability to cache pollution.

---

### C. Thomas Wang 32-Bit Integer Hash Mixer

In memory simulation, address traces often exhibit strides (e.g., 4-byte `int`, 64-byte cache lines, or 256-byte GPU alignment). Direct modulo hashing produces massive bucket clustering. We use an integer bitwise mixer:

```c
static inline uint32_t hash_int32(int key) {
    uint32_t x = (uint32_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = (x >> 16) ^ x;
    return x;
}
```
Combined with bitmasking (`idx = hash & (bucket_count - 1)`), bucket resolution executes in $\approx 1$ nanosecond.

---

## 4. Workload Generators

| Generator | Signature | Access Pattern | Architectural Relevance |
| :--- | :--- | :--- | :--- |
| **Temporal Locality** | `generate_temporal_locality(size, max_addr)` | **80/20 Pareto Rule**: 20% of addresses receive 80% of traffic. | Simulates compute kernels, hot texture/buffer reuse, and iterative numerical solvers. |
| **Uniform Random** | `generate_random(size, max_addr)` | Equal probability across all addresses. | Simulates pointer chasing, graph traversal, and sparse linear algebra. |
| **Sequential Streaming** | `generate_sequential(size, loop_range)` | Monotonically increasing address stride. | Simulates streaming kernels, image filtering, and loop thrashing when $N > C$. |

---

## 5. Experimental Benchmark Results

### Benchmark Setup
- **Cache Capacity ($C$)**: 1,000 slots (lines)
- **Workload Volume**: 1,000,000 memory accesses per pass
- **Timer**: POSIX `clock_gettime(CLOCK_MONOTONIC)` (nanosecond resolution)
- **Optimization**: GCC 13.2 `-O3 -std=c11`

### Comparative Results Matrix

```
========================================================================================
 WORKLOAD 1: Temporal Locality (80/20 Pareto: 20% addresses -> 80% traffic)
========================================================================================
Cache Replacement Policy     | Hits         | Misses       | Hit Rate   | Latency     | Throughput
-----------------------------+--------------+--------------+------------+-------------+-----------
FIFO (First-In, First-Out)   |       296913 |       703087 |     29.69% |    41.34 ms |  24.19 Mops
LRU (Least Recently Used)    |       311052 |       688948 |     31.11% |    50.58 ms |  19.77 Mops
-----------------------------+--------------+--------------+------------+-------------+-----------
 Delta (LRU vs FIFO):         | Hit Rate:  +1.41% | Latency per access: FIFO = 41.3 ns, LRU = 50.6 ns
========================================================================================

========================================================================================
 WORKLOAD 2: Uniform Random Access (Zero Locality)
========================================================================================
Cache Replacement Policy     | Hits         | Misses       | Hit Rate   | Latency     | Throughput
-----------------------------+--------------+--------------+------------+-------------+-----------
FIFO (First-In, First-Out)   |       100298 |       899702 |     10.03% |    47.23 ms |  21.17 Mops
LRU (Least Recently Used)    |       100321 |       899679 |     10.03% |    47.56 ms |  21.03 Mops
-----------------------------+--------------+--------------+------------+-------------+-----------
 Delta (LRU vs FIFO):         | Hit Rate:  +0.00% | Latency per access: FIFO = 47.2 ns, LRU = 47.6 ns
========================================================================================

========================================================================================
 WORKLOAD 3: Sequential Streaming Loop (Working Set = 1,200 > Capacity = 1,000)
========================================================================================
Cache Replacement Policy     | Hits         | Misses       | Hit Rate   | Latency     | Throughput
-----------------------------+--------------+--------------+------------+-------------+-----------
FIFO (First-In, First-Out)   |            0 |      1000000 |      0.00% |    44.36 ms |  22.54 Mops
LRU (Least Recently Used)    |            0 |      1000000 |      0.00% |    43.02 ms |  23.24 Mops
-----------------------------+--------------+--------------+------------+-------------+-----------
 Delta (LRU vs FIFO):         | Hit Rate:  +0.00% | Latency per access: FIFO = 44.4 ns, LRU = 43.0 ns
========================================================================================
```

---

## 6. Technical Discussion Points for Systems Engineering Interviews

### 1. The Hit-Rate vs. Throughput Trade-off
- **LRU Superior Hit Rate**: Under temporal locality, LRU outperforms FIFO because it pins the active working set in cache. When a hot line is accessed, it is immediately promoted to MRU. In FIFO, heavily accessed lines are evicted simply because they were loaded earlier.
- **FIFO Splicing Latency Advantage**: Notice that FIFO achieves **$\approx 24.2$ Mops/s** versus LRU's **$\approx 19.8$ Mops/s**. On a cache hit, FIFO does **zero** pointer mutations (pure lookup). In contrast, LRU must mutate 4 pointers (`prev->next`, `next->prev`, and MRU insertions) on **every hit**. In latency-critical software or hardware controllers, metadata maintenance can become a bottleneck.

### 2. GPU Hardware Cache Parallels (Ampere / Ada / Hopper / Blackwell)
- In GPU hardware, maintaining a full true LRU state for a 16-way or 32-way associative cache across thousands of concurrent threads (warps) is prohibitive due to the wire routing and area cost of recency matrices.
- Modern GPUs employ **Pseudo-LRU (PLRU)** variants (such as Tree-PLRU or Bit-PLRU) or **Cache Eviction Priorities / Streaming Bypasses** (e.g., PTX cache operators `ld.global.cs` [streaming / evict-first] and `ld.global.lu` [last-use]).
- When streaming through a large dataset (Workload 3), standard LRU suffers total cache thrashing ($0\%$ hit rate) while evicting all other cached data. Performance engineers use cache bypass modifiers to stream data without polluting L1/L2 caches.

### 3. Belady's Anomaly
- **FIFO** is susceptible to Belady's Anomaly, where increasing cache capacity can paradoxically *increase* the number of cache misses for specific access sequences.
- **LRU** belongs to the family of **Stack Algorithms**, mathematically guaranteeing that the contents of an $M$-slot cache are always a strict subset of an $(M+1)$-slot cache, making it immune to Belady's Anomaly.

---

## 7. Build and Verification Instructions

### Directory Structure
```
cache_project/
├── CMakeLists.txt          # Modern C11 CMake build configuration
├── Makefile                # Cross-platform Makefile for GCC/MinGW
├── README.md               # Technical documentation & interview guide
├── include/
│   ├── cache_types.h       # Polymorphic interface & statistics tracking
│   ├── fifo_cache.h        # FIFO cache API
│   ├── lru_cache.h         # LRU cache API
│   └── workload.h          # Synthetic workload generators
└── src/
    ├── fifo_cache.c        # Custom circular queue + hash set implementation
    ├── lru_cache.c         # Custom sentinel DLL + hash table implementation
    ├── workload.c          # Workload trace generation engine
    └── main.c              # POSIX benchmark harness & reporting dashboard
```

### Build via CMake (Recommended)
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
./cache_sim
```

### Build via Makefile (GCC / MinGW)
```bash
mingw32-make clean
mingw32-make
./bin/cache_sim
```

### Direct GCC Command
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -O3 -Iinclude src/fifo_cache.c src/lru_cache.c src/workload.c src/main.c -o bin/cache_sim.exe
```

---

## 8. Memory Safety & Valgrind Invariants

All dynamic memory allocations (`malloc`, `calloc`) are paired with deterministic deallocation routines:
- `cache_destroy()` completely recurses through all hash buckets and doubly linked list nodes.
- `workload_free()` deallocates synthetic trace arrays.
- Zero memory leaks under `valgrind --leak-check=full --show-leak-kinds=all ./cache_sim` and `-fsanitize=address,undefined`.
