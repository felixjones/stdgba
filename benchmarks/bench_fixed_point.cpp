/// @file benchmarks/bench_fixed_point.cpp
/// @brief Benchmark fixed-point integral operator fast paths for power-of-two constants.

#include <gba/fixed_point>

#include "bench.hpp"

namespace {

    using fix8 = gba::fixed<int, 8>;
    using ufix8 = gba::fixed<unsigned int, 8>;

    constexpr int iters = 256;

    volatile int runtime_pow2 = 8;
    volatile int runtime_neg_pow2 = -8;
    volatile int runtime_nonpow2 = 3;
    volatile int runtime_fixed_pow2_bits = 2048;
    volatile int runtime_fixed_neg_pow2_bits = -2048;
    // Raw fixed<int,8> bit patterns loaded through volatile to avoid full constant folding.
    volatile int input_mul_bits = 384;
    volatile int input_div_bits = 3072;
    volatile int input_tiny_negative_bits = -1;
    volatile unsigned int input_unsigned_div_bits = 3072u;
    volatile int sink_bits = 0;

    [[gnu::always_inline]]
    inline fix8 load_input(volatile int& raw_bits) {
        const auto raw = raw_bits;
        return __builtin_bit_cast(fix8, raw);
    }

    [[gnu::always_inline]]
    inline ufix8 load_input_u(volatile unsigned int& raw_bits) {
        const auto raw = raw_bits;
        return __builtin_bit_cast(ufix8, raw);
    }

    [[gnu::noinline]]
    void fixed_mul_const_pow2() {
        auto value = load_input(input_mul_bits);
        value *= 8;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_runtime_pow2() {
        auto value = load_input(input_mul_bits);
        value *= runtime_pow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_const_pow2() {
        auto value = load_input(input_div_bits);
        value /= 8;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_runtime_pow2() {
        auto value = load_input(input_div_bits);
        value /= runtime_pow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_fixed_const_pow2() {
        auto value = load_input(input_mul_bits);
        const fix8 rhs = 8.0;
        value *= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_fixed_runtime_pow2() {
        auto value = load_input(input_mul_bits);
        const auto rhs = load_input(runtime_fixed_pow2_bits);
        value *= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_fixed_const_pow2() {
        auto value = load_input(input_div_bits);
        const fix8 rhs = 8.0;
        value /= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_fixed_runtime_pow2() {
        auto value = load_input(input_div_bits);
        const auto rhs = load_input(runtime_fixed_pow2_bits);
        value /= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_fixed_const_neg_pow2() {
        auto value = load_input(input_mul_bits);
        const fix8 rhs = -8.0;
        value *= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_fixed_runtime_neg_pow2() {
        auto value = load_input(input_mul_bits);
        const auto rhs = load_input(runtime_fixed_neg_pow2_bits);
        value *= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_fixed_const_neg_pow2() {
        auto value = load_input(input_div_bits);
        const fix8 rhs = -8.0;
        value /= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_fixed_runtime_neg_pow2() {
        auto value = load_input(input_div_bits);
        const auto rhs = load_input(runtime_fixed_neg_pow2_bits);
        value /= rhs;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_const_neg_pow2() {
        auto value = load_input(input_mul_bits);
        value *= -8;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_runtime_neg_pow2() {
        auto value = load_input(input_mul_bits);
        value *= runtime_neg_pow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_const_neg_pow2() {
        auto value = load_input(input_div_bits);
        value /= -8;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_runtime_neg_pow2() {
        auto value = load_input(input_div_bits);
        value /= runtime_neg_pow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_const_pow2_tiny_negative() {
        auto value = load_input(input_tiny_negative_bits);
        value /= 2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_runtime_pow2_tiny_negative() {
        auto value = load_input(input_tiny_negative_bits);
        value /= (runtime_pow2 >> 2);
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_const_nonpow2() {
        auto value = load_input(input_mul_bits);
        value *= 3;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_mul_runtime_nonpow2() {
        auto value = load_input(input_mul_bits);
        value *= runtime_nonpow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_const_nonpow2() {
        auto value = load_input(input_div_bits);
        value /= 3;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void fixed_div_runtime_nonpow2() {
        auto value = load_input(input_div_bits);
        value /= runtime_nonpow2;
        sink_bits = gba::bit_cast(value);
    }

    [[gnu::noinline]]
    void ufixed_div_const_pow2() {
        auto value = load_input_u(input_unsigned_div_bits);
        value /= 8u;
        sink_bits = static_cast<int>(gba::bit_cast(value));
    }

    [[gnu::noinline]]
    void ufixed_div_runtime_pow2() {
        auto value = load_input_u(input_unsigned_div_bits);
        value /= static_cast<unsigned int>(runtime_pow2);
        sink_bits = static_cast<int>(gba::bit_cast(value));
    }

    [[gnu::noinline]]
    void lhs_mul_const_pow2() {
        auto value = load_input(input_mul_bits);
        auto out = gba::as_lhs(value) * 8;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_mul_runtime_pow2() {
        auto value = load_input(input_mul_bits);
        auto out = gba::as_lhs(value) * runtime_pow2;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_div_const_pow2() {
        auto value = load_input(input_div_bits);
        auto out = gba::as_lhs(value) / 8;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_div_runtime_pow2() {
        auto value = load_input(input_div_bits);
        auto out = gba::as_lhs(value) / runtime_pow2;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_mul_const_neg_pow2() {
        auto value = load_input(input_mul_bits);
        auto out = gba::as_lhs(value) * -8;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_mul_runtime_neg_pow2() {
        auto value = load_input(input_mul_bits);
        auto out = gba::as_lhs(value) * runtime_neg_pow2;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_div_const_neg_pow2() {
        auto value = load_input(input_div_bits);
        auto out = gba::as_lhs(value) / -8;
        sink_bits = gba::bit_cast(out);
    }

    [[gnu::noinline]]
    void lhs_div_runtime_neg_pow2() {
        auto value = load_input(input_div_bits);
        auto out = gba::as_lhs(value) / runtime_neg_pow2;
        sink_bits = gba::bit_cast(out);
    }

    using bench_fn = void (*)();

    void run_row(const char* name, bench_fn constant_literal_fn, bench_fn runtime_variable_fn) {
        const auto constant_cycles = gba::benchmark::measure_avg_net(iters, constant_literal_fn);
        const auto runtime_cycles = gba::benchmark::measure_avg_net(iters, runtime_variable_fn);
        bench::print_row(name, constant_cycles, runtime_cycles);
    }

} // namespace

int main() {
    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "=== fixed_point integral fast-path benchmark (cycles, net) ===");
        bench::log_printf(gba::log::level::info,
                          "left cycle column = constant literal operand; right = runtime variable operand");
    });

    bench::print_header("--- fixed<int,8> direct operators ---");
    run_row("mul *= 8", fixed_mul_const_pow2, fixed_mul_runtime_pow2);
    run_row("div /= 8", fixed_div_const_pow2, fixed_div_runtime_pow2);
    run_row("mul *= -8", fixed_mul_const_neg_pow2, fixed_mul_runtime_neg_pow2);
    run_row("div /= -8", fixed_div_const_neg_pow2, fixed_div_runtime_neg_pow2);
    run_row("mul *= fixed(8)", fixed_mul_fixed_const_pow2, fixed_mul_fixed_runtime_pow2);
    run_row("div /= fixed(8)", fixed_div_fixed_const_pow2, fixed_div_fixed_runtime_pow2);
    run_row("mul *= fixed(-8)", fixed_mul_fixed_const_neg_pow2, fixed_mul_fixed_runtime_neg_pow2);
    run_row("div /= fixed(-8)", fixed_div_fixed_const_neg_pow2, fixed_div_fixed_runtime_neg_pow2);
    run_row("div /= 2 (tiny -)", fixed_div_const_pow2_tiny_negative, fixed_div_runtime_pow2_tiny_negative);
    run_row("mul *= 3 control", fixed_mul_const_nonpow2, fixed_mul_runtime_nonpow2);
    run_row("div /= 3 control", fixed_div_const_nonpow2, fixed_div_runtime_nonpow2);
    run_row("ufixed /= 8", ufixed_div_const_pow2, ufixed_div_runtime_pow2);

    bench::print_header("--- as_lhs(...) wrapper forwarding ---");
    run_row("as_lhs * 8", lhs_mul_const_pow2, lhs_mul_runtime_pow2);
    run_row("as_lhs / 8", lhs_div_const_pow2, lhs_div_runtime_pow2);
    run_row("as_lhs * -8", lhs_mul_const_neg_pow2, lhs_mul_runtime_neg_pow2);
    run_row("as_lhs / -8", lhs_div_const_neg_pow2, lhs_div_runtime_neg_pow2);

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
