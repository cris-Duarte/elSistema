#pragma once
#include <cstddef>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
namespace voxel {
#ifdef VOXEL_TEST_ALLOCATOR
void* testAllocate(std::size_t bytes);
void testFree(void* p);
#endif
inline void* allocateExternal(std::size_t bytes) {
#ifdef VOXEL_TEST_ALLOCATOR
    return testAllocate(bytes);
#elif defined(ESP_PLATFORM)
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return std::malloc(bytes);
#endif
}
inline void freeExternal(void* p) {
#ifdef VOXEL_TEST_ALLOCATOR
    testFree(p);
#else
    std::free(p);
#endif
}
}
