/// @file benchmarks/bench_memcmp.cpp
/// @brief Benchmark comparing stdgba memcmp/bcmp vs simple reference.

#include <cstdio>
#include <cstring>

#include <gba/benchmark>

using namespace gba::literals;

extern "C" {
int __stdgba_memcmp(const void*, const void*, std::size_t);
int __stdgba_bcmp(const void*, const void*, std::size_t);
int bench_newlib_memcmp(const void*, const void*, std::size_t);
}

namespace {

    alignas(8) unsigned char iwram_a[4096];
    alignas(8) unsigned char iwram_b[4096];
    alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_a[40960];
    alignas(8) [[gnu::section(".ewram")]] unsigned char ewram_b[40960];

    using cmp_fn = int (*)(const void*, const void*, std::size_t);

    static cmp_fn volatile sg_memcmp = &__stdgba_memcmp;
    static cmp_fn volatile sg_bcmp = &__stdgba_bcmp;
    static cmp_fn volatile nl_memcmp = &bench_newlib_memcmp;

    // Wrappers that discard int return for gba::benchmark::measure (expects void)
    static void do_sg_memcmp(const void* a, const void* b, std::size_t n) {
        volatile int r = sg_memcmp(a, b, n);
        (void)r;
    }
    static void do_sg_bcmp(const void* a, const void* b, std::size_t n) {
        volatile int r = sg_bcmp(a, b, n);
        (void)r;
    }
    static void do_nl_memcmp(const void* a, const void* b, std::size_t n) {
        volatile int r = nl_memcmp(a, b, n);
        (void)r;
    }

    void fill_equal(unsigned char* a, unsigned char* b, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<unsigned char>(i);
            b[i] = static_cast<unsigned char>(i);
        }
    }

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== memcmp/bcmp benchmark ===");
        gba::benchmark::log(gba::log::level::info,
                   "  {size:<8}  {memcmp:>6} {bcmp:>6} {ref:>6}  {save:>5}"_fmt,
                   "size"_arg = "Size",
                   "memcmp"_arg = "memcmp",
                   "bcmp"_arg = "bcmp",
                   "ref"_arg = "ref",
                   "save"_arg = "save%");
    });

    static constexpr unsigned int sizes[] = {4, 8, 12, 16, 20, 24, 28, 32, 48, 64, 96, 128, 256, 512, 1024, 2048, 4096};

    // IWRAM equal data
    gba::benchmark::print_header("--- IWRAM (equal) ---");
    for (auto size : sizes) {
        if (size > sizeof(iwram_a)) break;
        const int iters = (size <= 256) ? 16 : 4;
        fill_equal(iwram_a, iwram_b, size);

        const auto sg = gba::benchmark::measure_avg(iters, &do_sg_memcmp, iwram_a, iwram_b, size);
        const auto bc = gba::benchmark::measure_avg(iters, &do_sg_bcmp, iwram_a, iwram_b, size);
        const auto nl = gba::benchmark::measure_avg(iters, &do_nl_memcmp, iwram_a, iwram_b, size);

        gba::benchmark::with_logger([&] {
            const auto save = (nl > sg) ? static_cast<int>((nl - sg) * 100 / nl)
                                        : -static_cast<int>((sg - nl) * 100 / sg);
            gba::benchmark::log(gba::log::level::info,
                       "  {size:>6}  {sg:>6} {bc:>6} {nl:>6}  {save:>3}%"_fmt,
                       "size"_arg = size,
                       "sg"_arg = sg,
                       "bc"_arg = bc,
                       "nl"_arg = nl,
                       "save"_arg = save);
        });
    }

    // EWRAM equal data
    gba::benchmark::print_header("--- EWRAM (equal) ---");
    for (auto size : {256u, 1024u, 4096u}) {
        if (size > sizeof(ewram_a)) break;
        fill_equal(ewram_a, ewram_b, size);

        const auto sg = gba::benchmark::measure_avg(4, &do_sg_memcmp, ewram_a, ewram_b, size);
        const auto nl = gba::benchmark::measure_avg(4, &do_nl_memcmp, ewram_a, ewram_b, size);

        gba::benchmark::with_logger([&] {
            const auto save = (nl > sg) ? static_cast<int>((nl - sg) * 100 / nl)
                                        : -static_cast<int>((sg - nl) * 100 / sg);
            gba::benchmark::log(gba::log::level::info,
                       "  {size:>6}  {sg:>6} {nl:>6}  {save:>3}%"_fmt,
                       "size"_arg = size,
                       "sg"_arg = sg,
                       "nl"_arg = nl,
                       "save"_arg = save);
        });
    }

    // Early exit: diff at byte 0
    gba::benchmark::print_header("--- IWRAM diff@0 ---");
    {
        fill_equal(iwram_a, iwram_b, 4096);
        iwram_b[0] ^= 0xFF;
        const auto sg = gba::benchmark::measure_avg(16, &do_sg_memcmp, iwram_a, iwram_b, std::size_t(4096));
        const auto nl = gba::benchmark::measure_avg(16, &do_nl_memcmp, iwram_a, iwram_b, std::size_t(4096));
        gba::benchmark::print_row("diff@0 n=4096", sg, nl);
    }

    // Diff at last byte
    gba::benchmark::print_header("--- IWRAM diff@last ---");
    {
        fill_equal(iwram_a, iwram_b, 4096);
        iwram_b[4095] ^= 0xFF;
        const auto sg = gba::benchmark::measure_avg(4, &do_sg_memcmp, iwram_a, iwram_b, std::size_t(4096));
        const auto nl = gba::benchmark::measure_avg(4, &do_nl_memcmp, iwram_a, iwram_b, std::size_t(4096));
        gba::benchmark::print_row("diff@last n=4096", sg, nl);
    }

    // Diff at various positions within a 64-byte buffer
    gba::benchmark::print_header("--- IWRAM diff@pos n=64 ---");
    for (auto pos : {0u, 15u, 16u, 31u, 32u, 47u, 48u, 63u}) {
        fill_equal(iwram_a, iwram_b, 64);
        iwram_b[pos] ^= 0xFF;
        char desc[32];
        std::snprintf(desc, sizeof(desc), "diff@%u n=64", pos);
        const auto sg = gba::benchmark::measure_avg(16, &do_sg_memcmp, iwram_a, iwram_b, std::size_t(64));
        const auto nl = gba::benchmark::measure_avg(16, &do_nl_memcmp, iwram_a, iwram_b, std::size_t(64));
        gba::benchmark::print_row(desc, sg, nl);
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
