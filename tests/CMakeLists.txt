find_package(librom REQUIRED)
find_program(MGBA_HEADLESS mgba-headless PATHS bin/win_x64)

file(GLOB_RECURSE TEST_SOURCES test_*.cpp)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} mgba.c)
    target_link_libraries(${test_name} PRIVATE librom stdgba)
    target_include_directories(${test_name} PRIVATE .)
    target_compile_options(${test_name} PUBLIC -g3)
    add_test(NAME ${test_name} COMMAND ${MGBA_HEADLESS} -S 0xFF -R r0 $<TARGET_FILE:${test_name}>)
endforeach()
