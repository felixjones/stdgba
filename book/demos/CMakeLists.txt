find_package(librom REQUIRED)

file(GLOB DEMO_SOURCES demo_*.cpp)

foreach(demo_src ${DEMO_SOURCES})
    get_filename_component(demo_name ${demo_src} NAME_WE)
    add_executable(${demo_name} ${demo_src})
    target_link_libraries(${demo_name} PRIVATE librom stdgba)
    if(demo_name STREQUAL "demo_tetris" OR demo_name STREQUAL "demo_wav")
        target_compile_options(${demo_name} PRIVATE -O3 -g3 -Wno-psabi -ftemplate-depth=10000 -fconstexpr-loop-limit=10000000 -fconstexpr-ops-limit=5000000000 -std=gnu++26)
    else()
        target_compile_options(${demo_name} PRIVATE -Os -g3 -Wno-psabi -fconstexpr-ops-limit=335544320)
    endif()
    target_include_directories(${demo_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

install_rom(demo_green_lo)
