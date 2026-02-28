/// @file tests/memory/test_memcpy.cpp
/// @brief Unit tests for optimized memcpy implementation.
///
/// Tests every code path in memcpy.s:
/// - Byte early-out (n <= 3)
/// - Alignment check dispatch (word / halfword / byte)
/// - Word alignment fixup (all 4 offsets: 0, 1, 2, 3)
/// - Bulk 32-byte ldm/stm loop (exact, +1, +2, +3 remainder)
/// - Word loop (various counts)
/// - Joaobapt byte/half tail (tail = 0, 1, 2, 3)
/// - Halfword path (with/without byte fixup, with/without trailing byte)
/// - Byte fallback (unresolvable misalignment, various sizes)
/// - __aeabi_memcpy4 / __aeabi_memcpy8 direct entry
/// - C memcpy return value

#include <cstring>

#include <mgba_test.hpp>

// Prevent compiler from replacing memcpy calls with builtins
static void* (*volatile memcpy_fn)(void*, const void*, std::size_t) = &std::memcpy;

// Direct access to the AEABI entries for targeted testing
extern "C" {
    void __aeabi_memcpy(void*, const void*, std::size_t);
    void __aeabi_memcpy4(void*, const void*, std::size_t);
    void __aeabi_memcpy8(void*, const void*, std::size_t);
}

// Function pointers to prevent inlining / constant folding
static void (*volatile aeabi_memcpy_fn)(void*, const void*, std::size_t) = &__aeabi_memcpy;
static void (*volatile aeabi_memcpy4_fn)(void*, const void*, std::size_t) = &__aeabi_memcpy4;
static void (*volatile aeabi_memcpy8_fn)(void*, const void*, std::size_t) = &__aeabi_memcpy8;

namespace {

alignas(8) unsigned char src[512];
alignas(8) unsigned char dst[512];

void fill_src() {
    for (std::size_t i = 0; i < sizeof(src); ++i) {
        src[i] = static_cast<unsigned char>(i);
    }
}

void clear_dst() {
    for (auto& b : dst) b = 0xFF;
}

// Verify that dst[od..od+n) == src[os..os+n) and sentinel bytes are untouched
void verify(std::size_t od, std::size_t os, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        ASSERT_EQ(dst[od + i], src[os + i]);
    }
    if (od > 0) {
        ASSERT_EQ(dst[od - 1], static_cast<unsigned char>(0xFF));
    }
    if (od + n < sizeof(dst)) {
        ASSERT_EQ(dst[od + n], static_cast<unsigned char>(0xFF));
    }
}

void do_copy(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memcpy_fn(d, s, n); });
}

void do_copy4(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memcpy4_fn(d, s, n); });
}

void do_copy8(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memcpy8_fn(d, s, n); });
}

} // namespace

int main() {
    fill_src();

    // Byte early-out path (n <= 3)
    // Exercises: cmp r2, #3; ble .Lcopy_bytes

    // n=0: no bytes copied
    {
        clear_dst();
        do_copy(dst, src, 0);
        ASSERT_EQ(dst[0], static_cast<unsigned char>(0xFF));
    }

    // n=1
    {
        clear_dst();
        do_copy(dst, src, 1);
        ASSERT_EQ(dst[0], static_cast<unsigned char>(0));
        ASSERT_EQ(dst[1], static_cast<unsigned char>(0xFF));
    }

    // n=2
    {
        clear_dst();
        do_copy(dst, src, 2);
        verify(0, 0, 2);
    }

    // n=3
    {
        clear_dst();
        do_copy(dst, src, 3);
        verify(0, 0, 3);
    }

    // Word-promotable path, already word-aligned (fixup offset = 0)
    // Exercises: alignment check -> rsbs/movs (no fixup) -> word/bulk

    // n=4: single word via word loop (the key GCC implicit copy size)
    {
        clear_dst();
        do_copy(dst, src, 4);
        verify(0, 0, 4);
    }

    // n=5: 1 word + tail=1
    {
        clear_dst();
        do_copy(dst, src, 5);
        verify(0, 0, 5);
    }

    // n=6: 1 word + tail=2
    {
        clear_dst();
        do_copy(dst, src, 6);
        verify(0, 0, 6);
    }

    // n=7: 1 word + tail=3
    {
        clear_dst();
        do_copy(dst, src, 7);
        verify(0, 0, 7);
    }

    // n=8: 2 words exactly (common GCC struct copy)
    {
        clear_dst();
        do_copy(dst, src, 8);
        verify(0, 0, 8);
    }

    // n=12: 3 words (common GCC struct copy)
    {
        clear_dst();
        do_copy(dst, src, 12);
        verify(0, 0, 12);
    }

    // n=16: 4 words (common GCC struct copy)
    {
        clear_dst();
        do_copy(dst, src, 16);
        verify(0, 0, 16);
    }

    // n=20: 5 words (GCC struct copy)
    {
        clear_dst();
        do_copy(dst, src, 20);
        verify(0, 0, 20);
    }

    // n=24: 6 words
    {
        clear_dst();
        do_copy(dst, src, 24);
        verify(0, 0, 24);
    }

    // n=28: 7 words (just under bulk threshold)
    {
        clear_dst();
        do_copy(dst, src, 28);
        verify(0, 0, 28);
    }

    // Bulk path (n >= 32), all joaobapt tail variants
    // Exercises: push/ldmia/stmia/pop loop + word tail + byte/half tail

    // 32 bytes: 1 bulk iteration, tail=0 (exact, bxeq lr after pop)
    {
        clear_dst();
        do_copy(dst, src, 32);
        verify(0, 0, 32);
    }

    // 33 bytes: 1 bulk + tail=1 (byte)
    {
        clear_dst();
        do_copy(dst, src, 33);
        verify(0, 0, 33);
    }

    // 34 bytes: 1 bulk + tail=2 (halfword)
    {
        clear_dst();
        do_copy(dst, src, 34);
        verify(0, 0, 34);
    }

    // 35 bytes: 1 bulk + tail=3 (halfword + byte)
    {
        clear_dst();
        do_copy(dst, src, 35);
        verify(0, 0, 35);
    }

    // 36 bytes: 1 bulk + 1 word, tail=0
    {
        clear_dst();
        do_copy(dst, src, 36);
        verify(0, 0, 36);
    }

    // 64 bytes: 2 bulk iterations, tail=0
    {
        clear_dst();
        do_copy(dst, src, 64);
        verify(0, 0, 64);
    }

    // 96 bytes: 3 bulk iterations, tail=0
    {
        clear_dst();
        do_copy(dst, src, 96);
        verify(0, 0, 96);
    }

    // 128 bytes: 4 bulk iterations, tail=0
    {
        clear_dst();
        do_copy(dst, src, 128);
        verify(0, 0, 128);
    }

    // 256 bytes: 8 bulk iterations
    {
        clear_dst();
        do_copy(dst, src, 256);
        verify(0, 0, 256);
    }

    // Word alignment fixup: all 4 offsets
    // Exercises: rsbs r3, r0, #4; movs r3, r3, lsl #31 with each bit combo

    // offset=1 (need fixup: 1 byte mi + 2 bytes cs = 3 bytes)
    {
        clear_dst();
        do_copy(dst + 1, src + 1, 36);
        verify(1, 1, 36);
    }

    // offset=2 (need fixup: 0 bytes mi + 2 bytes cs = 2 bytes)
    {
        clear_dst();
        do_copy(dst + 2, src + 2, 36);
        verify(2, 2, 36);
    }

    // offset=3 (need fixup: 1 byte mi + 0 bytes cs = 1 byte)
    {
        clear_dst();
        do_copy(dst + 3, src + 3, 36);
        verify(3, 3, 36);
    }

    // offset=1, small n=4 (fixup 3 bytes, then 1 byte tail)
    {
        clear_dst();
        do_copy(dst + 1, src + 1, 4);
        verify(1, 1, 4);
    }

    // offset=2, small n=4 (fixup 2 bytes, then 2 byte tail)
    {
        clear_dst();
        do_copy(dst + 2, src + 2, 4);
        verify(2, 2, 4);
    }

    // offset=3, small n=4 (fixup 1 byte, then 3 byte tail)
    {
        clear_dst();
        do_copy(dst + 3, src + 3, 4);
        verify(3, 3, 4);
    }

    // offset=1, large n crossing bulk threshold
    {
        clear_dst();
        do_copy(dst + 1, src + 1, 128);
        verify(1, 1, 128);
    }

    // offset=3, large n crossing bulk threshold
    {
        clear_dst();
        do_copy(dst + 3, src + 3, 100);
        verify(3, 3, 100);
    }

    // Halfword path (XOR bit 1 set, bit 0 clear)
    // Exercises: .Lcopy_halves_entry -> halfword loop -> trailing byte

    // dst word-aligned, src+2: no byte fixup at entry, even n (no trailing)
    {
        clear_dst();
        do_copy(dst, src + 2, 20);
        verify(0, 2, 20);
    }

    // dst word-aligned, src+2: no byte fixup, odd n (trailing byte)
    {
        clear_dst();
        do_copy(dst, src + 2, 21);
        verify(0, 2, 21);
    }

    // dst+2, src word-aligned: no byte fixup, even n
    {
        clear_dst();
        do_copy(dst + 2, src, 20);
        verify(2, 0, 20);
    }

    // dst+2, src word-aligned: no byte fixup, odd n (trailing byte)
    {
        clear_dst();
        do_copy(dst + 2, src, 21);
        verify(2, 0, 21);
    }

    // Both pointers odd: byte fixup at entry (tst r0, #1 -> ne)
    // dst+1, src+3: XOR = 2 (bit 1 set, bit 0 clear) -> halfword path
    // tst r0 #1 -> bit 0 of (dst+1) is set -> byte fixup
    {
        clear_dst();
        do_copy(dst + 1, src + 3, 20);
        verify(1, 3, 20);
    }

    // Both pointers odd, odd n (trailing byte after halfword loop)
    {
        clear_dst();
        do_copy(dst + 1, src + 3, 21);
        verify(1, 3, 21);
    }

    // Both pointers odd, small n=4
    {
        clear_dst();
        do_copy(dst + 1, src + 3, 4);
        verify(1, 3, 4);
    }

    // Both pointers odd, n=5
    {
        clear_dst();
        do_copy(dst + 3, src + 1, 5);
        verify(3, 1, 5);
    }

    // Halfword path with large n
    {
        clear_dst();
        do_copy(dst, src + 2, 128);
        verify(0, 2, 128);
    }

    // Byte fallback (XOR bit 0 set = unresolvable misalignment)
    // Exercises: alignment check -> .Lcopy_bytes for n > 3

    // dst+1, src+0: XOR bit 0 set, n=4 (just above byte early-out)
    {
        clear_dst();
        do_copy(dst + 1, src, 4);
        verify(1, 0, 4);
    }

    // dst+0, src+1: XOR bit 0 set, n=5
    {
        clear_dst();
        do_copy(dst, src + 1, 5);
        verify(0, 1, 5);
    }

    // dst+1, src+0: XOR bit 0 set, n=6
    {
        clear_dst();
        do_copy(dst + 1, src, 6);
        verify(1, 0, 6);
    }

    // dst+0, src+1: XOR bit 0 set, n=16
    {
        clear_dst();
        do_copy(dst, src + 1, 16);
        verify(0, 1, 16);
    }

    // dst+1, src+0: large byte copy
    {
        clear_dst();
        do_copy(dst + 1, src, 64);
        verify(1, 0, 64);
    }

    // dst+3, src+0: XOR bits 0 and 1 both set (still byte path)
    {
        clear_dst();
        do_copy(dst + 3, src, 16);
        verify(3, 0, 16);
    }

    // __aeabi_memcpy4 direct entry
    // Exercises: word-aligned fast path, no alignment preamble

    // 0 bytes (edge: cmp #32 blt, cmp #4 blt, tail movs with r2=0)
    {
        clear_dst();
        do_copy4(dst, src, 0);
        ASSERT_EQ(dst[0], static_cast<unsigned char>(0xFF));
    }

    // 4 bytes
    {
        clear_dst();
        do_copy4(dst, src, 4);
        verify(0, 0, 4);
    }

    // 8 bytes
    {
        clear_dst();
        do_copy4(dst, src, 8);
        verify(0, 0, 8);
    }

    // 12 bytes
    {
        clear_dst();
        do_copy4(dst, src, 12);
        verify(0, 0, 12);
    }

    // 16 bytes
    {
        clear_dst();
        do_copy4(dst, src, 16);
        verify(0, 0, 16);
    }

    // 32 bytes (hits bulk)
    {
        clear_dst();
        do_copy4(dst, src, 32);
        verify(0, 0, 32);
    }

    // 33 bytes (bulk + 1 byte tail)
    {
        clear_dst();
        do_copy4(dst, src, 33);
        verify(0, 0, 33);
    }

    // 34 bytes (bulk + 2 byte tail)
    {
        clear_dst();
        do_copy4(dst, src, 34);
        verify(0, 0, 34);
    }

    // 35 bytes (bulk + 3 byte tail)
    {
        clear_dst();
        do_copy4(dst, src, 35);
        verify(0, 0, 35);
    }

    // 64 bytes
    {
        clear_dst();
        do_copy4(dst, src, 64);
        verify(0, 0, 64);
    }

    // 128 bytes
    {
        clear_dst();
        do_copy4(dst, src, 128);
        verify(0, 0, 128);
    }

    // __aeabi_memcpy8 direct entry

    // 8 bytes
    {
        clear_dst();
        do_copy8(dst, src, 8);
        verify(0, 0, 8);
    }

    // 64 bytes
    {
        clear_dst();
        do_copy8(dst, src, 64);
        verify(0, 0, 64);
    }

    // 37 bytes (bulk + 1 word + tail=1)
    {
        clear_dst();
        do_copy8(dst, src, 37);
        verify(0, 0, 37);
    }

    // C memcpy return value

    {
        clear_dst();
        auto result = memcpy_fn(dst, src, 10);
        ASSERT_TRUE(result == dst);
        verify(0, 0, 10);
    }

    {
        clear_dst();
        auto result = memcpy_fn(dst + 4, src, 64);
        ASSERT_TRUE(result == dst + 4);
        verify(4, 0, 64);
    }

    // Data pattern stress: all-zero and alternating source

    // All-zero source
    {
        for (auto& b : src) b = 0;
        clear_dst();
        do_copy(dst, src, 64);
        for (std::size_t i = 0; i < 64; ++i) {
            ASSERT_EQ(dst[i], static_cast<unsigned char>(0));
        }
        fill_src();
    }

    // Alternating pattern (catches endianness / byte-lane bugs)
    {
        for (std::size_t i = 0; i < 32; ++i) {
            src[i] = static_cast<unsigned char>(0xAA + i);
        }
        clear_dst();
        do_copy(dst, src, 32);
        for (std::size_t i = 0; i < 32; ++i) {
            ASSERT_EQ(dst[i], static_cast<unsigned char>(0xAA + i));
        }
        fill_src();
    }

    // Adjacent non-overlapping buffers

    {
        fill_src();
        clear_dst();
        do_copy(dst, src, 64);
        do_copy(dst + 64, dst, 64);
        for (std::size_t i = 0; i < 64; ++i) {
            ASSERT_EQ(dst[64 + i], dst[i]);
        }
    }

    // Sweep: every size 0..64, word-aligned
    // Catches off-by-one across all path transitions (3->4, 31->32)

    for (std::size_t n = 0; n <= 64; n = test::do_not_optimize([&] { return n + 1; })) {
        clear_dst();
        do_copy(dst, src, n);
        for (std::size_t i = 0; i < n; ++i) {
            ASSERT_EQ(dst[i], src[i]);
        }
        if (n < sizeof(dst)) {
            ASSERT_EQ(dst[n], static_cast<unsigned char>(0xFF));
        }
    }

    // Sweep: every size 0..64, offset +1 (word fixup path)

    for (std::size_t n = 0; n <= 64; n = test::do_not_optimize([&] { return n + 1; })) {
        clear_dst();
        do_copy(dst + 1, src + 1, n);
        for (std::size_t i = 0; i < n; ++i) {
            ASSERT_EQ(dst[1 + i], src[1 + i]);
        }
        ASSERT_EQ(dst[0], static_cast<unsigned char>(0xFF));
        if (1 + n < sizeof(dst)) {
            ASSERT_EQ(dst[1 + n], static_cast<unsigned char>(0xFF));
        }
    }

    // Sweep: every size 0..64, odd misalignment (byte path for n > 3)

    for (std::size_t n = 0; n <= 64; n = test::do_not_optimize([&] { return n + 1; })) {
        clear_dst();
        do_copy(dst + 1, src, n);
        for (std::size_t i = 0; i < n; ++i) {
            ASSERT_EQ(dst[1 + i], src[i]);
        }
        ASSERT_EQ(dst[0], static_cast<unsigned char>(0xFF));
        if (1 + n < sizeof(dst)) {
            ASSERT_EQ(dst[1 + n], static_cast<unsigned char>(0xFF));
        }
    }

    test::exit(test::failures());
    __builtin_unreachable();
}
