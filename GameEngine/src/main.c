#include "dlmalloc_test.h"
#include "fast_string_test.h"

int main(int argc, char** argv) {

    run_dlmalloc_test();
    run_fast_string_tests();

    return 0;
}