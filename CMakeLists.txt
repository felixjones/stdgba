cmake_minimum_required(VERSION 3.18)

project(stdgba LANGUAGES CXX VERSION 2025.6.0)

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
        include/gba/memory
        include/gba/peripherals
        include/gba/timer
        include/gba/video
)
target_include_directories(stdgba INTERFACE include)
target_compile_features(stdgba INTERFACE cxx_std_23)
target_sources(stdgba INTERFACE
        source/interrupt.cpp)

enable_testing()
add_subdirectory(tests)
