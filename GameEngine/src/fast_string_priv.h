#pragma once

#include "fast_string.h"
#include <string.h>

extern const FastStringAllocator* g_fast_string_allocator;

#define FAST_ALLOC(size) \
    g_fast_string_allocator->alloc(size)

#define FAST_REALLOC(ptr, new_size) \
    g_fast_string_allocator->realloc(ptr, new_size)

#define FAST_FREE(ptr) \
    if (ptr) g_fast_string_allocator->free(ptr)

#define FAST_ALLOC_ALIGNED(size, alignment) \
    (g_fast_string_allocator->alloc_aligned ? \
     g_fast_string_allocator->alloc_aligned(size, alignment) : \
     g_fast_string_allocator->alloc(size))

#define FAST_FREE_ALIGNED(ptr, alignment) \
    if (ptr) { \
        if (g_fast_string_allocator->free_aligned) \
            g_fast_string_allocator->free_aligned(ptr); \
        else \
            g_fast_string_allocator->free(ptr); \
    }

#define FAST_STR_IS_SSO(fs)   (((fs)->flags & FAST_STR_FLAG_SSO) != 0)
#define FAST_STR_IS_HEAP(fs)  (((fs)->flags & FAST_STR_FLAG_HEAP) != 0)
#define FAST_STR_IS_RO(fs)    (((fs)->flags & FAST_STR_FLAG_READONLY) != 0)

static __forceinline char* fast_str_data(FastString* fs) {
    if (FAST_STR_IS_SSO(fs))
        return fs->data.sso_buf;
    if (FAST_STR_IS_HEAP(fs))
        return fs->data.heap_ptr;
    return (char*)fs->data.ro_ptr;
}

static __forceinline const char* fast_str_data_const(const FastString* fs) {
    if (FAST_STR_IS_SSO(fs))
        return fs->data.sso_buf;
    if (FAST_STR_IS_HEAP(fs))
        return fs->data.heap_ptr;
    return fs->data.ro_ptr;
}

#define FAST_STR_MIN_HEAP_ALLOC 64

static __forceinline size_t fast_str_grow_capacity(size_t current_cap, size_t needed) {
    size_t new_cap = current_cap;
    while (new_cap < needed) {
        new_cap = (new_cap * 3) / 2;
        if (new_cap < FAST_STR_MIN_HEAP_ALLOC)
            new_cap = FAST_STR_MIN_HEAP_ALLOC;
    }
    return new_cap;
}