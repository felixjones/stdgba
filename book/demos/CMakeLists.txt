find_package(librom REQUIRED)
include(CheckCXXSourceCompiles)

file(GLOB DEMO_SOURCES demo_*.cpp)

set(EMBED_DEMOS
    demo_embed_sprite
    demo_hello_audio_wav
    demo_text_render
)

set(SKIPPED_EMBED_DEMOS)

set(_stdgba_embed_probe_file "${CMAKE_CURRENT_BINARY_DIR}/stdgba_embed_probe.bin")
file(WRITE "${_stdgba_embed_probe_file}" "x")
string(REPLACE "\\" "/" _stdgba_embed_probe_file_norm "${_stdgba_embed_probe_file}")
set(STDGBA_EMBED_PROBE_FILE "${_stdgba_embed_probe_file_norm}")
set(_stdgba_embed_probe_source [[
constexpr unsigned char probe[] = {
#embed "@STDGBA_EMBED_PROBE_FILE@" limit(1)
};
int main() { return static_cast<int>(probe[0]); }
]])
string(CONFIGURE "${_stdgba_embed_probe_source}" _stdgba_embed_probe_source @ONLY)

set(_stdgba_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
set(_stdgba_saved_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_REQUIRED_FLAGS "${_stdgba_saved_required_flags} -std=gnu++26")
check_cxx_source_compiles("${_stdgba_embed_probe_source}" STDGBA_HAS_EMBED_SUPPORT_GNUXX26)

if(STDGBA_HAS_EMBED_SUPPORT_GNUXX26)
    set(STDGBA_HAS_EMBED_SUPPORT TRUE)
    set(STDGBA_EMBED_STD_FLAG "-std=gnu++26")
else()
    set(CMAKE_REQUIRED_FLAGS "${_stdgba_saved_required_flags} -std=gnu++2c")
    check_cxx_source_compiles("${_stdgba_embed_probe_source}" STDGBA_HAS_EMBED_SUPPORT_GNUXX2C)
    if(STDGBA_HAS_EMBED_SUPPORT_GNUXX2C)
        set(STDGBA_HAS_EMBED_SUPPORT TRUE)
        set(STDGBA_EMBED_STD_FLAG "-std=gnu++2c")
    else()
        set(STDGBA_HAS_EMBED_SUPPORT FALSE)
        unset(STDGBA_EMBED_STD_FLAG)
    endif()
endif()

set(CMAKE_REQUIRED_FLAGS "${_stdgba_saved_required_flags}")
if(_stdgba_saved_try_compile_target_type)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_stdgba_saved_try_compile_target_type}")
else()
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
endif()

if(NOT STDGBA_HAS_EMBED_SUPPORT)
    set(SKIPPED_EMBED_DEMOS ${EMBED_DEMOS})
    message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} does not support #embed in tested C++26 modes (-std=gnu++26, -std=gnu++2c); skipping #embed demos: ${SKIPPED_EMBED_DEMOS}")
endif()

foreach(demo_src ${DEMO_SOURCES})
    get_filename_component(demo_name ${demo_src} NAME_WE)

    if(demo_name IN_LIST SKIPPED_EMBED_DEMOS)
        continue()
    endif()

    add_executable(${demo_name} ${demo_src})
    target_link_libraries(${demo_name} PRIVATE librom stdgba)

    if(demo_name IN_LIST EMBED_DEMOS)
        target_compile_options(${demo_name} PRIVATE -O3 -g3 -Wno-psabi -ftemplate-depth=10000 -fconstexpr-loop-limit=10000000 -fconstexpr-ops-limit=5000000000 ${STDGBA_EMBED_STD_FLAG})
    else()
        target_compile_options(${demo_name} PRIVATE -Os -g3 -Wno-psabi -fconstexpr-ops-limit=335544320)
    endif()

    target_include_directories(${demo_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

install_rom(demo_green_lo)
