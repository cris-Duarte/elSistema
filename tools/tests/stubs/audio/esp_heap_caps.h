#pragma once
#include <cstddef>
#include <cstdlib>
constexpr int MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2;
inline bool audio_test_fail_allocation = false;
inline size_t audio_test_allocations = 0;
inline void* heap_caps_malloc(size_t size, int) {
    if (audio_test_fail_allocation) return nullptr;
    void* p = std::malloc(size);
    if (p) ++audio_test_allocations;
    return p;
}
inline void heap_caps_free(void* p) {
    if (p) { --audio_test_allocations; std::free(p); }
}
