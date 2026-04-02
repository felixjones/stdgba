/// @file benchmarks/bench_lmul.cpp
/// @brief Benchmark comparing stdgba lmul/shifts vs agbabi lmul/shifts.
///
/// Uses gba::benchmark helpers for cycle-accurate measurement.
///
/// Tests 64-bit multiplication (__aeabi_lmul) and 64-bit shifts
/// (__aeabi_llsl, __aeabi_llsr, __aeabi_lasr) with various operand patterns.

#include <cstdint>

#include <gba/benchmark>
#include <gba/testing>

using namespace gba::literals;

// stdgba entry points (provided by lmul.s)
extern "C" {
long long __aeabi_lmul(long long a, long long b);
long long __aeabi_llsl(long long value, int shift);
long long __aeabi_llsr(long long value, int shift);
long long __aeabi_lasr(long long value, int shift);
}

// agbabi reference (renamed symbols)
extern "C" {
long long bench_agbabi_lmul(long long a, long long b);
long long bench_agbabi_llsl(long long value, int shift);
long long bench_agbabi_llsr(long long value, int shift);
long long bench_agbabi_lasr(long long value, int shift);
}

namespace {

    // Volatile sinks to prevent dead code elimination
    volatile long long sink64;

    // Wrapper functions that store to volatile sink
    void do_lmul_sg(long long a, long long b) {
        sink64 = __aeabi_lmul(a, b);
    }
    void do_lmul_ab(long long a, long long b) {
        sink64 = bench_agbabi_lmul(a, b);
    }

    void do_llsl_sg(long long v, int n) {
        sink64 = __aeabi_llsl(v, n);
    }
    void do_llsl_ab(long long v, int n) {
        sink64 = bench_agbabi_llsl(v, n);
    }

    void do_llsr_sg(long long v, int n) {
        sink64 = __aeabi_llsr(v, n);
    }
    void do_llsr_ab(long long v, int n) {
        sink64 = bench_agbabi_llsr(v, n);
    }

    void do_lasr_sg(long long v, int n) {
        sink64 = __aeabi_lasr(v, n);
    }
    void do_lasr_ab(long long v, int n) {
        sink64 = bench_agbabi_lasr(v, n);
    }

    struct mul_case {
        const char* name;
        long long a;
        long long b;
    };

    struct shift_case {
        const char* name;
        long long value;
        int shift;
    };

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== lmul/shift benchmark (cycles) ===");
    });

    // 64-bit multiplication
    {
        gba::benchmark::print_header("--- lmul (64-bit multiply) ---");
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info,
                       "  {case:<24}  stdgba   agbabi  save%"_fmt,
                       "case"_arg = "Case");
        });

        static const mul_case cases[] = {
            // Small x small (best-case mul timing: Rs fits in 1 byte)
            {                    "3 * 5",            3LL,           5LL},
            {              "0xFF * 0xFF",         0xFFLL,        0xFFLL},
            // Small x large (mixed mul timing)
            {        "0xFF * 0xFFFFFFFF",         0xFFLL,  0xFFFFFFFFLL},
            {          "7 * 0x100000000",            7LL, 0x100000000LL},
            // Large x large (worst-case mul timing: Rs uses all 4 bytes)
            {  "0xFFFFFFFF * 0xFFFFFFFF",   0xFFFFFFFFLL,  0xFFFFFFFFLL},
            {  "0x12345678 * 0x9ABCDEF0",   0x12345678LL,  0x9ABCDEF0LL},
            // Full 64-bit operands
            {      "0x123456789A * 0xAB", 0x123456789ALL,        0xABLL},
            {   "0xFFFFFFFFFFFFFFFF * 2",           -1LL,           2LL},
            {"0x100000000 * 0x100000000",  0x100000000LL, 0x100000000LL},
            // Cross-product heavy
            {  "0xFFFF0000 * 0x0000FFFF",   0xFFFF0000LL,  0x0000FFFFLL},
            // Zero cases
            {   "0 * 0xFFFFFFFFFFFFFFFF",            0LL,          -1LL},
            {                    "1 * 1",            1LL,           1LL},
        };

        for (const auto& [name, a, b] : cases) {
            const auto sg = gba::benchmark::measure_avg(64, do_lmul_sg, a, b);
            const auto ab = gba::benchmark::measure_avg(64, do_lmul_ab, a, b);
            gba::benchmark::print_row(name, sg, ab);
        }
    }

    // 64-bit logical shift left
    {
        gba::benchmark::print_header("--- llsl (64-bit logical shift left) ---");
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info,
                       "  {case:<24}  stdgba   agbabi  save%"_fmt,
                       "case"_arg = "Case");
        });

        constexpr auto val = static_cast<long long>(0xDEADBEEFCAFEBABEULL);
        static const shift_case cases[] = {
            { "val << 0", val,  0},
            { "val << 1", val,  1},
            {"val << 15", val, 15},
            {"val << 16", val, 16},
            {"val << 31", val, 31},
            {"val << 32", val, 32},
            {"val << 33", val, 33},
            {"val << 48", val, 48},
            {"val << 63", val, 63},
            {   "1 << 0", 1LL,  0},
            {  "1 << 32", 1LL, 32},
            {  "1 << 63", 1LL, 63},
        };

        for (const auto& [name, value, shift] : cases) {
            const auto sg = gba::benchmark::measure_avg(64, do_llsl_sg, value, shift);
            const auto ab = gba::benchmark::measure_avg(64, do_llsl_ab, value, shift);
            gba::benchmark::print_row(name, sg, ab);
        }
    }

    // 64-bit logical shift right
    {
        gba::benchmark::print_header("--- llsr (64-bit logical shift right) ---");
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info,
                       "  {case:<24}  stdgba   agbabi  save%"_fmt,
                       "case"_arg = "Case");
        });

        constexpr auto val = static_cast<long long>(0xDEADBEEFCAFEBABEULL);
        static const shift_case cases[] = {
            { "val >> 0",                  val,  0},
            { "val >> 1",                  val,  1},
            {"val >> 15",                  val, 15},
            {"val >> 16",                  val, 16},
            {"val >> 31",                  val, 31},
            {"val >> 32",                  val, 32},
            {"val >> 33",                  val, 33},
            {"val >> 48",                  val, 48},
            {"val >> 63",                  val, 63},
            { "max >> 1", 0x7FFFFFFFFFFFFFFFLL,  1},
            {"max >> 32", 0x7FFFFFFFFFFFFFFFLL, 32},
        };

        for (const auto& [name, value, shift] : cases) {
            const auto sg = gba::benchmark::measure_avg(64, do_llsr_sg, value, shift);
            const auto ab = gba::benchmark::measure_avg(64, do_llsr_ab, value, shift);
            gba::benchmark::print_row(name, sg, ab);
        }
    }

    // 64-bit arithmetic shift right
    {
        gba::benchmark::print_header("--- lasr (64-bit arithmetic shift right) ---");
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info,
                       "  {case:<24}  stdgba   agbabi  save%"_fmt,
                       "case"_arg = "Case");
        });

        constexpr auto neg_val = static_cast<long long>(0xDEADBEEFCAFEBABEULL);
        constexpr auto pos_val = 0x7EADBEEFCAFEBABELL;
        static const shift_case cases[] = {
            {         "-1 >> 1",                                  -1LL,  1},
            {        "-1 >> 31",                                  -1LL, 31},
            {        "-1 >> 32",                                  -1LL, 32},
            {        "-1 >> 63",                                  -1LL, 63},
            {"-0x80000000 >> 1", static_cast<long long>(-0x80000000LL),  1},
            {   "neg_val >> 16",                               neg_val, 16},
            {   "neg_val >> 32",                               neg_val, 32},
            {   "neg_val >> 48",                               neg_val, 48},
            {   "pos_val >> 16",                               pos_val, 16},
            {   "pos_val >> 32",                               pos_val, 32},
            {   "pos_val >> 48",                               pos_val, 48},
        };

        for (const auto& [name, value, shift] : cases) {
            const auto sg = gba::benchmark::measure_avg(64, do_lasr_sg, value, shift);
            const auto ab = gba::benchmark::measure_avg(64, do_lasr_ab, value, shift);
            gba::benchmark::print_row(name, sg, ab);
        }
    }

    // Correctness checks
    {
        // lmul
        gba::test.eq(__aeabi_lmul(2, 3), 6LL);
        gba::test.eq(__aeabi_lmul(0, -1LL), 0LL);
        gba::test.eq(__aeabi_lmul(0x100000000LL, 0x100000000LL), 0LL); // overflow
        gba::test.eq(__aeabi_lmul(0xFFFFFFFFLL, 0xFFFFFFFFLL), 0xFFFFFFFE00000001LL);
        gba::test.eq(__aeabi_lmul(-1LL, 2LL), -2LL);
        gba::test.eq(__aeabi_lmul(0x12345678LL, 1LL), 0x12345678LL);

        // llsl
        gba::test.eq(__aeabi_llsl(1, 0), 1LL);
        gba::test.eq(__aeabi_llsl(1, 1), 2LL);
        gba::test.eq(__aeabi_llsl(1, 31), 0x80000000LL);
        gba::test.eq(__aeabi_llsl(1, 32), 0x100000000LL);
        gba::test.eq(__aeabi_llsl(1, 63), static_cast<long long>(0x8000000000000000ULL));
        gba::test.eq(__aeabi_llsl(0xCAFEBABELL, 0), 0xCAFEBABELL);

        // llsr
        gba::test.eq(__aeabi_llsr(0x100000000LL, 32), 1LL);
        gba::test.eq(__aeabi_llsr(static_cast<long long>(0x8000000000000000ULL), 63), 1LL);
        gba::test.eq(__aeabi_llsr(0xFFLL, 4), 0xFLL);
        gba::test.eq(__aeabi_llsr(-1LL, 32), 0xFFFFFFFFLL);

        // lasr
        gba::test.eq(__aeabi_lasr(-1LL, 1), -1LL);
        gba::test.eq(__aeabi_lasr(-1LL, 32), -1LL);
        gba::test.eq(__aeabi_lasr(-1LL, 63), -1LL);
        gba::test.eq(__aeabi_lasr(-256LL, 4), -16LL);
        gba::test.eq(__aeabi_lasr(0x7FFFFFFFFFFFFFFFLL, 32), 0x7FFFFFFFLL);
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
