find_package(librom REQUIRED)
find_program(MGBA_HEADLESS mgba-headless PATHS bin/win_x64)

file(GLOB_RECURSE TEST_SOURCES test_*.cpp)

# Tests that require manual testing (excluded from ctest)
set(MANUAL_TESTS
    test_assert
    test_divzero
    test_pretty_printers
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    if(test_name STREQUAL "test_overlay")
        target_sources(${test_name} PRIVATE overlay/overlay_code.cpp)
    endif()
    target_link_libraries(${test_name} PRIVATE librom stdgba)
    target_include_directories(${test_name} PRIVATE .)
    if(test_name STREQUAL "test_memmove_inline")
        target_compile_options(${test_name} PUBLIC -O3 -g3 -Wno-psabi)
    else()
        target_compile_options(${test_name} PUBLIC -g3 -Wno-psabi)
    endif()
    if(NOT ${test_name} IN_LIST MANUAL_TESTS)
        add_test(NAME ${test_name} COMMAND ${MGBA_HEADLESS} -S 0x1A -R r0 -t 10 $<TARGET_FILE:${test_name}>)
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 15)
    endif()
endforeach()
