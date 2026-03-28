/// @file benchmarks/bench_angle.cpp
/// @brief Benchmark angle multiply/divide fast paths for power-of-two constants.
///
/// Compares compile-time constant power-of-two operands against runtime-variable
/// equivalents and non-power-of-two controls. Pattern mirrors bench_fixed_point.cpp.
///
/// Run with: timeout 15 mgba-headless -S 0x1A -R r0 -t 10 build/benchmarks/bench_angle.elf

#include <gba/angle>

#include "bench.hpp"

namespace {

    constexpr int iters = 256;

    volatile unsigned int runtime_pow2 = 8;
    volatile unsigned int runtime_nonpow2 = 3;
    volatile unsigned int runtime_div2 = 2;
    // Raw angle bit pattern loaded through volatile to avoid full constant folding.
    volatile unsigned int input_mul_bits = 0x10000000u; // ~22.5 degrees
    volatile unsigned int input_div_bits = 0x40000000u; // 90 degrees
    volatile unsigned int sink_bits = 0;

    [[gnu::always_inline]]
    inline gba::angle load_input(volatile unsigned int& raw_bits) {
        return gba::angle{raw_bits};
    }

    // -- Multiply by constant 8 --------------------------------------------------

    [[gnu::noinline]]
    void angle_mul_const_pow2() {
        auto value = load_input(input_mul_bits);
        value *= 8u;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_mul_runtime_pow2() {
        auto value = load_input(input_mul_bits);
        value *= runtime_pow2;
        sink_bits = gba::bit_cast(value);
    }

    // -- Divide by constant 8 ----------------------------------------------------

    [[gnu::noinline]]
    void angle_div_const_pow2() {
        auto value = load_input(input_div_bits);
        value /= 8u;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_div_runtime_pow2() {
        auto value = load_input(input_div_bits);
        value /= runtime_pow2;
        sink_bits = gba::bit_cast(value);
    }

    // -- Divide by constant 2 ----------------------------------------------------

    [[gnu::noinline]]
    void angle_div_const_2() {
        auto value = load_input(input_div_bits);
        value /= 2u;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_div_runtime_2() {
        auto value = load_input(input_div_bits);
        value /= runtime_div2;
        sink_bits = gba::bit_cast(value);
    }

    // -- Controls: non-power-of-two ----------------------------------------------

    [[gnu::noinline]]
    void angle_mul_const_nonpow2() {
        auto value = load_input(input_mul_bits);
        value *= 3u;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_mul_runtime_nonpow2() {
        auto value = load_input(input_mul_bits);
        value *= runtime_nonpow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_div_const_nonpow2() {
        auto value = load_input(input_div_bits);
        value /= 3u;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void angle_div_runtime_nonpow2() {
        auto value = load_input(input_div_bits);
        value /= runtime_nonpow2;
        sink_bits = gba::bit_cast(value);
    }

    // -- Free-function operator variants -----------------------------------------

    [[gnu::noinline]]
    void angle_free_mul_const_pow2() {
        auto value = load_input(input_mul_bits);
        auto result = value * 8u;
        sink_bits = gba::bit_cast(result);
    }

    [[gnu::noinline]]
    void angle_free_mul_runtime_pow2() {
        auto value = load_input(input_mul_bits);
        auto result = value * runtime_pow2;
        sink_bits = gba::bit_cast(result);
    }

    [[gnu::noinline]]
    void angle_free_div_const_pow2() {
        auto value = load_input(input_div_bits);
        auto result = value / 8u;
        sink_bits = gba::bit_cast(result);
    }

    [[gnu::noinline]]
    void angle_free_div_runtime_pow2() {
        auto value = load_input(input_div_bits);
        auto result = value / runtime_pow2;
        sink_bits = gba::bit_cast(result);
    }

} // namespace

int main() {
    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "=== angle power-of-two constant benchmark ===");
        bench::log_printf(gba::log::level::info, "  %d avg-net iterations per measurement", iters);
        bench::log_printf(gba::log::level::info, "  %-36s  %5s  %5s  %5s", "operation", "const", "rtvar", "delta");
    });

    auto report = [](const char* name, unsigned int konst, unsigned int rtvar) {
        bench::with_logger([&] {
            const int delta = static_cast<int>(konst) - static_cast<int>(rtvar);
            bench::log_printf(gba::log::level::info, "  %-36s  %5u  %5u  %+4d", name, konst, rtvar, delta);
        });
    };

    // -- *= pow2 ----------------------------------------------------------
    {
        const auto k = bench::measure_avg(iters, angle_mul_const_pow2);
        const auto r = bench::measure_avg(iters, angle_mul_runtime_pow2);
        report("angle *= 8 (pow2)", k, r);
    }

    // -- /= pow2 ----------------------------------------------------------
    {
        const auto k = bench::measure_avg(iters, angle_div_const_pow2);
        const auto r = bench::measure_avg(iters, angle_div_runtime_pow2);
        report("angle /= 8 (pow2)", k, r);
    }

    // -- /= 2 -------------------------------------------------------------
    {
        const auto k = bench::measure_avg(iters, angle_div_const_2);
        const auto r = bench::measure_avg(iters, angle_div_runtime_2);
        report("angle /= 2", k, r);
    }

    // -- Controls: *= 3, /= 3 (non-power-of-two) -------------------------
    {
        const auto k = bench::measure_avg(iters, angle_mul_const_nonpow2);
        const auto r = bench::measure_avg(iters, angle_mul_runtime_nonpow2);
        report("angle *= 3 (control)", k, r);
    }
    {
        const auto k = bench::measure_avg(iters, angle_div_const_nonpow2);
        const auto r = bench::measure_avg(iters, angle_div_runtime_nonpow2);
        report("angle /= 3 (control)", k, r);
    }

    // -- Free-function operators ------------------------------------------
    {
        const auto k = bench::measure_avg(iters, angle_free_mul_const_pow2);
        const auto r = bench::measure_avg(iters, angle_free_mul_runtime_pow2);
        report("angle * 8 (free fn)", k, r);
    }
    {
        const auto k = bench::measure_avg(iters, angle_free_div_const_pow2);
        const auto r = bench::measure_avg(iters, angle_free_div_runtime_pow2);
        report("angle / 8 (free fn)", k, r);
    }

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
