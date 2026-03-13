/// @file benchmarks/bench.hpp
/// @brief Shared benchmark utilities using stdgba timer API.
///
/// Provides measurement helpers and scoped logging utilities backed by
/// gba/logger (no dependency on the vendored mgba.c/mgba.h).
#pragma once

#include <gba/benchmark>
#include <gba/logger>
#include <gba/testing>

#include <cstdarg>
#include <cstdio>
#include <utility>

namespace bench {

// Measure cycles for a void(Args...) function call
template<typename Fn, typename... Args>
[[gnu::noinline]]
unsigned int measure(Fn&& fn, Args&&... args) {
    return gba::benchmark::measure(std::forward<Fn>(fn), std::forward<Args>(args)...);
}

// Average over N iterations
template<typename Fn, typename... Args>
unsigned int measure_avg(int iterations, Fn&& fn, Args&&... args) {
    return gba::benchmark::measure_avg(iterations, std::forward<Fn>(fn), std::forward<Args>(args)...);
}

/// @brief Ensure the logging backend is initialised, then run fn.
template<typename Fn>
inline void with_logger(Fn&& fn) {
    if (!gba::log::get_backend()) {
        gba::log::init();
    }
    std::forward<Fn>(fn)();
}

/// @brief printf-style info log via gba::log (256-char buffer).
inline void log_printf(gba::log::level lvl, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    gba::log::write(lvl, buf);
}

// Print a table header
inline void print_header(const char* title) {
    with_logger([&] {
        log_printf(gba::log::level::info, "");
        log_printf(gba::log::level::info, "%s", title);
    });
}

// Print a row: description, stdgba cycles, agbabi cycles, savings %
inline void print_row(const char* desc, unsigned int sg, unsigned int ab) {
    const int save = (ab > sg)
        ? static_cast<int>((ab - sg) * 100 / ab)
        : -static_cast<int>((sg - ab) * 100 / sg);
    with_logger([&] {
        log_printf(gba::log::level::info, "  %-24s %7u  %7u  %3d%%", desc, sg, ab, save);
    });
}

/// @brief Terminate benchmark via SWI for mgba-headless.
///
/// On GBA, returning from main() falls into an infinite loop in the CRT
/// startup code. Benchmarks must call this instead of `return 0` so that
/// mgba-headless detects program termination via the SWI and exits.
///
/// Uses STDGBA_EXIT_SWI (default 0x1A, from gba/testing).
[[gnu::noreturn, gnu::noinline]]
inline void exit(int code = 0) noexcept {
    register auto r0 asm("r0") = code;
#define BENCH_SWI_STR2(x) #x
#define BENCH_SWI_STR(x) BENCH_SWI_STR2(x)
    asm volatile("swi " BENCH_SWI_STR(STDGBA_EXIT_SWI) " << ((1f - . == 4) * -16); 1:" : : "r"(r0) : "memory");
#undef BENCH_SWI_STR
#undef BENCH_SWI_STR2
    __builtin_unreachable();
}

} // namespace bench
