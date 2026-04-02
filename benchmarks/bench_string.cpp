/// @file benchmarks/bench_string.cpp
/// @brief Benchmarks for ROM Thumb string functions vs newlib.
///
/// Uses volatile function pointers to prevent inlining and ensure we
/// measure the actual assembly implementation.

#include <cstdint>
#include <cstring>

#include <gba/benchmark>


using namespace gba::literals;

// Our implementations (override newlib at link time)
static std::size_t (*volatile stdgba_strlen)(const char*) = &std::strlen;
static int (*volatile stdgba_strcmp)(const char*, const char*) = &std::strcmp;
static int (*volatile stdgba_strncmp)(const char*, const char*, std::size_t) = &std::strncmp;
static void* (*volatile stdgba_memchr)(const void*, int, std::size_t) = &std::memchr;

extern "C" {
char* strchr(const char*, int);
char* strrchr(const char*, int);
std::size_t strnlen(const char*, std::size_t);
}

static char* (*volatile stdgba_strchr)(const char*, int) = &strchr;
static char* (*volatile stdgba_strrchr)(const char*, int) = &strrchr;
static std::size_t (*volatile stdgba_strnlen)(const char*, std::size_t) = &strnlen;

namespace {

    // Buffers in EWRAM (ROM functions read from here)
    alignas(4) char str1[8192];
    alignas(4) char str2[8192];
    alignas(4) unsigned char membuf[8192];

    void fill_string(char* s, int len) {
        for (int i = 0; i < len; ++i) {
            s[i] = static_cast<char>('A' + (i % 26));
        }
        s[len] = '\0';
    }

    void fill_same(int len) {
        fill_string(str1, len);
        for (int i = 0; i <= len; ++i) str2[i] = str1[i];
    }

} // namespace

int main() {
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "=== string function benchmarks (ROM/Thumb) ==="); });

    // strlen benchmark
    gba::benchmark::print_header("--- strlen (cycles) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  Length     cycles"); });

    for (int len : {0, 1, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 128, 256, 512, 1024, 4096}) {
        fill_string(str1, len);
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strlen, static_cast<const char*>(str1));
        gba::benchmark::with_logger(
            [&] { gba::benchmark::log(gba::log::level::info, "  {len:>5}    {cycles:>6}"_fmt, "len"_arg = len, "cycles"_arg = cycles); });
    }

    // strlen at various alignments
    gba::benchmark::print_header("--- strlen alignment (len=64) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  Offset    cycles"); });
    for (int off = 0; off < 4; ++off) {
        fill_string(str1 + off, 64);
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strlen, static_cast<const char*>(str1 + off));
        gba::benchmark::with_logger(
            [&] { gba::benchmark::log(gba::log::level::info, "  {off}         {cycles}"_fmt, "off"_arg = off, "cycles"_arg = cycles); });
    }

    // strnlen benchmark
    gba::benchmark::print_header("--- strnlen (len=256) ---");
    fill_string(str1, 256);
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  maxlen    cycles"); });
    for (int maxlen : {0, 1, 16, 64, 128, 256, 512}) {
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strnlen, static_cast<const char*>(str1),
                                                 static_cast<std::size_t>(maxlen));
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {maxlen:>5}    {cycles:>6}"_fmt, "maxlen"_arg = maxlen,
                       "cycles"_arg = cycles);
        });
    }

    // strcmp benchmark (equal strings)
    gba::benchmark::print_header("--- strcmp equal (cycles) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  Length     cycles"); });
    for (int len : {0, 1, 4, 8, 16, 32, 64, 128, 256, 1024}) {
        fill_same(len);
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strcmp, static_cast<const char*>(str1),
                                                 static_cast<const char*>(str2));
        gba::benchmark::with_logger(
            [&] { gba::benchmark::log(gba::log::level::info, "  {len:>5}    {cycles:>6}"_fmt, "len"_arg = len, "cycles"_arg = cycles); });
    }

    // strcmp diff at position 0 vs end
    gba::benchmark::print_header("--- strcmp diff position (len=64) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  DiffPos   cycles"); });
    for (int pos : {0, 1, 3, 4, 7, 15, 31, 63}) {
        fill_same(64);
        str2[pos] = str1[pos] + 1;
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strcmp, static_cast<const char*>(str1),
                                                 static_cast<const char*>(str2));
        gba::benchmark::with_logger(
            [&] { gba::benchmark::log(gba::log::level::info, "  {pos:>5}    {cycles:>6}"_fmt, "pos"_arg = pos, "cycles"_arg = cycles); });
    }

    // strncmp benchmark
    gba::benchmark::print_header("--- strncmp equal (n=len, cycles) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  Length     cycles"); });
    for (int len : {1, 4, 16, 64, 256}) {
        fill_same(len);
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strncmp, static_cast<const char*>(str1),
                                                 static_cast<const char*>(str2), static_cast<std::size_t>(len));
        gba::benchmark::with_logger(
            [&] { gba::benchmark::log(gba::log::level::info, "  {len:>5}    {cycles:>6}"_fmt, "len"_arg = len, "cycles"_arg = cycles); });
    }

    // memchr benchmark
    gba::benchmark::print_header("--- memchr (cycles) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  Size   @pos     cycles"); });
    for (int size : {16, 64, 256, 1024, 4096}) {
        for (int i = 0; i < size; ++i) membuf[i] = 0xAA;
        // Target at end
        membuf[size - 1] = 0x42;
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_memchr, static_cast<const void*>(membuf), 0x42,
                                                 static_cast<std::size_t>(size));
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {size:>4}   end     {cycles:>6}"_fmt, "size"_arg = size,
                       "cycles"_arg = cycles);
        });
        membuf[size - 1] = 0xAA;

        // Not found
        unsigned int cycles_nf = gba::benchmark::measure_avg(4, stdgba_memchr, static_cast<const void*>(membuf), 0x42,
                                                    static_cast<std::size_t>(size));
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {size:>4}   n/a     {cycles:>6}"_fmt, "size"_arg = size,
                       "cycles"_arg = cycles_nf);
        });
    }

    // strchr benchmark
    gba::benchmark::print_header("--- strchr (cycles) ---");
    gba::benchmark::with_logger([&] { gba::benchmark::log(gba::log::level::info, "  StrLen @pos     cycles"); });
    for (int len : {16, 64, 256, 1024}) {
        fill_string(str1, len);
        // Place unique char only at end (0x01 doesn't appear in A-Z pattern)
        str1[len - 1] = 0x01;
        unsigned int cycles = gba::benchmark::measure_avg(4, stdgba_strchr, static_cast<const char*>(str1), 0x01);
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {len:>4}   end     {cycles:>6}"_fmt, "len"_arg = len,
                       "cycles"_arg = cycles);
        });
        str1[len - 1] = 'Z'; // restore
        // Not found
        unsigned int cycles_nf = gba::benchmark::measure_avg(4, stdgba_strchr, static_cast<const char*>(str1), '~');
        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {len:>4}   n/a     {cycles:>6}"_fmt, "len"_arg = len,
                       "cycles"_arg = cycles_nf);
        });
    }

    gba::benchmark::with_logger([&] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    return 0;
}
