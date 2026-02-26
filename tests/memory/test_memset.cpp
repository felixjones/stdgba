/**
 * @file tests/memory/test_memset.cpp
 * @brief Unit tests for optimized memset implementation.
 *
 * Tests every code path in memset.s:
 *   - Byte early-out (n <= 3)
 *   - Alignment fixup (all 4 offsets: 0, 1, 2, 3)
 *   - Bulk 32-byte stm loop (exact, +1, +2, +3 remainder)
 *   - Word loop via computed jump (various counts)
 *   - Joaobapt byte/half tail (tail = 0, 1, 2, 3)
 *   - Byte fallback (small sizes)
 *   - __aeabi_memset4 / __aeabi_memset8 direct entry
 *   - __aeabi_memclr / __aeabi_memclr4 / __aeabi_memclr8 direct entry
 *   - C memset return value
 *   - Various fill byte values (0x00, 0xFF, 0xAB, 0x01)
 */

#include <cstdint>
#include <cstring>

#include "../mgba_test.hpp"

// Prevent compiler from replacing memset calls with builtins
static void* (*volatile memset_fn)(void*, int, std::size_t) = &std::memset;

// Direct access to the AEABI entries for targeted testing
extern "C" {
    void __aeabi_memset(void*, std::size_t, int);
    void __aeabi_memset4(void*, std::size_t, int);
    void __aeabi_memset8(void*, std::size_t, int);
    void __aeabi_memclr(void*, std::size_t);
    void __aeabi_memclr4(void*, std::size_t);
    void __aeabi_memclr8(void*, std::size_t);
}

// Function pointers to prevent inlining / constant folding
static void (*volatile aeabi_memset_fn)(void*, std::size_t, int) = &__aeabi_memset;
static void (*volatile aeabi_memset4_fn)(void*, std::size_t, int) = &__aeabi_memset4;
static void (*volatile aeabi_memset8_fn)(void*, std::size_t, int) = &__aeabi_memset8;
static void (*volatile aeabi_memclr_fn)(void*, std::size_t) = &__aeabi_memclr;
static void (*volatile aeabi_memclr4_fn)(void*, std::size_t) = &__aeabi_memclr4;
static void (*volatile aeabi_memclr8_fn)(void*, std::size_t) = &__aeabi_memclr8;

namespace {

alignas(8) unsigned char buf[512];

constexpr unsigned char SENTINEL = 0xCD;

void clear_buf() {
    for (auto& b : buf) b = SENTINEL;
}

// Verify that buf[off..off+n) == fill and sentinels are untouched
void verify(std::size_t off, std::size_t n, unsigned char fill) {
    for (std::size_t i = 0; i < n; ++i) {
        ASSERT_EQ(buf[off + i], fill);
    }
    if (off > 0) {
        ASSERT_EQ(buf[off - 1], SENTINEL);
    }
    if (off + n < sizeof(buf)) {
        ASSERT_EQ(buf[off + n], SENTINEL);
    }
}

void do_set(void* d, std::size_t n, int c) {
    test::do_not_optimize([&] { aeabi_memset_fn(d, n, c); });
}

void do_set4(void* d, std::size_t n, int c) {
    test::do_not_optimize([&] { aeabi_memset4_fn(d, n, c); });
}

void do_set8(void* d, std::size_t n, int c) {
    test::do_not_optimize([&] { aeabi_memset8_fn(d, n, c); });
}

void do_clr(void* d, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memclr_fn(d, n); });
}

void do_clr4(void* d, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memclr4_fn(d, n); });
}

void do_clr8(void* d, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memclr8_fn(d, n); });
}

} // namespace

int main() {
    // =========================================================================
    // Byte early-out path (n <= 3)
    // Exercises: cmp r1, #3; ble .Lfill_bytes
    // =========================================================================

    // n=0: no bytes set
    {
        clear_buf();
        do_set(buf, 0, 0xAB);
        ASSERT_EQ(buf[0], SENTINEL);
    }

    // n=1: single byte
    {
        clear_buf();
        do_set(buf, 1, 0xAB);
        ASSERT_EQ(buf[0], static_cast<unsigned char>(0xAB));
        ASSERT_EQ(buf[1], SENTINEL);
    }

    // n=2: two bytes
    {
        clear_buf();
        do_set(buf, 2, 0xAB);
        verify(0, 2, 0xAB);
    }

    // n=3: three bytes
    {
        clear_buf();
        do_set(buf, 3, 0xAB);
        verify(0, 3, 0xAB);
    }

    // =========================================================================
    // Word-promotable path, already word-aligned (fixup offset = 0)
    // =========================================================================

    // n=4: single word via word loop
    {
        clear_buf();
        do_set(buf, 4, 0xAB);
        verify(0, 4, 0xAB);
    }

    // n=5: 1 word + tail=1
    {
        clear_buf();
        do_set(buf, 5, 0xAB);
        verify(0, 5, 0xAB);
    }

    // n=6: 1 word + tail=2
    {
        clear_buf();
        do_set(buf, 6, 0xAB);
        verify(0, 6, 0xAB);
    }

    // n=7: 1 word + tail=3
    {
        clear_buf();
        do_set(buf, 7, 0xAB);
        verify(0, 7, 0xAB);
    }

    // n=8: 2 words exactly
    {
        clear_buf();
        do_set(buf, 8, 0xAB);
        verify(0, 8, 0xAB);
    }

    // n=12: 3 words exactly
    {
        clear_buf();
        do_set(buf, 12, 0xAB);
        verify(0, 12, 0xAB);
    }

    // n=16: 4 words exactly
    {
        clear_buf();
        do_set(buf, 16, 0xAB);
        verify(0, 16, 0xAB);
    }

    // n=20: 5 words exactly
    {
        clear_buf();
        do_set(buf, 20, 0xAB);
        verify(0, 20, 0xAB);
    }

    // n=24: 6 words exactly
    {
        clear_buf();
        do_set(buf, 24, 0xAB);
        verify(0, 24, 0xAB);
    }

    // n=28: 7 words exactly
    {
        clear_buf();
        do_set(buf, 28, 0xAB);
        verify(0, 28, 0xAB);
    }

    // =========================================================================
    // Bulk stm loop (n >= 32)
    // =========================================================================

    // n=32: exactly one bulk block
    {
        clear_buf();
        do_set(buf, 32, 0xAB);
        verify(0, 32, 0xAB);
    }

    // n=33: bulk + 1 tail word + 1 tail byte
    {
        clear_buf();
        do_set(buf, 33, 0xAB);
        verify(0, 33, 0xAB);
    }

    // n=34: bulk + tail=2
    {
        clear_buf();
        do_set(buf, 34, 0xAB);
        verify(0, 34, 0xAB);
    }

    // n=35: bulk + tail=3
    {
        clear_buf();
        do_set(buf, 35, 0xAB);
        verify(0, 35, 0xAB);
    }

    // n=36: bulk + 1 word exactly
    {
        clear_buf();
        do_set(buf, 36, 0xAB);
        verify(0, 36, 0xAB);
    }

    // n=60: bulk + 7 words
    {
        clear_buf();
        do_set(buf, 60, 0xAB);
        verify(0, 60, 0xAB);
    }

    // n=64: exactly two bulk blocks
    {
        clear_buf();
        do_set(buf, 64, 0xAB);
        verify(0, 64, 0xAB);
    }

    // n=96: three bulk blocks
    {
        clear_buf();
        do_set(buf, 96, 0xAB);
        verify(0, 96, 0xAB);
    }

    // n=128: four bulk blocks
    {
        clear_buf();
        do_set(buf, 128, 0xAB);
        verify(0, 128, 0xAB);
    }

    // n=255: large odd size
    {
        clear_buf();
        do_set(buf, 255, 0xAB);
        verify(0, 255, 0xAB);
    }

    // n=256: large even power of 2
    {
        clear_buf();
        do_set(buf, 256, 0xAB);
        verify(0, 256, 0xAB);
    }

    // n=512: full buffer
    {
        clear_buf();
        do_set(buf, 512, 0xAB);
        verify(0, 512, 0xAB);
    }

    // =========================================================================
    // Alignment fixup path: dest not word-aligned
    // =========================================================================

    // offset=1: fixup 1 byte, then word-aligned
    {
        clear_buf();
        do_set(buf + 1, 7, 0xAB);
        verify(1, 7, 0xAB);
    }

    // offset=2: fixup 2 bytes (halfword), then word-aligned
    {
        clear_buf();
        do_set(buf + 2, 6, 0xAB);
        verify(2, 6, 0xAB);
    }

    // offset=3: fixup 1 byte, then word-aligned
    {
        clear_buf();
        do_set(buf + 3, 5, 0xAB);
        verify(3, 5, 0xAB);
    }

    // offset=1 with bulk size
    {
        clear_buf();
        do_set(buf + 1, 63, 0xAB);
        verify(1, 63, 0xAB);
    }

    // offset=1 with large size
    {
        clear_buf();
        do_set(buf + 1, 127, 0xAB);
        verify(1, 127, 0xAB);
    }

    // offset=3 with bulk size
    {
        clear_buf();
        do_set(buf + 3, 65, 0xAB);
        verify(3, 65, 0xAB);
    }

    // =========================================================================
    // __aeabi_memset4 / __aeabi_memset8 direct entry
    // =========================================================================

    // memset4: small
    {
        clear_buf();
        do_set4(buf, 4, 0xAB);
        verify(0, 4, 0xAB);
    }

    // memset4: medium
    {
        clear_buf();
        do_set4(buf, 28, 0xAB);
        verify(0, 28, 0xAB);
    }

    // memset4: bulk
    {
        clear_buf();
        do_set4(buf, 64, 0xAB);
        verify(0, 64, 0xAB);
    }

    // memset4: bulk + tail
    {
        clear_buf();
        do_set4(buf, 100, 0xAB);
        verify(0, 100, 0xAB);
    }

    // memset8: bulk
    {
        clear_buf();
        do_set8(buf, 64, 0xAB);
        verify(0, 64, 0xAB);
    }

    // memset8: large
    {
        clear_buf();
        do_set8(buf, 256, 0xAB);
        verify(0, 256, 0xAB);
    }

    // =========================================================================
    // __aeabi_memclr / __aeabi_memclr4 / __aeabi_memclr8
    // =========================================================================

    // memclr: small
    {
        clear_buf();
        do_clr(buf, 4);
        verify(0, 4, 0x00);
    }

    // memclr: medium
    {
        clear_buf();
        do_clr(buf, 20);
        verify(0, 20, 0x00);
    }

    // memclr: large
    {
        clear_buf();
        do_clr(buf, 128);
        verify(0, 128, 0x00);
    }

    // memclr: unaligned
    {
        clear_buf();
        do_clr(buf + 1, 31);
        verify(1, 31, 0x00);
    }

    // memclr4: bulk
    {
        clear_buf();
        do_clr4(buf, 64);
        verify(0, 64, 0x00);
    }

    // memclr8: bulk
    {
        clear_buf();
        do_clr8(buf, 128);
        verify(0, 128, 0x00);
    }

    // =========================================================================
    // Various fill byte values
    // =========================================================================

    // 0x00 (zero fill)
    {
        clear_buf();
        do_set(buf, 32, 0x00);
        verify(0, 32, 0x00);
    }

    // 0xFF (all ones)
    {
        clear_buf();
        do_set(buf, 32, 0xFF);
        verify(0, 32, 0xFF);
    }

    // 0x01 (low bit only)
    {
        clear_buf();
        do_set(buf, 32, 0x01);
        verify(0, 32, 0x01);
    }

    // 0x80 (high bit only)
    {
        clear_buf();
        do_set(buf, 32, 0x80);
        verify(0, 32, 0x80);
    }

    // 0x55 (alternating bits)
    {
        clear_buf();
        do_set(buf, 32, 0x55);
        verify(0, 32, 0x55);
    }

    // 0xAA (alternating bits, inverted)
    {
        clear_buf();
        do_set(buf, 32, 0xAA);
        verify(0, 32, 0xAA);
    }

    // Fill value > 0xFF (AEABI specifies lowest byte only)
    {
        clear_buf();
        do_set(buf, 8, 0x1AB);
        verify(0, 8, 0xAB);
    }

    // =========================================================================
    // C memset wrapper: return value
    // =========================================================================

    // memset must return dest
    {
        clear_buf();
        void* ret = memset_fn(buf, 0xAB, 16);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(ret), reinterpret_cast<std::uintptr_t>(buf));
        verify(0, 16, 0xAB);
    }

    // memset with n=0 returns dest
    {
        clear_buf();
        void* ret = memset_fn(buf, 0xAB, 0);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(ret), reinterpret_cast<std::uintptr_t>(buf));
        ASSERT_EQ(buf[0], SENTINEL);
    }

    // =========================================================================
    // Sweep: exhaustive sizes 0..68 for each alignment offset 0..3
    // This catches off-by-one errors in every code path.
    // =========================================================================
    for (std::size_t off = 0; off < 4; ++off) {
        for (std::size_t n = 0; n <= 68; ++n) {
            if (off + n > sizeof(buf)) break;
            clear_buf();
            do_set(buf + off, n, 0xBE);
            for (std::size_t i = 0; i < n; ++i) {
                ASSERT_EQ(buf[off + i], static_cast<unsigned char>(0xBE));
            }
            if (off > 0) {
                ASSERT_EQ(buf[off - 1], SENTINEL);
            }
            if (off + n < sizeof(buf)) {
                ASSERT_EQ(buf[off + n], SENTINEL);
            }
        }
    }

    // =========================================================================
    // Sweep: memset4 for sizes 0..68 (word-aligned dest)
    // =========================================================================
    for (std::size_t n = 0; n <= 68; ++n) {
        clear_buf();
        do_set4(buf, n, 0xBE);
        for (std::size_t i = 0; i < n; ++i) {
            ASSERT_EQ(buf[i], static_cast<unsigned char>(0xBE));
        }
        if (n < sizeof(buf)) {
            ASSERT_EQ(buf[n], SENTINEL);
        }
    }

    test::finalize();
    __builtin_unreachable();
}
