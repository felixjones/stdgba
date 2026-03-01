find_package(librom REQUIRED)

file(GLOB DEMO_SOURCES demo_*.cpp)

foreach(demo_src ${DEMO_SOURCES})
    get_filename_component(demo_name ${demo_src} NAME_WE)
    add_executable(${demo_name} ${demo_src})
    target_link_libraries(${demo_name} PRIVATE librom stdgba)
    target_compile_options(${demo_name} PRIVATE -Os -g3 -Wno-psabi -fconstexpr-ops-limit=335544320)
    target_include_directories(${demo_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

install_rom(demo_green_lo)
