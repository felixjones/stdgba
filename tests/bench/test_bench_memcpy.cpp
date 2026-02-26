/**
 * @file tests/bench/test_bench_memcpy.cpp
 * @brief Benchmark comparing stdgba memcpy vs agbabi memcpy.
 *
 * Uses GBA hardware timers via bench::timer for cycle-accurate measurement.
 * This is a manual test (excluded from ctest). Run in mgba to see output.
 */

#include <cstring>

#include "bench.hpp"

// stdgba entry points
extern "C" {
    void __aeabi_memcpy(void*, const void*, std::size_t);
    void __aeabi_memcpy4(void*, const void*, std::size_t);
}

// agbabi reference (renamed symbols from agbabi_memcpy_ref.s)
extern "C" {
    void bench_agbabi_memcpy(void*, const void*, std::size_t);
    void bench_agbabi_memcpy4(void*, const void*, std::size_t);
}

namespace {

// Buffers in EWRAM (for large copy benchmarks)
alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_src[40960];
alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_dst[40960];

// Buffers in IWRAM (for latency-sensitive benchmarks, smaller)
alignas(8) unsigned char iwram_src[4096];
alignas(8) unsigned char iwram_dst[4096];

void fill_src() {
    for (std::size_t i = 0; i < sizeof(ewram_src); ++i) {
        ewram_src[i] = static_cast<unsigned char>(i);
    }
    for (std::size_t i = 0; i < sizeof(iwram_src); ++i) {
        iwram_src[i] = static_cast<unsigned char>(i);
    }
}

using copy_fn = void (*)(void*, const void*, std::size_t);

void bench_range(const char* title,
                 unsigned char* dst, const unsigned char* src, std::size_t max_size,
                 copy_fn sg_fn, copy_fn sg4_fn,
                 copy_fn ab_fn, copy_fn ab4_fn,
                 const unsigned int* sizes, std::size_t n_sizes) {
    bench::print_header(title);
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %-8s  stdgba stdgba4  agbabi agbabi4  save%%", "Size");
    });

    for (std::size_t si = 0; si < n_sizes; ++si) {
        const auto size = sizes[si];
        if (size > max_size) break;

        const int iters = (size <= 256) ? 64 : (size <= 4096) ? 16 : 4;

        const auto sg  = bench::measure_avg(iters, sg_fn,  dst, src, size);
        const auto sg4 = bench::measure_avg(iters, sg4_fn, dst, src, size);
        const auto ab  = bench::measure_avg(iters, ab_fn,  dst, src, size);
        const auto ab4 = bench::measure_avg(iters, ab4_fn, dst, src, size);

        const int save = (ab > sg)
            ? static_cast<int>((ab - sg) * 100 / ab)
            : -static_cast<int>((sg - ab) * 100 / sg);

        test::with_logger([&] {
            mgba_printf(MGBA_LOG_INFO, "  %6u  %6u %6u  %6u %6u  %3d%%",
                size, sg, sg4, ab, ab4, save);
        });
    }
}

} // namespace

int main() {
    fill_src();

    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "=== memcpy benchmark (cycles, word-aligned) ===");
    });

    static constexpr unsigned int iwram_sizes[] = {
        4, 8, 12, 16, 20, 24, 28, 32,
        48, 64, 96, 128, 256, 512, 1024, 2048, 4096
    };

    bench_range("--- IWRAM -> IWRAM (1-cycle access) ---",
        iwram_dst, iwram_src, sizeof(iwram_src),
        &__aeabi_memcpy, &__aeabi_memcpy4,
        &bench_agbabi_memcpy, &bench_agbabi_memcpy4,
        iwram_sizes, std::size(iwram_sizes));

    static constexpr unsigned int ewram_sizes[] = {
        32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 38400
    };

    bench_range("--- EWRAM -> EWRAM (wait states) ---",
        ewram_dst, ewram_src, sizeof(ewram_src),
        &__aeabi_memcpy, &__aeabi_memcpy4,
        &bench_agbabi_memcpy, &bench_agbabi_memcpy4,
        ewram_sizes, std::size(ewram_sizes));

    // Verify correctness
    std::memset(ewram_dst, 0xFF, 256);
    __aeabi_memcpy(ewram_dst, ewram_src, 256);
    for (std::size_t i = 0; i < 256; ++i) {
        ASSERT_EQ(ewram_dst[i], ewram_src[i]);
    }

    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "");
        mgba_printf(MGBA_LOG_INFO, "=== benchmark complete ===");
    });

    test::exit(test::failures());
    __builtin_unreachable();
}
