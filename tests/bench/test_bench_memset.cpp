/// @file tests/bench/test_bench_memset.cpp
/// @brief Benchmark comparing stdgba memset vs agbabi memset.
///
/// Uses GBA hardware timers via bench::timer for cycle-accurate measurement.
/// This is a manual test (excluded from ctest). Run in mgba to see output.

#include <cstring>

#include "bench.hpp"

// stdgba entry points
extern "C" {
    void __aeabi_memset(void*, std::size_t, int);
    void __aeabi_memset4(void*, std::size_t, int);
    void __aeabi_memclr(void*, std::size_t);
    void __aeabi_memclr4(void*, std::size_t);
}

// agbabi reference (renamed symbols from agbabi_memset_ref.s)
extern "C" {
    void bench_agbabi_memset(void*, std::size_t, int);
    void bench_agbabi_memset4(void*, std::size_t, int);
    void bench_agbabi_memclr(void*, std::size_t);
    void bench_agbabi_memclr4(void*, std::size_t);
}

namespace {

// Buffers in EWRAM (for large fill benchmarks)
alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_buf[40960];

// Buffers in IWRAM (for latency-sensitive benchmarks, smaller)
alignas(8) unsigned char iwram_buf[4096];

// Wrappers for bench::measure_avg (void(Args...) signature)
using set_fn = void (*)(void*, std::size_t, int);
using clr_fn = void (*)(void*, std::size_t);

void bench_range(const char* title,
                 unsigned char* buf, std::size_t max_size,
                 set_fn sg_fn, set_fn sg4_fn,
                 set_fn ab_fn, set_fn ab4_fn,
                 const unsigned int* sizes, std::size_t n_sizes) {
    bench::print_header(title);
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %-8s  stdgba stdgba4  agbabi agbabi4  save%%", "Size");
    });

    for (std::size_t si = 0; si < n_sizes; ++si) {
        const auto size = sizes[si];
        if (size > max_size) break;

        const int iters = (size <= 256) ? 64 : (size <= 4096) ? 16 : 4;

        const auto sg  = bench::measure_avg(iters, sg_fn,  buf, size, 0xAB);
        const auto sg4 = bench::measure_avg(iters, sg4_fn, buf, size, 0xAB);
        const auto ab  = bench::measure_avg(iters, ab_fn,  buf, size, 0xAB);
        const auto ab4 = bench::measure_avg(iters, ab4_fn, buf, size, 0xAB);

        const int save = (ab > sg)
            ? static_cast<int>((ab - sg) * 100 / ab)
            : -static_cast<int>((sg - ab) * 100 / sg);

        test::with_logger([&] {
            mgba_printf(MGBA_LOG_INFO, "  %6u  %6u %6u  %6u %6u  %3d%%",
                size, sg, sg4, ab, ab4, save);
        });
    }
}

void bench_clr(const char* title,
               unsigned char* buf, std::size_t max_size,
               clr_fn sg_fn, clr_fn sg4_fn,
               clr_fn ab_fn, clr_fn ab4_fn,
               const unsigned int* sizes, std::size_t n_sizes) {
    bench::print_header(title);
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "  %-8s  stdgba stdgba4  agbabi agbabi4  save%%", "Size");
    });

    for (std::size_t si = 0; si < n_sizes; ++si) {
        const auto size = sizes[si];
        if (size > max_size) break;

        const int iters = (size <= 256) ? 64 : (size <= 4096) ? 16 : 4;

        const auto sg  = bench::measure_avg(iters, sg_fn,  buf, size);
        const auto sg4 = bench::measure_avg(iters, sg4_fn, buf, size);
        const auto ab  = bench::measure_avg(iters, ab_fn,  buf, size);
        const auto ab4 = bench::measure_avg(iters, ab4_fn, buf, size);

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
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "=== memset benchmark (cycles, word-aligned) ===");
    });

    static constexpr unsigned int iwram_sizes[] = {
        4, 8, 12, 16, 20, 24, 28, 32,
        48, 64, 96, 128, 256, 512, 1024, 2048, 4096
    };

    bench_range("--- IWRAM memset (1-cycle access) ---",
        iwram_buf, sizeof(iwram_buf),
        &__aeabi_memset, &__aeabi_memset4,
        &bench_agbabi_memset, &bench_agbabi_memset4,
        iwram_sizes, std::size(iwram_sizes));

    static constexpr unsigned int ewram_sizes[] = {
        32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 38400
    };

    bench_range("--- EWRAM memset (wait states) ---",
        ewram_buf, sizeof(ewram_buf),
        &__aeabi_memset, &__aeabi_memset4,
        &bench_agbabi_memset, &bench_agbabi_memset4,
        ewram_sizes, std::size(ewram_sizes));

    // memclr benchmark
    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "");
        mgba_printf(MGBA_LOG_INFO, "=== memclr benchmark (cycles, word-aligned) ===");
    });

    bench_clr("--- IWRAM memclr ---",
        iwram_buf, sizeof(iwram_buf),
        &__aeabi_memclr, &__aeabi_memclr4,
        &bench_agbabi_memclr, &bench_agbabi_memclr4,
        iwram_sizes, std::size(iwram_sizes));

    bench_clr("--- EWRAM memclr ---",
        ewram_buf, sizeof(ewram_buf),
        &__aeabi_memclr, &__aeabi_memclr4,
        &bench_agbabi_memclr, &bench_agbabi_memclr4,
        ewram_sizes, std::size(ewram_sizes));

    // Verify correctness
    std::memset(ewram_buf, 0xFF, 256);
    __aeabi_memset(ewram_buf, 256, 0xAB);
    for (std::size_t i = 0; i < 256; ++i) {
        ASSERT_EQ(ewram_buf[i], static_cast<unsigned char>(0xAB));
    }

    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "");
        mgba_printf(MGBA_LOG_INFO, "=== benchmark complete ===");
    });

    test::exit(test::failures());
    __builtin_unreachable();
}
