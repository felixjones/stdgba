/**
 * @file tests/bench/test_bench_inline_memcmp.cpp
 * @brief Benchmark the compile-time specialisation paths in memcmp.cpp
 *        to determine optimal inlining thresholds.
 *
 * Tests three questions:
 *   1. What is the crossover for inline byte compares vs __stdgba_memcmp?
 *   2. What is the crossover for inline word XOR vs __stdgba_memcmp?
 *   3. Does the memcmp wrapper (with all specialisations) beat a raw call?
 *
 * All from ROM/Thumb context (the common case that determines the cap).
 *
 * This is a manual test (excluded from ctest). Run in mgba to see output.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bench.hpp"

extern "C" {
    int __stdgba_memcmp(const void*, const void*, std::size_t);
    int __stdgba_bcmp(const void*, const void*, std::size_t);
}

// Volatile function pointers to prevent devirtualisation
static int (*volatile fn_memcmp)(const void*, const void*, std::size_t) = &__stdgba_memcmp;
static int (*volatile fn_bcmp)(const void*, const void*, std::size_t) = &__stdgba_bcmp;

namespace {

constexpr std::size_t MAX_N = 128;
constexpr int ITERS = 256;

// Part 1: inline byte-by-byte memcmp vs __stdgba_memcmp call
// ROM/Thumb context, unaligned pointers (byte-only path)
template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_memcmp_bytes_inline() {
    unsigned char a[MAX_N];
    unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        // Inline N byte compares
        auto pa = static_cast<volatile const unsigned char*>(a);
        auto pb = static_cast<volatile const unsigned char*>(b);
        int r = 0;
        if constexpr (N >= 1) { if (!r && pa[0] != pb[0]) r = static_cast<int>(pa[0]) - static_cast<int>(pb[0]); }
        if constexpr (N >= 2) { if (!r && pa[1] != pb[1]) r = static_cast<int>(pa[1]) - static_cast<int>(pb[1]); }
        if constexpr (N >= 3) { if (!r && pa[2] != pb[2]) r = static_cast<int>(pa[2]) - static_cast<int>(pb[2]); }
        if constexpr (N >= 4) { if (!r && pa[3] != pb[3]) r = static_cast<int>(pa[3]) - static_cast<int>(pb[3]); }
        if constexpr (N >= 5) { if (!r && pa[4] != pb[4]) r = static_cast<int>(pa[4]) - static_cast<int>(pb[4]); }
        if constexpr (N >= 6) { if (!r && pa[5] != pb[5]) r = static_cast<int>(pa[5]) - static_cast<int>(pb[5]); }
        if constexpr (N >= 7) { if (!r && pa[6] != pb[6]) r = static_cast<int>(pa[6]) - static_cast<int>(pb[6]); }
        if constexpr (N >= 8) { if (!r && pa[7] != pb[7]) r = static_cast<int>(pa[7]) - static_cast<int>(pb[7]); }
        result = r;
        total += bench::timer_stop();
    }
    return total / ITERS;
}

template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_memcmp_bytes_call() {
    unsigned char a[MAX_N];
    unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = fn_memcmp(a, b, N);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

// Part 2: inline word XOR compare vs __stdgba_memcmp call
// Word-aligned, word-multiple sizes (tests the .cpp word-XOR specialisation)
template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_memcmp_words_inline() {
    static_assert(N % 4 == 0 && N > 0);
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        // Inline word XOR compare (same logic as memcmp.cpp specialisation 3)
        auto p1 = reinterpret_cast<volatile const std::uint32_t*>(a);
        auto p2 = reinterpret_cast<volatile const std::uint32_t*>(b);
        int r = 0;
        for (std::size_t w = 0; w < N / 4; ++w) {
            const auto w1 = p1[w];
            const auto w2 = p2[w];
            if (w1 != w2) {
                auto ba = reinterpret_cast<volatile const unsigned char*>(&p1[w]);
                auto bb = reinterpret_cast<volatile const unsigned char*>(&p2[w]);
                for (int j = 0; j < 4; ++j) {
                    if (ba[j] != bb[j]) { r = static_cast<int>(ba[j]) - static_cast<int>(bb[j]); break; }
                }
                break;
            }
        }
        result = r;
        total += bench::timer_stop();
    }
    return total / ITERS;
}

template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_memcmp_words_call() {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = fn_memcmp(a, b, N);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

// Part 3: memcmp wrapper (with inlining) vs __stdgba_memcmp directly
// Tests the actual benefit of the .cpp wrapper when the compiler can
// prove constant size and word alignment at -O3.
template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_wrapper_memcmp() {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        // std::memcmp goes through our wrapper -- LTO + -O3 enables
        // compile-time specialisation when N and alignment are known.
        result = std::memcmp(a, b, N);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_direct_memcmp() {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = fn_memcmp(a, b, N);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

// Part 4: bcmp wrapper vs __stdgba_bcmp directly
template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_wrapper_bcmp() {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        // GCC may lower memcmp()==0 to bcmp at -O2+
        result = (std::memcmp(a, b, N) == 0) ? 0 : 1;
        total += bench::timer_stop();
    }
    return total / ITERS;
}

template<std::size_t N>
[[gnu::noinline]]
unsigned int bench_direct_bcmp() {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = fn_bcmp(a, b, N);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

// Part 5: volatile-size memcmp through wrapper vs direct
// When N is NOT a constant, the wrapper should just forward to __stdgba_memcmp
// with zero overhead. This verifies there's no regression.
[[gnu::noinline]]
unsigned int bench_wrapper_volatile_n(std::size_t n) {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    volatile std::size_t vn = n;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = std::memcmp(a, b, vn);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

[[gnu::noinline]]
unsigned int bench_direct_volatile_n(std::size_t n) {
    alignas(4) unsigned char a[MAX_N];
    alignas(4) unsigned char b[MAX_N];
    for (std::size_t i = 0; i < MAX_N; i++) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
    volatile int result = 0;
    unsigned int total = 0;
    volatile std::size_t vn = n;
    for (int i = 0; i < ITERS; i++) {
        asm volatile("" ::: "memory");
        bench::timer_start();
        result = fn_memcmp(a, b, vn);
        total += bench::timer_stop();
    }
    return total / ITERS;
}

template<std::size_t N>
void run_bytes_cmp() {
    const auto inl = bench_memcmp_bytes_inline<N>();
    const auto call = bench_memcmp_bytes_call<N>();
    const char* w = (inl <= call) ? "inline" : "CALL";
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %3u  %6u %6u  %s",
            static_cast<unsigned>(N), inl, call, w);
    });
}

template<std::size_t N>
void run_words_cmp() {
    const auto inl = bench_memcmp_words_inline<N>();
    const auto call = bench_memcmp_words_call<N>();
    const char* w = (inl <= call) ? "inline" : "CALL";
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %3u  %6u %6u  %s",
            static_cast<unsigned>(N), inl, call, w);
    });
}

template<std::size_t N>
void run_wrapper_cmp() {
    const auto wrap = bench_wrapper_memcmp<N>();
    const auto direct = bench_direct_memcmp<N>();
    const int save = (direct > wrap)
        ? static_cast<int>((direct - wrap) * 100 / direct)
        : -static_cast<int>((wrap - direct) * 100 / wrap);
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %3u  %6u %6u  %3d%%",
            static_cast<unsigned>(N), wrap, direct, save);
    });
}

template<std::size_t N>
void run_wrapper_bcmp() {
    const auto wrap = bench_wrapper_bcmp<N>();
    const auto direct = bench_direct_bcmp<N>();
    const int save = (direct > wrap)
        ? static_cast<int>((direct - wrap) * 100 / direct)
        : -static_cast<int>((wrap - direct) * 100 / wrap);
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %3u  %6u %6u  %3d%%",
            static_cast<unsigned>(N), wrap, direct, save);
    });
}

void run_volatile_n(std::size_t n) {
    const auto wrap = bench_wrapper_volatile_n(n);
    const auto direct = bench_direct_volatile_n(n);
    const int save = (direct > wrap)
        ? static_cast<int>((direct - wrap) * 100 / direct)
        : -static_cast<int>((wrap - direct) * 100 / wrap);
    test::with_logger([&] {
        mgba_printf(MGBA_LOG_INFO, "  %3u  %6u %6u  %3d%%",
            static_cast<unsigned>(n), wrap, direct, save);
    });
}

} // namespace

int main() {
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO,
            "=== memcmp inline specialisation benchmarks (ROM/Thumb, -O3) ===");
        mgba_printf(MGBA_LOG_INFO, "");
    });

    // Part 1: byte threshold
    bench::print_header("--- memcmp: inline bytes vs __stdgba_memcmp call ---");
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %3s  %6s %6s  %s",
            "N", "inline", "call", "winner");
    });
    run_bytes_cmp<1>();
    run_bytes_cmp<2>();
    run_bytes_cmp<3>();
    run_bytes_cmp<4>();
    run_bytes_cmp<5>();
    run_bytes_cmp<6>();
    run_bytes_cmp<7>();
    run_bytes_cmp<8>();

    // Part 2: word XOR threshold
    bench::print_header("--- memcmp: inline word XOR vs __stdgba_memcmp call (aligned) ---");
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %3s  %6s %6s  %s",
            "N", "inline", "call", "winner");
    });
    run_words_cmp<4>();
    run_words_cmp<8>();
    run_words_cmp<12>();
    run_words_cmp<16>();
    run_words_cmp<20>();
    run_words_cmp<24>();
    run_words_cmp<28>();
    run_words_cmp<32>();
    run_words_cmp<48>();
    run_words_cmp<64>();

    // Part 3: wrapper with constant N (aligned) vs direct call
    bench::print_header("--- memcmp: wrapper (const N, aligned) vs direct call ---");
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %3s  %6s %6s  %5s",
            "N", "wrap", "direct", "save%");
    });
    run_wrapper_cmp<1>();
    run_wrapper_cmp<2>();
    run_wrapper_cmp<4>();
    run_wrapper_cmp<8>();
    run_wrapper_cmp<12>();
    run_wrapper_cmp<16>();
    run_wrapper_cmp<20>();
    run_wrapper_cmp<24>();
    run_wrapper_cmp<28>();
    run_wrapper_cmp<32>();
    run_wrapper_cmp<48>();
    run_wrapper_cmp<64>();
    run_wrapper_cmp<128>();

    // Part 4: bcmp wrapper with constant N (aligned) vs direct call
    bench::print_header("--- bcmp: wrapper (const N, aligned) vs direct call ---");
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %3s  %6s %6s  %5s",
            "N", "wrap", "direct", "save%");
    });
    run_wrapper_bcmp<1>();
    run_wrapper_bcmp<4>();
    run_wrapper_bcmp<8>();
    run_wrapper_bcmp<16>();
    run_wrapper_bcmp<32>();
    run_wrapper_bcmp<64>();

    // Part 5: volatile N (wrapper should just forward, no regression)
    bench::print_header("--- memcmp: wrapper (volatile N) vs direct call ---");
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %3s  %6s %6s  %5s",
            "N", "wrap", "direct", "save%");
    });
    run_volatile_n(4);
    run_volatile_n(16);
    run_volatile_n(32);
    run_volatile_n(64);
    run_volatile_n(128);

    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "");
        mgba_printf(MGBA_LOG_INFO, "=== benchmark complete ===");
    });

    test::exit(test::failures());
    __builtin_unreachable();
}
