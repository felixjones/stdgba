/// @file benchmarks/bench_memmove.cpp
/// @brief Benchmark comparing stdgba memmove vs agbabi memmove.
///
/// Focuses on the backward copy path (the interesting case for memmove).
/// Forward copy delegates to memcpy, so that's already benchmarked separately.
///
/// Uses gba::benchmark helpers for cycle-accurate measurement.

#include <cstring>

#include "bench.hpp"

// stdgba entry points
extern "C" {
void __aeabi_memmove(void*, const void*, std::size_t);
void __aeabi_memmove4(void*, const void*, std::size_t);
}

// agbabi reference
extern "C" {
void bench_agbabi_memmove(void*, const void*, std::size_t);
void bench_agbabi_memmove4(void*, const void*, std::size_t);
}

namespace {

    // IWRAM buffers (overlap scenarios use a single buffer)
    alignas(8) unsigned char iwram_buf[8192];

    // EWRAM buffers
    alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_buf[65536];

    using move_fn = void (*)(void*, const void*, std::size_t);

    // Volatile function pointers to prevent devirtualisation
    static move_fn volatile sg_fn = &__aeabi_memmove;
    static move_fn volatile sg4_fn = &__aeabi_memmove4;
    static move_fn volatile ab_fn = &bench_agbabi_memmove;
    static move_fn volatile ab4_fn = &bench_agbabi_memmove4;

    void fill_buf(unsigned char* buf, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) buf[i] = static_cast<unsigned char>(i);
    }

    void bench_backward(const char* title, unsigned char* buf, std::size_t max_size, unsigned int overlap,
                        const unsigned int* sizes, std::size_t n_sizes) {
        bench::print_header(title);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-8s  %6s %6s  %6s %6s  %5s", "Size", "stdgba", "stdg4",
                              "agbabi", "agb4", "save%");
        });

        for (std::size_t si = 0; si < n_sizes; ++si) {
            const auto size = sizes[si];
            if (size + overlap > max_size) break;

            const int iters = (size <= 256) ? 64 : (size <= 4096) ? 16 : 4;

            // Backward: dest = buf + overlap, src = buf, n = size
            // This forces backward copy since dest > src and dest < src + size
            fill_buf(buf, size + overlap);

            const auto sg = bench::measure_avg(iters, sg_fn, buf + overlap, buf, size);
            fill_buf(buf, size + overlap);
            const auto sg4 = bench::measure_avg(iters, sg4_fn, buf + overlap, buf, size);
            fill_buf(buf, size + overlap);
            const auto ab = bench::measure_avg(iters, ab_fn, buf + overlap, buf, size);
            fill_buf(buf, size + overlap);
            const auto ab4 = bench::measure_avg(iters, ab4_fn, buf + overlap, buf, size);

            const int save = (ab > sg) ? static_cast<int>((ab - sg) * 100 / ab)
                                       : -static_cast<int>((sg - ab) * 100 / sg);

            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %6u  %6u %6u  %6u %6u  %3d%%", size, sg, sg4, ab, ab4,
                                  save);
            });
        }
    }

} // namespace

int main() {
    bench::with_logger(
        [] { bench::log_printf(gba::log::level::info, "=== memmove benchmark (backward copy, word-aligned) ==="); });

    static constexpr unsigned int iwram_sizes[] = {4,  8,  12,  16,  20,  24,   28,   32,  48,
                                                   64, 96, 128, 256, 512, 1024, 2048, 4096};

    // Overlap by 4 bytes (word): common array shift
    bench_backward("--- IWRAM backward (overlap=4) ---", iwram_buf, sizeof(iwram_buf), 4, iwram_sizes,
                   std::size(iwram_sizes));

    // Overlap by 32 bytes: partial buffer shift
    bench_backward("--- IWRAM backward (overlap=32) ---", iwram_buf, sizeof(iwram_buf), 32, iwram_sizes,
                   std::size(iwram_sizes));

    static constexpr unsigned int ewram_sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 38400};

    bench_backward("--- EWRAM backward (overlap=4) ---", ewram_buf, sizeof(ewram_buf), 4, ewram_sizes,
                   std::size(ewram_sizes));

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
