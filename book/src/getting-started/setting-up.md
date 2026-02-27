# Setting Up

## Prerequisites

- **ARM GCC cross-compiler** -- the `arm-none-eabi` toolchain
- **CMake 3.25+** -- for the build system
- **A GBA emulator** -- [mGBA](https://mgba.io/) is recommended for development

## Project structure

A typical stdgba project looks like this:

```text
my_game/
    CMakeLists.txt
    source/
        main.cpp
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_game C CXX ASM)

add_executable(my_game source/main.cpp)
target_compile_features(my_game PRIVATE cxx_std_23)
target_link_libraries(my_game PRIVATE stdgba librom)
```

stdgba requires C++23. The `librom` library provides the GBA ROM header, startup code, and linker script.

## Compiler flags

The toolchain sets these automatically, but it helps to know what they do:

| Flag | Purpose |
|------|---------|
| `-mcpu=arm7tdmi` | Target the GBA's ARM7TDMI CPU |
| `-mthumb` | Generate Thumb code by default (smaller, fits in cache) |
| `-ffunction-sections` | Each function gets its own section (enables `--gc-sections`) |
| `-fdata-sections` | Each variable gets its own section |
| `-O2` | Recommended optimization level |

## Building

```sh
cmake --preset release -S . -B build
cmake --build build
```

The output is a `.gba` ROM file in the build directory. Open it in mGBA to run.

## Testing with mGBA

stdgba's own test suite uses `mgba-headless` for automated testing:

```sh
cmake --build build --target test
```

Tests use a special `mgba_test.hpp` harness that communicates pass/fail through mGBA's debug registers.
