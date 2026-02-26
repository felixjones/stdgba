/**
 * @file tests/bench/bench.hpp
 * @brief Shared benchmark utilities using stdgba timer API.
 *
 * Provides a cycle-accurate timer via cascaded TM2+TM3, exposed through
 * gba::reg_tmcnt_h / gba::reg_tmcnt_l_stat / gba::reg_tmcnt_l_reload.
 */
#pragma once

#include <gba/peripherals>

#include "../mgba_test.hpp"

namespace bench {

// Start cascaded 32-bit cycle counter (TM2 = 1-cycle prescaler, TM3 = cascade)
inline void timer_start() {
    gba::reg_tmcnt_h[2] = {};   // disable TM2
    gba::reg_tmcnt_h[3] = {};   // disable TM3
    gba::reg_tmcnt_l[2] = 0;    // reload = 0
    gba::reg_tmcnt_l[3] = 0;    // reload = 0
    gba::reg_tmcnt_h[3] = { .cascade = true, .enabled = true };
    gba::reg_tmcnt_h[2] = { .cycles = gba::cycles_1, .enabled = true };
}

// Read 32-bit cycle count and stop timers
inline unsigned int timer_stop() {
    const unsigned short lo = gba::reg_tmcnt_l_stat[2];
    const unsigned short hi = gba::reg_tmcnt_l_stat[3];
    gba::reg_tmcnt_h[2] = {};
    gba::reg_tmcnt_h[3] = {};
    return static_cast<unsigned int>(lo) | (static_cast<unsigned int>(hi) << 16);
}

// Measure cycles for a void(Args...) function call
template<typename Fn, typename... Args>
[[gnu::noinline]]
unsigned int measure(Fn fn, Args... args) {
    timer_start();
    fn(args...);
    return timer_stop();
}

// Average over N iterations
template<typename Fn, typename... Args>
unsigned int measure_avg(int iterations, Fn fn, Args... args) {
    unsigned int total = 0;
    for (int i = 0; i < iterations; ++i) {
        total += measure(fn, args...);
    }
    return total / static_cast<unsigned int>(iterations);
}

// Print a table header
inline void print_header(const char* title) {
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "");
        mgba_printf(MGBA_LOG_INFO, "%s", title);
    });
}

// Print a row: description, stdgba cycles, agbabi cycles, savings %
inline void print_row(const char* desc, unsigned int sg, unsigned int ab) {
    const int save = (ab > sg)
        ? static_cast<int>((ab - sg) * 100 / ab)
        : -static_cast<int>((sg - ab) * 100 / sg);
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %-24s %7u  %7u  %3d%%", desc, sg, ab, save);
    });
}

} // namespace bench
