#pragma once

#ifndef FAST_STRING_H
#define FAST_STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef FAST_STRING_EXPORTS
#define FAST_STRING_API __declspec(dllexport)
#else
#define FAST_STRING_API __declspec(dllimport)
#endif

typedef struct {
    void* (*alloc)(size_t size);
    void* (*realloc)(void* ptr, size_t new_size);
    void  (*free)(void* ptr);
    void* (*alloc_aligned)(size_t size, size_t alignment);
    void  (*free_aligned)(void* ptr);
} FastStringAllocator;

FAST_STRING_API extern const FastStringAllocator* fast_string_default_allocator(void);
FAST_STRING_API extern const FastStringAllocator* fast_string_system_allocator(void);

FAST_STRING_API void fast_string_set_allocator(const FastStringAllocator* allocator);

FAST_STRING_API const FastStringAllocator* fast_string_get_allocator(void);

#define FAST_STRING_SSO_SIZE 24

typedef enum {
    FAST_STR_FLAG_SSO = 0x01,
    FAST_STR_FLAG_HEAP = 0x02,
    FAST_STR_FLAG_READONLY = 0x04
} FastStringFlags;

typedef struct {
    union {
        char* heap_ptr;
        char sso_buf[FAST_STRING_SSO_SIZE];
        const char* ro_ptr;
    } data;

    size_t len;
    size_t capacity;
    uint8_t flags;
    uint8_t _padding[7];
} FastString;

FAST_STRING_API FastString fast_string_new(const char* str);
FAST_STRING_API FastString fast_string_new_len(const char* str, size_t len);
FAST_STRING_API FastString fast_string_empty(void);
FAST_STRING_API FastString fast_string_with_capacity(size_t capacity);
FAST_STRING_API FastString fast_string_from_ro(const char* str);
FAST_STRING_API FastString fast_string_clone(const FastString* fs);
FAST_STRING_API void fast_string_free(FastString* fs);
FAST_STRING_API void fast_string_copy(FastString* dest, const FastString* src);
FAST_STRING_API void fast_string_move(FastString* dest, FastString* src);
FAST_STRING_API void fast_string_assign_cstr(FastString* fs, const char* str);
FAST_STRING_API FastString fast_string_concat(const FastString* a, const FastString* b);
FAST_STRING_API FastString fast_string_concat_cstr(const FastString* a, const char* b);
FAST_STRING_API void fast_string_append(FastString* fs, const FastString* b);
FAST_STRING_API void fast_string_append_cstr(FastString* fs, const char* str);
FAST_STRING_API FastString fast_string_concat_multi(const FastString** strings, size_t count);
FAST_STRING_API void fast_string_clear(FastString* fs);
FAST_STRING_API bool fast_string_resize(FastString* fs, size_t new_len);
FAST_STRING_API bool fast_string_reserve(FastString* fs, size_t new_capacity);
FAST_STRING_API bool fast_string_insert(FastString* fs, size_t pos, const char* str);
FAST_STRING_API bool fast_string_erase(FastString* fs, size_t pos, size_t count);
FAST_STRING_API bool fast_string_replace(FastString* fs, const char* find, const char* replace);
FAST_STRING_API ptrdiff_t fast_string_find_char(const FastString* fs, char ch, size_t start);
FAST_STRING_API ptrdiff_t fast_string_find_str(const FastString* fs, const char* needle, size_t start);
FAST_STRING_API int fast_string_cmp(const FastString* a, const FastString* b);
FAST_STRING_API int fast_string_cmp_cstr(const FastString* a, const char* b);
FAST_STRING_API bool fast_string_eq(const FastString* a, const FastString* b);
FAST_STRING_API const char* fast_string_cstr(const FastString* fs);
FAST_STRING_API size_t fast_string_len(const FastString* fs);
FAST_STRING_API bool fast_string_is_empty(const FastString* fs);
FAST_STRING_API char fast_string_at(const FastString* fs, size_t index);
FAST_STRING_API bool fast_string_at_safe(const FastString* fs, size_t index, char* out);
FAST_STRING_API void fast_string_trim(FastString* fs);
FAST_STRING_API void fast_string_to_lower(FastString* fs);
FAST_STRING_API void fast_string_to_upper(FastString* fs);
FAST_STRING_API void fast_string_reverse(FastString* fs);

#endif