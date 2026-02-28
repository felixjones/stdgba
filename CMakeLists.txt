cmake_minimum_required(VERSION 3.18)

project(stdgba LANGUAGES C CXX ASM VERSION 2026.2.0)

add_library(stdgba INTERFACE
        include/gba/angle
        include/gba/bios
        include/gba/compress
        include/gba/filter
        include/gba/fixed_point
        include/gba/format
        include/gba/functional
        include/gba/interrupt
        include/gba/keyinput
        include/gba/logger
        include/gba/memory
        include/gba/music
        include/gba/overlay
        include/gba/peripherals
        include/gba/save
        include/gba/shapes
        include/gba/timer
        include/gba/video
)
target_include_directories(stdgba INTERFACE include)
target_compile_features(stdgba INTERFACE cxx_std_23)
target_sources(stdgba INTERFACE
        source/assert.cpp
        source/assert_entry.s
        source/div.s
        source/ldiv.s
        source/lmul.s
        source/divzero.cpp
        source/divzero_entry.s
        source/flash_ops.cpp
        source/flash.s
        source/interrupt.cpp
        source/memchr.s
        source/memcmp.cpp
        source/memcmp.s
        source/memcpy.cpp
        source/memcpy.s
        source/memmove.cpp
        source/memmove.s
        source/memset.cpp
        source/memset.s
        source/strchr.s
        source/strcmp.s
        source/strlen.s
)

set_source_files_properties(source/flash.s PROPERTIES COMPILE_FLAGS "-fPIC")

if(STDGBA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
