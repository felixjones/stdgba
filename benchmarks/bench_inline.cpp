/// @file benchmarks/bench_inline.cpp
/// @brief Benchmark the compile-time specialisation paths in memcpy.cpp
/// and memset.cpp to determine optimal thresholds.
///
/// Tests three questions:
/// 1. What is the crossover for inline byte stores vs __aeabi_memcpy/memset?
/// 2. What is the crossover for inline str vs __aeabi_memcpy4/memset4?
/// 3. Does calling __aeabi_*4 directly (skipping alignment check) help?
///
/// All from ROM/Thumb context (the common case that determines the cap).
///

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gba/benchmark>

using namespace gba::literals;

extern "C" {
void __aeabi_memcpy(void*, const void*, std::size_t);
void __aeabi_memcpy4(void*, const void*, std::size_t);
void __aeabi_memset(void*, std::size_t, int);
void __aeabi_memset4(void*, std::size_t, int);
}

// Volatile function pointers to prevent devirtualisation
static void (*volatile fn_memcpy)(void*, const void*, std::size_t) = &__aeabi_memcpy;
static void (*volatile fn_memcpy4)(void*, const void*, std::size_t) = &__aeabi_memcpy4;
static void (*volatile fn_memset)(void*, std::size_t, int) = &__aeabi_memset;
static void (*volatile fn_memset4)(void*, std::size_t, int) = &__aeabi_memset4;

namespace {

constexpr std::size_t MAX_N = 128;
constexpr int ITERS = 256;

    // Part 1: byte-by-byte memcpy (inline ldrb/strb sequence) vs call
    // ROM/Thumb context, unaligned pointers (byte-only)
    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memcpy_bytes_inline() {
        char dst[MAX_N];
        char src[MAX_N];
        for (std::size_t i = 0; i < MAX_N; i++) src[i] = static_cast<char>(i + 1);
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] {
                // Inline N byte copies
                auto d = reinterpret_cast<volatile unsigned char*>(dst);
                auto s = reinterpret_cast<volatile const unsigned char*>(src);
                if constexpr (N >= 1) d[0] = s[0];
                if constexpr (N >= 2) d[1] = s[1];
                if constexpr (N >= 3) d[2] = s[2];
                if constexpr (N >= 4) d[3] = s[3];
                if constexpr (N >= 5) d[4] = s[4];
                if constexpr (N >= 6) d[5] = s[5];
                if constexpr (N >= 7) d[6] = s[6];
                if constexpr (N >= 8) d[7] = s[7];
                if constexpr (N >= 9) d[8] = s[8];
                if constexpr (N >= 10) d[9] = s[9];
                if constexpr (N >= 11) d[10] = s[10];
                if constexpr (N >= 12) d[11] = s[11];
                if constexpr (N >= 13) d[12] = s[12];
                if constexpr (N >= 14) d[13] = s[13];
                if constexpr (N >= 15) d[14] = s[14];
                if constexpr (N >= 16) d[15] = s[15];
            });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memcpy_bytes_call() {
        char dst[MAX_N];
        char src[MAX_N];
        for (std::size_t i = 0; i < MAX_N; i++) src[i] = static_cast<char>(i + 1);
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memcpy(dst, src, N); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    // Part 2: byte-by-byte memset (inline strb sequence) vs call
    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memset_bytes_inline() {
        char dst[MAX_N];
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] {
                auto d = reinterpret_cast<volatile unsigned char*>(dst);
                if constexpr (N >= 1) d[0] = 0xAB;
                if constexpr (N >= 2) d[1] = 0xAB;
                if constexpr (N >= 3) d[2] = 0xAB;
                if constexpr (N >= 4) d[3] = 0xAB;
                if constexpr (N >= 5) d[4] = 0xAB;
                if constexpr (N >= 6) d[5] = 0xAB;
                if constexpr (N >= 7) d[6] = 0xAB;
                if constexpr (N >= 8) d[7] = 0xAB;
                if constexpr (N >= 9) d[8] = 0xAB;
                if constexpr (N >= 10) d[9] = 0xAB;
                if constexpr (N >= 11) d[10] = 0xAB;
                if constexpr (N >= 12) d[11] = 0xAB;
                if constexpr (N >= 13) d[12] = 0xAB;
                if constexpr (N >= 14) d[13] = 0xAB;
                if constexpr (N >= 15) d[14] = 0xAB;
                if constexpr (N >= 16) d[15] = 0xAB;
            });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memset_bytes_call() {
        char dst[MAX_N];
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memset(dst, N, 0xAB); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    // Part 3: __aeabi_memcpy vs __aeabi_memcpy4 when alignment is known
    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memcpy_generic() {
        alignas(4) char dst[MAX_N];
        alignas(4) char src[MAX_N];
        for (std::size_t i = 0; i < MAX_N; i++) src[i] = static_cast<char>(i + 1);
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memcpy(dst, src, N); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memcpy_aligned() {
        alignas(4) char dst[MAX_N];
        alignas(4) char src[MAX_N];
        for (std::size_t i = 0; i < MAX_N; i++) src[i] = static_cast<char>(i + 1);
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memcpy4(dst, src, N); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    // Part 4: __aeabi_memset vs __aeabi_memset4 when alignment is known
    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memset_generic() {
        alignas(4) char dst[MAX_N];
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memset(dst, N, 0xAB); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    template<std::size_t N>
    [[gnu::noinline]]
    unsigned int bench_memset_aligned() {
        alignas(4) char dst[MAX_N];
        unsigned int total = 0;
        for (int i = 0; i < ITERS; i++) {
            asm volatile("" ::: "memory");
            total += gba::benchmark::measure([&] { fn_memset4(dst, N, 0xAB); });
            asm volatile("" ::"m"(dst));
        }
        return total / ITERS;
    }

    template<std::size_t N>
    void run_bytes_cpy() {
        const auto inl = bench_memcpy_bytes_inline<N>();
        const auto call = bench_memcpy_bytes_call<N>();
        const char* winner = (inl <= call) ? "inline" : "CALL";
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info,
                       "  {n:>3}  {inl:>6} {call:>6}  {winner}"_fmt,
                       "n"_arg = static_cast<unsigned>(N),
                       "inl"_arg = inl,
                       "call"_arg = call,
                       "winner"_arg = winner);
        });
    }

    template<std::size_t N>
    void run_bytes_set() {
        const auto inl = bench_memset_bytes_inline<N>();
        const auto call = bench_memset_bytes_call<N>();
        const char* winner = (inl <= call) ? "inline" : "CALL";
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info,
                       "  {n:>3}  {inl:>6} {call:>6}  {winner}"_fmt,
                       "n"_arg = static_cast<unsigned>(N),
                       "inl"_arg = inl,
                       "call"_arg = call,
                       "winner"_arg = winner);
        });
    }

    template<std::size_t N>
    void run_align_cpy() {
        const auto gen = bench_memcpy_generic<N>();
        const auto aln = bench_memcpy_aligned<N>();
        const int save = (gen > aln) ? static_cast<int>((gen - aln) * 100 / gen)
                                     : -static_cast<int>((aln - gen) * 100 / aln);
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info,
                       "  {n:>3}  {gen:>6} {aln:>6}  {save:>3}%"_fmt,
                       "n"_arg = static_cast<unsigned>(N),
                       "gen"_arg = gen,
                       "aln"_arg = aln,
                       "save"_arg = save);
        });
    }

    template<std::size_t N>
    void run_align_set() {
        const auto gen = bench_memset_generic<N>();
        const auto aln = bench_memset_aligned<N>();
        const int save = (gen > aln) ? static_cast<int>((gen - aln) * 100 / gen)
                                     : -static_cast<int>((aln - gen) * 100 / aln);
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info,
                       "  {n:>3}  {gen:>6} {aln:>6}  {save:>3}%"_fmt,
                       "n"_arg = static_cast<unsigned>(N),
                       "gen"_arg = gen,
                       "aln"_arg = aln,
                       "save"_arg = save);
        });
    }

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== inline specialisation benchmarks (ROM/Thumb, -O3) ===");
        gba::benchmark::log(gba::log::level::info, "");
    });

    gba::benchmark::print_header("--- memcpy: inline bytes vs __aeabi_memcpy ---");
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info,
                   "  {n:>3}  {inl:>6} {call:>6}  {winner}"_fmt,
                   "n"_arg = "N",
                   "inl"_arg = "inline",
                   "call"_arg = "call",
                   "winner"_arg = "winner");
    });
    run_bytes_cpy<1>();
    run_bytes_cpy<2>();
    run_bytes_cpy<3>();
    run_bytes_cpy<4>();
    run_bytes_cpy<5>();
    run_bytes_cpy<6>();
    run_bytes_cpy<7>();
    run_bytes_cpy<8>();
    run_bytes_cpy<10>();
    run_bytes_cpy<12>();
    run_bytes_cpy<14>();
    run_bytes_cpy<16>();

    gba::benchmark::print_header("--- memset: inline bytes vs __aeabi_memset ---");
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info,
                   "  {n:>3}  {inl:>6} {call:>6}  {winner}"_fmt,
                   "n"_arg = "N",
                   "inl"_arg = "inline",
                   "call"_arg = "call",
                   "winner"_arg = "winner");
    });
    run_bytes_set<1>();
    run_bytes_set<2>();
    run_bytes_set<3>();
    run_bytes_set<4>();
    run_bytes_set<5>();
    run_bytes_set<6>();
    run_bytes_set<7>();
    run_bytes_set<8>();
    run_bytes_set<10>();
    run_bytes_set<12>();
    run_bytes_set<14>();
    run_bytes_set<16>();

    gba::benchmark::print_header("--- memcpy: __aeabi_memcpy vs __aeabi_memcpy4 ---");
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info,
                   "  {n:>3}  {gen:>6} {aln:>6}  {save:>5}"_fmt,
                   "n"_arg = "N",
                   "gen"_arg = "generic",
                   "aln"_arg = "align4",
                   "save"_arg = "save%");
    });
    run_align_cpy<4>();
    run_align_cpy<8>();
    run_align_cpy<12>();
    run_align_cpy<16>();
    run_align_cpy<20>();
    run_align_cpy<24>();
    run_align_cpy<28>();
    run_align_cpy<32>();
    run_align_cpy<48>();
    run_align_cpy<64>();
    run_align_cpy<96>();
    run_align_cpy<128>();

    gba::benchmark::print_header("--- memset: __aeabi_memset vs __aeabi_memset4 ---");
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info,
                   "  {n:>3}  {gen:>6} {aln:>6}  {save:>5}"_fmt,
                   "n"_arg = "N",
                   "gen"_arg = "generic",
                   "aln"_arg = "align4",
                   "save"_arg = "save%");
    });
    run_align_set<4>();
    run_align_set<8>();
    run_align_set<12>();
    run_align_set<16>();
    run_align_set<20>();
    run_align_set<24>();
    run_align_set<28>();
    run_align_set<32>();
    run_align_set<48>();
    run_align_set<64>();
    run_align_set<96>();
    run_align_set<128>();

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
