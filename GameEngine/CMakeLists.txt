cmake_minimum_required(VERSION 3.10)
project(GameEngine C)

add_executable(GameEngine
    src/main.c
    src/fast_string.c
    src/fast_string_test.c
    src/dlmalloc.c
    src/dlmalloc_test.c
    src/fast_string.h
    src/fast_string_priv.h
    src/fast_string_test.h
    src/dlmalloc_test.h
)

target_include_directories(GameEngine PRIVATE src)