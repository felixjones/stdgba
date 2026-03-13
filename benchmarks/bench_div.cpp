/// @file benchmarks/bench_div.cpp
/// @brief Benchmark comparing stdgba div/ldiv vs agbabi div/ldiv.
///
/// Uses gba::benchmark helpers for cycle-accurate measurement.
///
/// Tests 32-bit (idiv, uidiv) and 64-bit (ldiv, uldiv) division with
/// various operand patterns: small/large, balanced, worst-case, etc.

#include <cstdint>

#include "bench.hpp"

// stdgba entry points (provided by div.s and ldiv.s)
extern "C" {
// 32-bit: r0 = num / denom, r1 = num % denom
unsigned int __aeabi_uidiv(unsigned int num, unsigned int denom);
int __aeabi_idiv(int num, int denom);

// 64-bit: r0:r1 = num / denom, r2:r3 = num % denom
long long __aeabi_uldivmod(unsigned long long num, unsigned long long denom);
long long __aeabi_ldivmod(long long num, long long denom);
}

// agbabi reference (renamed symbols)
extern "C" {
unsigned int bench_agbabi_uidiv(unsigned int num, unsigned int denom);
int bench_agbabi_idiv(int num, int denom);
long long bench_agbabi_uldivmod(unsigned long long num, unsigned long long denom);
long long bench_agbabi_ldivmod(long long num, long long denom);
}

namespace {

    // Volatile sinks to prevent dead code elimination
    volatile unsigned int sink32;
    volatile long long sink64;

    // Wrapper types for the benchmark template
    using udiv_fn = unsigned int (*)(unsigned int, unsigned int);
    using idiv_fn = int (*)(int, int);
    using uldiv_fn = long long (*)(unsigned long long, unsigned long long);
    using ldiv_fn = long long (*)(long long, long long);

    // Wrappers that store to volatile sink (prevents dead-code elimination)
    void do_udiv_sg(unsigned int a, unsigned int b) {
        sink32 = __aeabi_uidiv(a, b);
    }
    void do_udiv_ab(unsigned int a, unsigned int b) {
        sink32 = bench_agbabi_uidiv(a, b);
    }
    void do_idiv_sg(int a, int b) {
        sink32 = static_cast<unsigned int>(__aeabi_idiv(a, b));
    }
    void do_idiv_ab(int a, int b) {
        sink32 = static_cast<unsigned int>(bench_agbabi_idiv(a, b));
    }

    // For 64-bit div, GCC's calling convention passes 64-bit args in r0:r1 and r2:r3
    void do_uldiv_sg(unsigned long long a, unsigned long long b) {
        sink64 = __aeabi_uldivmod(a, b);
    }
    void do_uldiv_ab(unsigned long long a, unsigned long long b) {
        sink64 = bench_agbabi_uldivmod(a, b);
    }
    void do_ldiv_sg(long long a, long long b) {
        sink64 = __aeabi_ldivmod(a, b);
    }
    void do_ldiv_ab(long long a, long long b) {
        sink64 = bench_agbabi_ldivmod(a, b);
    }

    struct test_case_32 {
        const char* name;
        unsigned int num;
        unsigned int denom;
    };

    struct test_case_64 {
        const char* name;
        unsigned long long num;
        unsigned long long denom;
    };

} // namespace

int main() {
    bench::with_logger([] { bench::log_printf(gba::log::level::info, "=== div/ldiv benchmark (cycles) ==="); });

    // 32-bit unsigned division
    {
        bench::print_header("--- uidiv (32-bit unsigned) ---");
        bench::with_logger(
            [] { bench::log_printf(gba::log::level::info, "  %-24s  stdgba   agbabi  save%%", "Case"); });

        static const test_case_32 cases[] = {
            {            "7 / 3",          7,     3},
            {         "100 / 10",        100,    10},
            {          "255 / 7",        255,     7},
            {        "1000 / 33",       1000,    33},
            {      "65535 / 256",      65535,   256},
            {  "0x10000 / 0x100",    0x10000, 0x100},
            {    "0xFFFFFF / 17",   0xFFFFFF,    17},
            {   "0xFFFFFFFF / 3", 0xFFFFFFFF,     3},
            {"0xFFFFFFFF / 0xFF", 0xFFFFFFFF,  0xFF},
            {   "0xFFFFFFFF / 2", 0xFFFFFFFF,     2},
            {            "1 / 1",          1,     1},
            {            "0 / 1",          0,     1},
            {    "3 / 7 (n < d)",          3,     7},
        };

        for (const auto& [name, num, denom] : cases) {
            const auto sg = bench::measure_avg(64, do_udiv_sg, num, denom);
            const auto ab = bench::measure_avg(64, do_udiv_ab, num, denom);
            bench::print_row(name, sg, ab);
        }
    }

    // 32-bit signed division
    {
        bench::print_header("--- idiv (32-bit signed) ---");
        bench::with_logger(
            [] { bench::log_printf(gba::log::level::info, "  %-24s  stdgba   agbabi  save%%", "Case"); });

        static const struct {
            const char* name;
            int num;
            int denom;
        } cases[] = {
            {       "100 / 10",         100,  10},
            {      "-100 / 10",        -100,  10},
            {      "100 / -10",         100, -10},
            {     "-100 / -10",        -100, -10},
            { "0x7FFFFFFF / 3",  0x7FFFFFFF,   3},
            {"-0x7FFFFFFF / 3", -0x7FFFFFFF,   3},
            {    "1000000 / 7",     1000000,   7},
            {         "-1 / 1",          -1,   1},
        };

        for (const auto& [name, num, denom] : cases) {
            const auto sg = bench::measure_avg(64, do_idiv_sg, num, denom);
            const auto ab = bench::measure_avg(64, do_idiv_ab, num, denom);
            bench::print_row(name, sg, ab);
        }
    }

    // 64-bit unsigned division (64/32 path -- high word of denom is 0)
    {
        bench::print_header("--- uldiv 64/32 (denom fits 32-bit) ---");
        bench::with_logger(
            [] { bench::log_printf(gba::log::level::info, "  %-24s  stdgba   agbabi  save%%", "Case"); });

        static const test_case_64 cases[] = {
            {     "0x100000000 / 3",        0x100000000ULL,    3},
            {  "0x100000000 / 0xFF",        0x100000000ULL, 0xFF},
            {   "1000000000000 / 7",      1000000000000ULL,    7},
            { "0xFFFFFFFFFFFF / 17",     0xFFFFFFFFFFFFULL,   17},
            {"0xFFFFFFFFFFFFFFFF/3", 0xFFFFFFFFFFFFFFFFULL,    3},
            {    "0xFF / 3 (small)",               0xFFULL,    3},
        };

        for (const auto& [name, num, denom] : cases) {
            const auto sg = bench::measure_avg(32, do_uldiv_sg, num, denom);
            const auto ab = bench::measure_avg(32, do_uldiv_ab, num, denom);
            bench::print_row(name, sg, ab);
        }
    }

    // 64-bit unsigned division (64/64 path -- high word of denom is nonzero)
    {
        bench::print_header("--- uldiv 64/64 (full 64-bit denom) ---");
        bench::with_logger(
            [] { bench::log_printf(gba::log::level::info, "  %-24s  stdgba   agbabi  save%%", "Case"); });

        static const test_case_64 cases[] = {
            {             "max / (max/2+1)", 0xFFFFFFFFFFFFFFFFULL, 0x8000000000000000ULL},
            {           "max / 0x100000001", 0xFFFFFFFFFFFFFFFFULL,        0x100000001ULL},
            {           "max / 0x100000003", 0xFFFFFFFFFFFFFFFFULL,        0x100000003ULL},
            {"0xABCD00000000 / 0x100000000",     0xABCD00000000ULL,        0x100000000ULL},
            {             "n < d (1 / max)",                  1ULL, 0xFFFFFFFFFFFFFFFFULL},
        };

        for (const auto& [name, num, denom] : cases) {
            const auto sg = bench::measure_avg(32, do_uldiv_sg, num, denom);
            const auto ab = bench::measure_avg(32, do_uldiv_ab, num, denom);
            bench::print_row(name, sg, ab);
        }
    }

    // 64-bit signed division
    {
        bench::print_header("--- ldiv (64-bit signed) ---");
        bench::with_logger(
            [] { bench::log_printf(gba::log::level::info, "  %-24s  stdgba   agbabi  save%%", "Case"); });

        static const struct {
            const char* name;
            long long num;
            long long denom;
        } cases[] = {
            { "1000000000000 / 7",       1000000000000LL,             7},
            {"-1000000000000 / 7",      -1000000000000LL,             7},
            {"1000000000000 / -7",       1000000000000LL,            -7},
            {       "-10^12 / -7",      -1000000000000LL,            -7},
            {           "max / 3",  0x7FFFFFFFFFFFFFFFLL,             3},
            {          "-max / 3", -0x7FFFFFFFFFFFFFFFLL,             3},
            { "max / 0x100000001",  0x7FFFFFFFFFFFFFFFLL, 0x100000001LL},
        };

        for (const auto& [name, num, denom] : cases) {
            const auto sg = bench::measure_avg(32, do_ldiv_sg, num, denom);
            const auto ab = bench::measure_avg(32, do_ldiv_ab, num, denom);
            bench::print_row(name, sg, ab);
        }
    }

    // Verify correctness of a few representative cases
    gba::test.eq(__aeabi_uidiv(100, 10), 10u);
    gba::test.eq(__aeabi_idiv(-100, 10), -10);
    gba::test.eq(__aeabi_idiv(100, -10), -10);

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
