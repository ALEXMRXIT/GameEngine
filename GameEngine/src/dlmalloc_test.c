#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

extern void* dlmalloc(size_t);
extern void dlfree(void*);
extern void* dlrealloc(void*, size_t);

static size_t alloc_count = 0;
static size_t free_count = 0;
static size_t realloc_count = 0;
static size_t total_allocated = 0;

static void* test_malloc(size_t size) {
    alloc_count++;
    total_allocated += size;
    return dlmalloc(size);
}

static void* test_realloc(void* ptr, size_t new_size) {
    realloc_count++;
    return dlrealloc(ptr, new_size);
}

static void test_free(void* ptr) {
    if (ptr) {
        free_count++;
        dlfree(ptr);
    }
}

static void reset_stats(void) {
    alloc_count = 0;
    free_count = 0;
    total_allocated = 0;
}

static void print_stats(void) {
    printf("\n========== DLMALLOC STATS ==========\n");
    printf("allocations:   %zu\n", alloc_count);
    printf("frees:         %zu\n", free_count);
    printf("reallocs:      %zu\n", realloc_count);
    printf("total:         %zu bytes (%.2f KB)\n", total_allocated, total_allocated / 1024.0);
    printf("=====================================\n");
}

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAILED: %s\n", msg); \
            return 1; \
        } \
    } while(0)

static int test_dlmalloc_malloc_free(void) {
    printf("\n[TEST] malloc/free\n");

    void* p1 = test_malloc(100);
    TEST_ASSERT(p1 != NULL, "malloc failed");
    TEST_ASSERT(alloc_count == 1, "alloc count wrong");

    void* p2 = test_malloc(200);
    TEST_ASSERT(p2 != NULL, "malloc failed");
    TEST_ASSERT(alloc_count == 2, "alloc count wrong");

    test_free(p1);
    TEST_ASSERT(free_count == 1, "free count wrong");

    test_free(p2);
    TEST_ASSERT(free_count == 2, "free count wrong");

    return 0;
}

static int test_dlmalloc_realloc(void) {
    printf("\n[TEST] realloc\n");

    void* p = test_malloc(100);
    TEST_ASSERT(p != NULL, "malloc failed");

    size_t before_realloc = realloc_count;
    p = test_realloc(p, 500);
    TEST_ASSERT(p != NULL, "realloc failed");
    TEST_ASSERT(realloc_count == before_realloc + 1, "realloc count wrong");

    test_free(p);

    return 0;
}

static int test_dlmalloc_multiple_allocations(void) {
    printf("\n[TEST] multiple allocations\n");

    reset_stats();

    void* ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = test_malloc(i + 1);
        TEST_ASSERT(ptrs[i] != NULL, "malloc failed");
    }

    TEST_ASSERT(alloc_count == 1000, "alloc count wrong");

    for (int i = 0; i < 1000; i++)
        test_free(ptrs[i]);

    TEST_ASSERT(free_count == 1000, "free count wrong");

    return 0;
}

static int test_dlmalloc_large_allocation(void) {
    printf("\n[TEST] large allocation\n");

    reset_stats();

    size_t large_size = 10 * 1024 * 1024;
    void* p = test_malloc(large_size);
    TEST_ASSERT(p != NULL, "large malloc failed");
    TEST_ASSERT(total_allocated >= large_size, "total allocated wrong");

    memset(p, 0xAA, large_size);

    test_free(p);

    return 0;
}

static int test_dlmalloc_realloc_grow_shrink(void) {
    printf("\n[TEST] realloc grow and shrink\n");

    reset_stats();

    void* p = test_malloc(100);
    TEST_ASSERT(p != NULL, "malloc failed");

    size_t before_realloc = realloc_count;
    p = test_realloc(p, 1000);
    TEST_ASSERT(p != NULL, "realloc grow failed");
    TEST_ASSERT(realloc_count == before_realloc + 1, "realloc count wrong");

    before_realloc = realloc_count;
    p = test_realloc(p, 50);
    TEST_ASSERT(p != NULL, "realloc shrink failed");
    TEST_ASSERT(realloc_count == before_realloc + 1, "realloc count wrong");

    test_free(p);

    return 0;
}

static int test_dlmalloc_free_null(void) {
    printf("\n[TEST] free with NULL\n");

    reset_stats();

    test_free(NULL);
    TEST_ASSERT(free_count == 0, "free(NULL) should not count");

    return 0;
}

static int test_dlmalloc_memory_pressure(void) {
    printf("\n[TEST] memory pressure\n");

    reset_stats();

    void** ptrs = (void**)malloc(10000 * sizeof(void*));

    for (int i = 0; i < 10000; i++) {
        ptrs[i] = test_malloc(1024);
        TEST_ASSERT(ptrs[i] != NULL, "malloc failed under pressure");
    }

    TEST_ASSERT(alloc_count == 10000, "alloc count wrong");

    for (int i = 0; i < 10000; i++)
        test_free(ptrs[i]);

    TEST_ASSERT(free_count == 10000, "free count wrong");

    free(ptrs);

    return 0;
}

static int test_dlmalloc_mix_alloc_free(void) {
    printf("\n[TEST] mix alloc/free patterns\n");

    reset_stats();

    void* ptrs[100];

    for (int i = 0; i < 50; i++)
        ptrs[i] = test_malloc(100);

    for (int i = 0; i < 25; i++)
        test_free(ptrs[i]);

    for (int i = 50; i < 100; i++)
        ptrs[i] = test_malloc(200);

    for (int i = 25; i < 100; i++)
        if (ptrs[i]) test_free(ptrs[i]);

    TEST_ASSERT(alloc_count == free_count, "alloc/free mismatch");

    return 0;
}

int run_dlmalloc_test(void) {
    printf("\n========================================\n");
    printf("\tDLMALLOC ALLOCATOR TEST\n");
    printf("========================================\n");

    int failed = 0;

    failed |= test_dlmalloc_malloc_free();
    failed |= test_dlmalloc_realloc();
    failed |= test_dlmalloc_multiple_allocations();
    failed |= test_dlmalloc_large_allocation();
    failed |= test_dlmalloc_realloc_grow_shrink();
    failed |= test_dlmalloc_free_null();
    failed |= test_dlmalloc_memory_pressure();
    failed |= test_dlmalloc_mix_alloc_free();

    print_stats();

    printf("\n========================================\n");

    if (failed == 0)
        printf("ALL DLMALLOC TESTS PASSED\n");
    else
        printf("SOME DLMALLOC TESTS FAILED\n");

    printf("========================================\n");

    return failed;
}