cmake_minimum_required(VERSION 3.10)

# ==============================================================================
# High-Performance Systems Portfolio: Custom Memory Cache Simulator (C11)
# ==============================================================================

project(CacheSimulator
    VERSION 1.0.0
    DESCRIPTION "High-Performance O(1) FIFO vs LRU Memory Cache Simulator in C11"
    LANGUAGES C
)

# Enforce C11 standard strictly
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/fifo_cache.c
    src/lru_cache.c
    src/workload.c
    src/main.c
)

# Header files (for IDE visibility)
set(HEADERS
    include/cache_types.h
    include/fifo_cache.h
    include/lru_cache.h
    include/workload.h
)

# Main Executable
add_executable(cache_sim ${SOURCES} ${HEADERS})

# Compiler warnings and optimization flags
if (MSVC)
    target_compile_options(cache_sim PRIVATE
        /W4
        /O2
        /permissive-
    )
else()
    target_compile_options(cache_sim PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wpointer-arith
        -Wcast-qual
        -Wstrict-prototypes
        -Wmissing-prototypes
        -O3
    )
endif()

# POSIX Realtime Library linking on Linux systems (if required)
if (UNIX AND NOT APPLE)
    target_link_libraries(cache_sim PRIVATE rt)
endif()
