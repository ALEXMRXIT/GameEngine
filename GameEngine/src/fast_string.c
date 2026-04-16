#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fast_string.h"
#include "fast_string_priv.h"

static void* system_alloc(size_t size) {
    return malloc(size);
}

static void* system_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

static void system_free(void* ptr) {
    free(ptr);
}

static void* system_alloc_aligned(size_t size, size_t alignment) {
    return _aligned_malloc(size, alignment);
}

static void system_free_aligned(void* ptr) {
    _aligned_free(ptr);
}

static const FastStringAllocator system_allocator = {
    .alloc = system_alloc,
    .realloc = system_realloc,
    .free = system_free,
    .alloc_aligned = system_alloc_aligned,
    .free_aligned = system_free_aligned
};

const FastStringAllocator* g_fast_string_allocator = &system_allocator;

const FastStringAllocator* fast_string_default_allocator(void) {
    return &system_allocator;
}

const FastStringAllocator* fast_string_system_allocator(void) {
    return &system_allocator;
}

void fast_string_set_allocator(const FastStringAllocator* allocator) {
    if (allocator && allocator->alloc && allocator->realloc && allocator->free)
        g_fast_string_allocator = allocator;
}

const FastStringAllocator* fast_string_get_allocator(void) {
    return g_fast_string_allocator;
}

static void fast_str_init_sso(FastString* fs, const char* data, size_t len) {
    fs->flags = FAST_STR_FLAG_SSO;
    fs->len = len;
    fs->capacity = 0;

    if (len > 0)
        memcpy(fs->data.sso_buf, data, len);

    fs->data.sso_buf[len] = '\0';
}

static bool fast_str_init_heap(FastString* fs, const char* data, size_t len, size_t extra_cap) {
    size_t needed_cap = len + extra_cap;
    if (needed_cap < FAST_STR_MIN_HEAP_ALLOC)
        needed_cap = FAST_STR_MIN_HEAP_ALLOC;

    fs->data.heap_ptr = (char*)FAST_ALLOC(needed_cap + 1);
    if (!fs->data.heap_ptr)
        return false;

    fs->flags = FAST_STR_FLAG_HEAP;
    fs->len = len;
    fs->capacity = needed_cap;

    if (len > 0 && data)
        memcpy(fs->data.heap_ptr, data, len);

    fs->data.heap_ptr[len] = '\0';

    return true;
}

static bool fast_str_make_mutable(FastString* fs) {
    if (!FAST_STR_IS_RO(fs))
        return true;

    const char* old_data = fs->data.ro_ptr;
    size_t old_len = fs->len;

    if (!fast_str_init_heap(fs, old_data, old_len, 0))
        return false;

    return true;
}

FastString fast_string_new(const char* str) {
    return fast_string_new_len(str, str ? strlen(str) : 0);
}

FastString fast_string_new_len(const char* str, size_t len) {
    FastString fs = { 0 };

    if (!str || len == 0) {
        fast_str_init_sso(&fs, "", 0);
        return fs;
    }

    if (len < FAST_STRING_SSO_SIZE)
        fast_str_init_sso(&fs, str, len);
    else
        fast_str_init_heap(&fs, str, len, 0);

    return fs;
}

FastString fast_string_empty(void) {
    return fast_string_new_len("", 0);
}

FastString fast_string_with_capacity(size_t capacity) {
    FastString fs = { 0 };

    if (capacity < FAST_STRING_SSO_SIZE)
        fast_str_init_sso(&fs, "", 0);
    else
        fast_str_init_heap(&fs, NULL, 0, capacity);

    return fs;
}

FastString fast_string_from_ro(const char* str) {
    FastString fs = { 0 };

    fs.flags = FAST_STR_FLAG_READONLY;
    fs.len = str ? strlen(str) : 0;
    fs.capacity = 0;
    fs.data.ro_ptr = str ? str : "";

    return fs;
}

FastString fast_string_clone(const FastString* fs) {
    if (!fs) return fast_string_empty();

    const char* data = fast_str_data_const(fs);
    return fast_string_new_len(data, fs->len);
}

void fast_string_free(FastString* fs) {
    if (!fs) return;

    if (FAST_STR_IS_HEAP(fs) && fs->data.heap_ptr)
        FAST_FREE(fs->data.heap_ptr);

    memset(fs, 0, sizeof(FastString));
}

void fast_string_copy(FastString* dest, const FastString* src) {
    if (!dest || !src) return;
    if (dest == src) return;

    fast_string_free(dest);

    const char* src_data = fast_str_data_const(src);
    *dest = fast_string_new_len(src_data, src->len);
}

void fast_string_move(FastString* dest, FastString* src) {
    if (!dest || !src) return;
    if (dest == src) return;

    fast_string_free(dest);
    memcpy(dest, src, sizeof(FastString));

    memset(src, 0, sizeof(FastString));
    fast_str_init_sso(src, "", 0);
}

void fast_string_assign_cstr(FastString* fs, const char* str) {
    if (!fs) return;

    FastString new_fs = fast_string_new(str ? str : "");
    fast_string_move(fs, &new_fs);
}

FastString fast_string_concat(const FastString* a, const FastString* b) {
    FastString result = { 0 };

    if (!a && !b) return fast_string_empty();
    if (!a) return fast_string_clone(b);
    if (!b) return fast_string_clone(a);

    size_t new_len = a->len + b->len;
    const char* a_data = fast_str_data_const(a);
    const char* b_data = fast_str_data_const(b);

    if (new_len < FAST_STRING_SSO_SIZE) {
        fast_str_init_sso(&result, a_data, a->len);
        memcpy(result.data.sso_buf + a->len, b_data, b->len);
        result.data.sso_buf[new_len] = '\0';
        result.len = new_len;
    }
    else {
        if (!fast_str_init_heap(&result, a_data, a->len, b->len))
            return fast_string_empty();

        memcpy(result.data.heap_ptr + a->len, b_data, b->len);
        result.data.heap_ptr[new_len] = '\0';
        result.len = new_len;
    }

    return result;
}

FastString fast_string_concat_cstr(const FastString* a, const char* b) {
    FastString temp = fast_string_new(b ? b : "");
    FastString result = fast_string_concat(a, &temp);
    fast_string_free(&temp);
    return result;
}

void fast_string_append(FastString* fs, const FastString* b) {
    if (!fs || !b) return;
    if (b->len == 0) return;

    if (FAST_STR_IS_RO(fs))
        if (!fast_str_make_mutable(fs)) return;

    const char* b_data = fast_str_data_const(b);
    size_t new_len = fs->len + b->len;

    if (FAST_STR_IS_SSO(fs) && new_len < FAST_STRING_SSO_SIZE) {
        memcpy(fs->data.sso_buf + fs->len, b_data, b->len);
        fs->data.sso_buf[new_len] = '\0';
        fs->len = new_len;
        return;
    }

    if (FAST_STR_IS_SSO(fs) || new_len > fs->capacity) {
        size_t new_cap = fast_str_grow_capacity(fs->capacity, new_len);
        char* new_data = (char*)FAST_ALLOC(new_cap + 1);
        if (!new_data) return;

        const char* old_data = fast_str_data_const(fs);
        memcpy(new_data, old_data, fs->len);
        memcpy(new_data + fs->len, b_data, b->len);
        new_data[new_len] = '\0';

        if (FAST_STR_IS_HEAP(fs) && fs->data.heap_ptr)
            FAST_FREE(fs->data.heap_ptr);

        fs->data.heap_ptr = new_data;
        fs->flags = FAST_STR_FLAG_HEAP;
        fs->capacity = new_cap;
        fs->len = new_len;
        return;
    }

    if (FAST_STR_IS_HEAP(fs) && new_len <= fs->capacity) {
        memcpy(fs->data.heap_ptr + fs->len, b_data, b->len);
        fs->data.heap_ptr[new_len] = '\0';
        fs->len = new_len;
        return;
    }

    if (FAST_STR_IS_SSO(fs)) {
        size_t new_cap = fast_str_grow_capacity(FAST_STRING_SSO_SIZE, new_len);
        char* new_data = (char*)FAST_ALLOC(new_cap + 1);
        if (!new_data) return;

        memcpy(new_data, fs->data.sso_buf, fs->len);
        memcpy(new_data + fs->len, b_data, b->len);
        new_data[new_len] = '\0';

        fs->data.heap_ptr = new_data;
        fs->flags = FAST_STR_FLAG_HEAP;
        fs->capacity = new_cap;
        fs->len = new_len;
        return;
    }
}

void fast_string_append_cstr(FastString* fs, const char* str) {
    if (!fs || !str) return;

    size_t str_len = strlen(str);
    if (str_len == 0) return;

    size_t new_len = fs->len + str_len;

    if (FAST_STR_IS_RO(fs))
        if (!fast_str_make_mutable(fs)) return;

    if (FAST_STR_IS_SSO(fs) && new_len < FAST_STRING_SSO_SIZE) {
        memcpy(fs->data.sso_buf + fs->len, str, str_len);
        fs->data.sso_buf[new_len] = '\0';
        fs->len = new_len;
        return;
    }

    if (FAST_STR_IS_SSO(fs) || new_len > fs->capacity) {
        size_t new_cap = fast_str_grow_capacity(fs->capacity, new_len);
        char* new_data = (char*)FAST_ALLOC(new_cap + 1);
        if (!new_data) return;

        const char* old_data = fast_str_data_const(fs);
        memcpy(new_data, old_data, fs->len);
        memcpy(new_data + fs->len, str, str_len);
        new_data[new_len] = '\0';

        if (FAST_STR_IS_HEAP(fs) && fs->data.heap_ptr)
            FAST_FREE(fs->data.heap_ptr);

        fs->data.heap_ptr = new_data;
        fs->flags = FAST_STR_FLAG_HEAP;
        fs->capacity = new_cap;
        fs->len = new_len;
        return;
    }

    if (FAST_STR_IS_HEAP(fs)) {
        memcpy(fs->data.heap_ptr + fs->len, str, str_len);
        fs->data.heap_ptr[new_len] = '\0';
        fs->len = new_len;
    }
}

FastString fast_string_concat_multi(const FastString** strings, size_t count) {
    if (!strings || count == 0) return fast_string_empty();

    size_t total_len = 0;
    for (size_t i = 0; i < count; i++)
        if (strings[i]) total_len += strings[i]->len;

    if (total_len == 0) return fast_string_empty();

    FastString result = { 0 };
    if (total_len < FAST_STRING_SSO_SIZE) {
        fast_str_init_sso(&result, "", 0);
        char* pos = result.data.sso_buf;
        for (size_t i = 0; i < count; i++) {
            if (strings[i] && strings[i]->len > 0) {
                const char* data = fast_str_data_const(strings[i]);
                memcpy(pos, data, strings[i]->len);
                pos += strings[i]->len;
            }
        }
        result.data.sso_buf[total_len] = '\0';
        result.len = total_len;
    }
    else {
        if (!fast_str_init_heap(&result, NULL, 0, total_len))
            return fast_string_empty();

        char* pos = result.data.heap_ptr;
        for (size_t i = 0; i < count; i++) {
            if (strings[i] && strings[i]->len > 0) {
                const char* data = fast_str_data_const(strings[i]);
                memcpy(pos, data, strings[i]->len);
                pos += strings[i]->len;
            }
        }
        result.data.heap_ptr[total_len] = '\0';
        result.len = total_len;
    }

    return result;
}

void fast_string_clear(FastString* fs) {
    if (!fs) return;

    if (!fast_str_make_mutable(fs)) return;

    fs->len = 0;
    char* data = fast_str_data(fs);
    if (data) data[0] = '\0';
}

bool fast_string_resize(FastString* fs, size_t new_len) {
    if (!fs) return false;

    if (new_len == fs->len) return true;

    if (!fast_str_make_mutable(fs)) return false;

    if (new_len < fs->len) {
        char* data = fast_str_data(fs);
        data[new_len] = '\0';
        fs->len = new_len;
        return true;
    }

    if (!fast_string_reserve(fs, new_len)) return false;

    char* data = fast_str_data(fs);
    memset(data + fs->len, 0, new_len - fs->len);
    data[new_len] = '\0';
    fs->len = new_len;

    return true;
}

bool fast_string_reserve(FastString* fs, size_t new_capacity) {
    if (!fs) return false;

    if (FAST_STR_IS_SSO(fs) && new_capacity < FAST_STRING_SSO_SIZE) {
        return true;
    }

    if (!fast_str_make_mutable(fs)) return false;

    size_t target_cap = new_capacity;
    if (target_cap < fs->len) target_cap = fs->len;
    if (target_cap < FAST_STR_MIN_HEAP_ALLOC) target_cap = FAST_STR_MIN_HEAP_ALLOC;

    if (target_cap <= fs->capacity) return true;

    char* new_data = (char*)FAST_ALLOC(target_cap + 1);
    if (!new_data) return false;

    const char* old_data = fast_str_data_const(fs);
    memcpy(new_data, old_data, fs->len);
    new_data[fs->len] = '\0';

    if (FAST_STR_IS_HEAP(fs))
        FAST_FREE(fs->data.heap_ptr);

    fs->data.heap_ptr = new_data;
    fs->flags = FAST_STR_FLAG_HEAP;
    fs->capacity = target_cap;

    return true;
}

bool fast_string_insert(FastString* fs, size_t pos, const char* str) {
    if (!fs || !str) return false;
    if (pos > fs->len) return false;

    size_t insert_len = strlen(str);
    if (insert_len == 0) return true;

    if (!fast_str_make_mutable(fs)) return false;

    size_t new_len = fs->len + insert_len;

    if (!fast_string_reserve(fs, new_len)) return false;

    char* data = fast_str_data(fs);

    memmove(data + pos + insert_len, data + pos, fs->len - pos + 1);
    memcpy(data + pos, str, insert_len);
    fs->len = new_len;

    return true;
}

bool fast_string_erase(FastString* fs, size_t pos, size_t count) {
    if (!fs) return false;
    if (pos >= fs->len) return false;

    if (!fast_str_make_mutable(fs)) return false;

    if (count > fs->len - pos)
        count = fs->len - pos;

    if (count == 0) return true;

    char* data = fast_str_data(fs);
    memmove(data + pos, data + pos + count, fs->len - pos - count + 1);
    fs->len -= count;

    return true;
}

bool fast_string_replace(FastString* fs, const char* find, const char* replace) {
    if (!fs || !find || !replace) return false;

    ptrdiff_t pos = fast_string_find_str(fs, find, 0);
    if (pos < 0) return false;

    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);

    if (!fast_str_make_mutable(fs)) return false;

    if (find_len == replace_len) {
        char* data = fast_str_data(fs);
        memcpy(data + pos, replace, replace_len);
    }
    else if (find_len > replace_len) {
        char* data = fast_str_data(fs);
        memcpy(data + pos, replace, replace_len);
        memmove(data + pos + replace_len, data + pos + find_len, fs->len - pos - find_len + 1);
        fs->len -= (find_len - replace_len);
    }
    else {
        size_t new_len = fs->len + (replace_len - find_len);
        if (!fast_string_reserve(fs, new_len)) return false;

        char* data = fast_str_data(fs);
        memmove(data + pos + replace_len, data + pos + find_len, fs->len - pos - find_len + 1);
        memcpy(data + pos, replace, replace_len);
        fs->len = new_len;
    }

    return true;
}

ptrdiff_t fast_string_find_char(const FastString* fs, char ch, size_t start) {
    if (!fs || start >= fs->len) return -1;

    const char* data = fast_str_data_const(fs);
    const char* found = (const char*)memchr(data + start, ch, fs->len - start);

    return found ? (found - data) : -1;
}

ptrdiff_t fast_string_find_str(const FastString* fs, const char* needle, size_t start) {
    if (!fs || !needle || start >= fs->len) return -1;

    size_t needle_len = strlen(needle);
    if (needle_len == 0) return start;
    if (needle_len > fs->len - start) return -1;

    const char* haystack = fast_str_data_const(fs) + start;
    size_t haystack_len = fs->len - start;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return start + i;
    }

    return -1;
}

int fast_string_cmp(const FastString* a, const FastString* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    size_t min_len = (a->len < b->len) ? a->len : b->len;
    const char* a_data = fast_str_data_const(a);
    const char* b_data = fast_str_data_const(b);

    int result = memcmp(a_data, b_data, min_len);

    if (result != 0) return result;
    if (a->len < b->len) return -1;
    if (a->len > b->len) return 1;
    return 0;
}

int fast_string_cmp_cstr(const FastString* a, const char* b) {
    FastString temp = fast_string_from_ro(b);
    int result = fast_string_cmp(a, &temp);

    return result;
}

bool fast_string_eq(const FastString* a, const FastString* b) {
    return fast_string_cmp(a, b) == 0;
}

const char* fast_string_cstr(const FastString* fs) {
    if (!fs) return "";
    return fast_str_data_const(fs);
}

size_t fast_string_len(const FastString* fs) {
    return fs ? fs->len : 0;
}

bool fast_string_is_empty(const FastString* fs) {
    return !fs || fs->len == 0;
}

char fast_string_at(const FastString* fs, size_t index) {
    if (!fs || index >= fs->len) return '\0';
    const char* data = fast_str_data_const(fs);
    return data[index];
}

bool fast_string_at_safe(const FastString* fs, size_t index, char* out) {
    if (!fs || !out || index >= fs->len) return false;
    const char* data = fast_str_data_const(fs);
    *out = data[index];
    return true;
}

void fast_string_trim(FastString* fs) {
    if (!fs || fs->len == 0) return;
    if (!fast_str_make_mutable(fs)) return;

    char* data = fast_str_data(fs);
    size_t start = 0;
    size_t end = fs->len - 1;

    while (start < fs->len && isspace((unsigned char)data[start])) start++;
    while (end > start && isspace((unsigned char)data[end])) end--;

    if (start > 0)
        memmove(data, data + start, end - start + 1);

    fs->len = end - start + 1;
    data[fs->len] = '\0';
}

void fast_string_to_lower(FastString* fs) {
    if (!fs || fs->len == 0) return;
    if (!fast_str_make_mutable(fs)) return;

    char* data = fast_str_data(fs);
    for (size_t i = 0; i < fs->len; i++)
        data[i] = (char)tolower((unsigned char)data[i]);
}

void fast_string_to_upper(FastString* fs) {
    if (!fs || fs->len == 0) return;
    if (!fast_str_make_mutable(fs)) return;

    char* data = fast_str_data(fs);
    for (size_t i = 0; i < fs->len; i++) {
        data[i] = (char)toupper((unsigned char)data[i]);
    }
}

void fast_string_reverse(FastString* fs) {
    if (!fs || fs->len < 2) return;
    if (!fast_str_make_mutable(fs)) return;

    char* data = fast_str_data(fs);
    size_t left = 0;
    size_t right = fs->len - 1;

    while (left < right) {
        char tmp = data[left];
        data[left] = data[right];
        data[right] = tmp;
        left++;
        right--;
    }
}