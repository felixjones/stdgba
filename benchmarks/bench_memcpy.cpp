/// @file benchmarks/bench_memcpy.cpp
/// @brief Benchmark comparing stdgba memcpy vs agbabi memcpy.
///
/// Uses gba::benchmark helpers for cycle-accurate measurement.

#include <cstring>

#include <gba/benchmark>
#include <gba/testing>

using namespace gba::literals;


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

    void bench_range(const char* title, unsigned char* dst, const unsigned char* src, std::size_t max_size,
                     copy_fn sg_fn, copy_fn sg4_fn, copy_fn ab_fn, copy_fn ab4_fn, const unsigned int* sizes,
                     std::size_t n_sizes) {
        gba::benchmark::print_header(title);
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info,
                       "  {size:<8}  stdgba stdgba4  agbabi agbabi4  save%"_fmt,
                       "size"_arg = "Size");
        });

        for (std::size_t si = 0; si < n_sizes; ++si) {
            const auto size = sizes[si];
            if (size > max_size) break;

            const int iters = (size <= 256) ? 64 : (size <= 4096) ? 16 : 4;

            const auto sg = gba::benchmark::measure_avg(iters, sg_fn, dst, src, size);
            const auto sg4 = gba::benchmark::measure_avg(iters, sg4_fn, dst, src, size);
            const auto ab = gba::benchmark::measure_avg(iters, ab_fn, dst, src, size);
            const auto ab4 = gba::benchmark::measure_avg(iters, ab4_fn, dst, src, size);

            const int save = (ab > sg) ? static_cast<int>((ab - sg) * 100 / ab)
                                       : -static_cast<int>((sg - ab) * 100 / sg);

            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info,
                           "  {size:>6}  {sg:>6} {sg4:>6}  {ab:>6} {ab4:>6}  {save:>3}%"_fmt,
                           "size"_arg = size,
                           "sg"_arg = sg,
                           "sg4"_arg = sg4,
                           "ab"_arg = ab,
                           "ab4"_arg = ab4,
                           "save"_arg = save);
            });
        }
    }

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== memcpy benchmark (cycles, word-aligned) ===");
    });

    static constexpr unsigned int iwram_sizes[] = {4,  8,  12,  16,  20,  24,   28,   32,  48,
                                                   64, 96, 128, 256, 512, 1024, 2048, 4096};

    bench_range("--- IWRAM -> IWRAM (1-cycle access) ---", iwram_dst, iwram_src, sizeof(iwram_src), &__aeabi_memcpy,
                &__aeabi_memcpy4, &bench_agbabi_memcpy, &bench_agbabi_memcpy4, iwram_sizes, std::size(iwram_sizes));

    static constexpr unsigned int ewram_sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 38400};

    bench_range("--- EWRAM -> EWRAM (wait states) ---", ewram_dst, ewram_src, sizeof(ewram_src), &__aeabi_memcpy,
                &__aeabi_memcpy4, &bench_agbabi_memcpy, &bench_agbabi_memcpy4, ewram_sizes, std::size(ewram_sizes));

    // Verify correctness
    std::memset(ewram_dst, 0xFF, 256);
    __aeabi_memcpy(ewram_dst, ewram_src, 256);
    for (std::size_t i = 0; i < 256; ++i) {
        gba::test.eq(ewram_dst[i], ewram_src[i]);
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
