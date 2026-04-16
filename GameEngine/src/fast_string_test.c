#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "fast_string.h"

extern void* dlmalloc(size_t);
extern void  dlfree(void*);
extern void* dlrealloc(void*, size_t);

static size_t g_alloc_count = 0;
static size_t g_free_count = 0;
static size_t g_realloc_count = 0;
static size_t g_total_allocated = 0;

static void* test_malloc(size_t size) {
    g_alloc_count++;
    g_total_allocated += size;
    return dlmalloc(size);
}

static void* test_realloc(void* ptr, size_t new_size) {
    g_realloc_count++;
    return dlrealloc(ptr, new_size);
}

static void test_free(void* ptr) {
    if (ptr) {
        g_free_count++;
        dlfree(ptr);
    }
}

static const FastStringAllocator test_allocator = {
    .alloc = test_malloc,
    .realloc = test_realloc,
    .free = test_free,
    .alloc_aligned = NULL,
    .free_aligned = NULL
};

typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
} Timer;

static void timer_start(Timer* t) {
    QueryPerformanceFrequency(&t->frequency);
    QueryPerformanceCounter(&t->start);
}

static double timer_elapsed_ms(Timer* t) {
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - t->start.QuadPart) * 1000.0 / (double)t->frequency.QuadPart;
}

static char* random_string(size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* str = (char*)malloc(len + 1);
    if (!str) return NULL;
    for (size_t i = 0; i < len; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[len] = '\0';
    return str;
}

static size_t get_heap_usage(void) {
    HANDLE heap = GetProcessHeap();
    PROCESS_HEAP_ENTRY entry = { 0 };
    size_t total = 0;
    while (HeapWalk(heap, &entry)) {
        if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
            total += entry.cbData;
        }
    }
    return total;
}

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAILED: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while(0)

#define TEST_START(name) \
    printf("\n========== %s ==========\n", name); \
    Timer timer; \
    timer_start(&timer)

#define TEST_END(name) \
    printf("%s completed in %.2f ms\n", name, timer_elapsed_ms(&timer))

static int test_sso_vs_heap_performance(void) {
    TEST_START("SSO vs HEAP Performance");
    const size_t iterations = 1000000;
    timer_start(&timer);
    for (size_t i = 0; i < iterations; i++) {
        FastString fs = fast_string_new("Small string");
        fast_string_free(&fs);
    }
    double sso_time = timer_elapsed_ms(&timer);
    timer_start(&timer);
    for (size_t i = 0; i < iterations; i++) {
        FastString fs = fast_string_new("This is a very long string that definitely exceeds the SSO limit of 24 bytes and will be allocated on the heap");
        fast_string_free(&fs);
    }
    double heap_time = timer_elapsed_ms(&timer);
    printf("SSO (small strings):  %.2f ms for %zu iterations\n", sso_time, iterations);
    printf("HEAP (large strings): %.2f ms for %zu iterations\n", heap_time, iterations);
    printf("SSO is %.2fx faster\n", heap_time / sso_time);
    TEST_ASSERT(sso_time < heap_time, "SSO should be faster than heap allocation");
    TEST_END("SSO vs HEAP Performance");
    return 0;
}

static int test_concat_stress(void) {
    TEST_START("Concat Stress Test");
    const size_t str_count = 1000;
    FastString* strings = (FastString*)malloc(sizeof(FastString) * str_count);
    for (size_t i = 0; i < str_count; i++) {
        size_t len = (i % 50) + 1;
        char* random_str = random_string(len);
        strings[i] = fast_string_new(random_str);
        free(random_str);
    }
    FastString result = fast_string_empty();
    timer_start(&timer);
    for (size_t i = 0; i < str_count; i++)
        fast_string_append(&result, &strings[i]);
    double concat_time = timer_elapsed_ms(&timer);
    printf("Concatenated %zu strings into %zu bytes in %.2f ms\n", str_count, result.len, concat_time);
    size_t expected_len = 0;
    for (size_t i = 0; i < str_count; i++)
        expected_len += strings[i].len;
    TEST_ASSERT(result.len == expected_len, "Concatenated length mismatch");
    fast_string_free(&result);
    for (size_t i = 0; i < str_count; i++)
        fast_string_free(&strings[i]);
    free(strings);
    TEST_END("Concat Stress Test");
    return 0;
}

static int test_insert_erase_stress(void) {
    TEST_START("Insert/Erase Stress Test");
    FastString fs = fast_string_new("Hello World");
    const size_t operations = 10000;
    timer_start(&timer);
    for (size_t i = 0; i < operations; i++) {
        if (fs.len > 0) {
            size_t pos = rand() % fs.len;
            char ch = 'A' + (rand() % 26);
            char buf[2] = { ch, '\0' };
            fast_string_insert(&fs, pos, buf);
            if (fs.len > 0) {
                pos = rand() % fs.len;
                fast_string_erase(&fs, pos, 1);
            }
        }
    }
    double time_ms = timer_elapsed_ms(&timer);
    printf("Performed %zu insert/erase operations in %.2f ms\n", operations, time_ms);
    fast_string_free(&fs);
    TEST_END("Insert/Erase Stress Test");
    return 0;
}

static int test_search_performance(void) {
    TEST_START("Search Performance");
    const size_t haystack_size = 100000;
    char* haystack_raw = random_string(haystack_size);
    FastString haystack = fast_string_new(haystack_raw);
    char target_char = haystack_raw[haystack_size / 2];
    timer_start(&timer);
    ptrdiff_t pos = fast_string_find_char(&haystack, target_char, 0);
    double find_char_time = timer_elapsed_ms(&timer);
    printf("Find existing char '%c' at pos %td: %.3f ms\n", target_char, pos, find_char_time);
    TEST_ASSERT(pos >= 0, "Should find existing character");
    timer_start(&timer);
    pos = fast_string_find_char(&haystack, '\x00', 0);
    double find_missing_time = timer_elapsed_ms(&timer);
    printf("Find missing char: %.3f ms\n", find_missing_time);
    TEST_ASSERT(pos == -1, "Should not find missing character");
    timer_start(&timer);
    pos = fast_string_find_str(&haystack, "XYZ123", 0);
    double find_str_time = timer_elapsed_ms(&timer);
    printf("Find non-existent substring: %.3f ms\n", find_str_time);
    fast_string_free(&haystack);
    free(haystack_raw);
    TEST_END("Search Performance");
    return 0;
}

static int test_move_copy_semantics(void) {
    TEST_START("Move/Copy Semantics");
    char* large_str = random_string(10000);
    FastString original = fast_string_new(large_str);
    size_t original_len = original.len;
    timer_start(&timer);
    FastString copy = fast_string_clone(&original);
    double copy_time = timer_elapsed_ms(&timer);
    printf("Deep copy of %zu bytes: %.3f ms\n", original_len, copy_time);
    TEST_ASSERT(copy.len == original.len, "Copy length mismatch");
    TEST_ASSERT(strcmp(fast_string_cstr(&copy), fast_string_cstr(&original)) == 0, "Copy content mismatch");
    FastString dest = fast_string_empty();
    timer_start(&timer);
    fast_string_move(&dest, &original);
    double move_time = timer_elapsed_ms(&timer);
    printf("Move operation: %.3f ms\n", move_time);
    TEST_ASSERT(original.len == 0, "Original should be empty after move");
    TEST_ASSERT(dest.len == original_len, "Destination length mismatch");
    TEST_ASSERT(fast_string_cmp(&dest, &copy) == 0, "Moved content mismatch");
    TEST_ASSERT(move_time < copy_time, "Move should be faster than copy");
    printf("Move is %.2fx faster than copy\n", copy_time / move_time);
    fast_string_free(&copy);
    fast_string_free(&dest);
    fast_string_free(&original);
    free(large_str);
    TEST_END("Move/Copy Semantics");
    return 0;
}

static int test_memory_leaks(void) {
    TEST_START("Memory Leak Detection");
    size_t heap_before = get_heap_usage();
    printf("Heap before test: %zu bytes\n", heap_before);
    const size_t iterations = 100000;
    for (size_t i = 0; i < iterations; i++) {
        FastString fs1 = fast_string_new("Test string that is long enough to be allocated on heap");
        FastString fs2 = fast_string_clone(&fs1);
        FastString fs3 = fast_string_concat(&fs1, &fs2);
        FastString fs4 = fast_string_empty();
        fast_string_move(&fs4, &fs3);
        fast_string_free(&fs1);
        fast_string_free(&fs2);
        fast_string_free(&fs4);
    }
    size_t heap_after = get_heap_usage();
    printf("Heap after test: %zu bytes\n", heap_after);
    ptrdiff_t leak = (ptrdiff_t)heap_after - (ptrdiff_t)heap_before;
    printf("Memory difference: %td bytes\n", leak);
    TEST_ASSERT(abs(leak) < 1024, "Significant memory leak detected");
    TEST_END("Memory Leak Detection");
    return 0;
}

static int test_edge_cases(void) {
    TEST_START("Edge Cases");
    FastString empty = fast_string_empty();
    TEST_ASSERT(empty.len == 0, "Empty string length should be 0");
    TEST_ASSERT(fast_string_is_empty(&empty), "Empty string should report empty");
    TEST_ASSERT(strcmp(fast_string_cstr(&empty), "") == 0, "Empty string C-string should be \"\"");
    const size_t huge_size = 1024 * 1024;
    char* huge_raw = random_string(huge_size);
    timer_start(&timer);
    FastString huge = fast_string_new(huge_raw);
    double create_time = timer_elapsed_ms(&timer);
    printf("Created %zu MB string in %.2f ms\n", huge_size / (1024 * 1024), create_time);
    TEST_ASSERT(huge.len == huge_size, "Huge string length mismatch");
    timer_start(&timer);
    fast_string_append_cstr(&huge, "small addition");
    double append_time = timer_elapsed_ms(&timer);
    printf("Appended to huge string in %.2f ms\n", append_time);
    TEST_ASSERT(huge.len == huge_size + 14, "Append length mismatch");
    fast_string_free(&huge);
    free(huge_raw);
    fast_string_free(&empty);
    TEST_END("Edge Cases");
    return 0;
}

static int test_readonly_strings(void) {
    TEST_START("Read-Only Strings");
    static const char* ro_data = "This is read-only string literal";
    FastString ro = fast_string_from_ro(ro_data);
    TEST_ASSERT(ro.flags == FAST_STR_FLAG_READONLY, "Should be READONLY flag");
    TEST_ASSERT(ro.len == strlen(ro_data), "RO string length mismatch");
    TEST_ASSERT(ro.data.ro_ptr == ro_data, "RO string should point to original data");
    FastString mutable_copy = fast_string_clone(&ro);
    fast_string_append_cstr(&mutable_copy, " - modified");
    TEST_ASSERT(mutable_copy.len > ro.len, "Modified string should be longer");
    TEST_ASSERT(strcmp(fast_string_cstr(&ro), ro_data) == 0, "Original RO should be unchanged");
    TEST_ASSERT(strstr(fast_string_cstr(&mutable_copy), "modified") != NULL, "Modified copy should contain new text");
    fast_string_free(&mutable_copy);
    fast_string_free(&ro);
    TEST_END("Read-Only Strings");
    return 0;
}

static int test_reserve_performance(void) {
    TEST_START("Reserve Performance");
    const size_t iterations = 10000;
    const size_t final_size = 100000;
    timer_start(&timer);
    FastString without_reserve = fast_string_empty();
    for (size_t i = 0; i < iterations; i++)
        fast_string_append_cstr(&without_reserve, "a");
    double without_reserve_time = timer_elapsed_ms(&timer);
    printf("Without reserve: %.2f ms\n", without_reserve_time);
    timer_start(&timer);
    FastString with_reserve = fast_string_with_capacity(final_size);
    for (size_t i = 0; i < iterations; i++)
        fast_string_append_cstr(&with_reserve, "a");
    double with_reserve_time = timer_elapsed_ms(&timer);
    printf("With reserve: %.2f ms\n", with_reserve_time);
    printf("Reserve is %.2fx faster\n", without_reserve_time / with_reserve_time);
    fast_string_free(&without_reserve);
    fast_string_free(&with_reserve);
    TEST_END("Reserve Performance");
    return 0;
}

static int test_multi_concat_performance(void) {
    TEST_START("Multi-Concat Performance");
    const size_t string_count = 5000;
    const size_t avg_len = 50;
    FastString* strings = (FastString*)malloc(sizeof(FastString) * string_count);
    const FastString** string_ptrs = (const FastString**)malloc(sizeof(FastString*) * string_count);
    for (size_t i = 0; i < string_count; i++) {
        char* random_str = random_string(avg_len);
        strings[i] = fast_string_new(random_str);
        string_ptrs[i] = &strings[i];
        free(random_str);
    }
    timer_start(&timer);
    FastString sequential = fast_string_empty();
    for (size_t i = 0; i < string_count; i++)
        fast_string_append(&sequential, &strings[i]);
    double sequential_time = timer_elapsed_ms(&timer);
    timer_start(&timer);
    FastString multi = fast_string_concat_multi(string_ptrs, string_count);
    double multi_time = timer_elapsed_ms(&timer);
    printf("Sequential append: %.2f ms\n", sequential_time);
    printf("Multi-concat: %.2f ms\n", multi_time);
    printf("Multi-concat is %.2fx faster\n", sequential_time / multi_time);
    TEST_ASSERT(sequential.len == multi.len, "Length mismatch between methods");
    TEST_ASSERT(fast_string_cmp(&sequential, &multi) == 0, "Content mismatch between methods");
    fast_string_free(&sequential);
    fast_string_free(&multi);
    for (size_t i = 0; i < string_count; i++)
        fast_string_free(&strings[i]);
    free(strings);
    free(string_ptrs);
    TEST_END("Multi-Concat Performance");
    return 0;
}

static int test_custom_allocator_integration(void) {
    TEST_START("Custom Allocator Integration");
    fast_string_set_allocator(&test_allocator);
    size_t before_alloc = g_alloc_count;
    size_t before_free = g_free_count;
    FastString s = fast_string_new("This string uses custom allocator for heap allocation");
    TEST_ASSERT(g_alloc_count > before_alloc, "Custom allocator was not called");
    fast_string_free(&s);
    TEST_ASSERT(g_free_count > before_free, "Custom free was not called");
    fast_string_set_allocator(NULL);
    TEST_END("Custom Allocator Integration");
    return 0;
}

int run_fast_string_tests(void) {
    printf("=============================================\n");
    printf("\tFAST STRING STRESS TEST SUITE\n");
    printf("=============================================\n");
    srand((unsigned int)time(NULL));
    fast_string_set_allocator(&test_allocator);
    int failed = 0;
    failed |= test_sso_vs_heap_performance();
    failed |= test_concat_stress();
    failed |= test_insert_erase_stress();
    failed |= test_search_performance();
    failed |= test_move_copy_semantics();
    failed |= test_memory_leaks();
    failed |= test_edge_cases();
    failed |= test_readonly_strings();
    failed |= test_reserve_performance();
    failed |= test_multi_concat_performance();
    failed |= test_custom_allocator_integration();
    printf("\n========================================\n");
    if (failed == 0)
        printf("ALL TESTS PASSED! 🎉\n");
    else
        printf("SOME TESTS FAILED! ❌\n");
    printf("========================================\n");
    fast_string_set_allocator(NULL);
    return failed;
}