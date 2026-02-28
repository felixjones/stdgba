/// @file tests/memory/test_memcmp.cpp
/// @brief Unit tests for optimized memcmp and bcmp implementations.

#include <cstring>

#include <mgba_test.hpp>

extern "C" {
    int memcmp(const void*, const void*, std::size_t);
    int bcmp(const void*, const void*, std::size_t);
    int __stdgba_memcmp(const void*, const void*, std::size_t);
    int __stdgba_bcmp(const void*, const void*, std::size_t);
}

// Use assembly entry points directly via volatile to prevent inlining.
static int (*volatile memcmp_fn)(const void*, const void*, std::size_t) = &__stdgba_memcmp;
static int (*volatile bcmp_fn)(const void*, const void*, std::size_t) = &__stdgba_bcmp;

namespace {

alignas(8) unsigned char a[512];
alignas(8) unsigned char b[512];

void fill_equal() {
    for (std::size_t i = 0; i < sizeof(a); ++i) {
        a[i] = static_cast<unsigned char>(i);
        b[i] = static_cast<unsigned char>(i);
    }
}

} // namespace

int main() {
    // Zero-length comparison
    fill_equal();
    ASSERT_EQ(memcmp_fn(a, b, 0), 0);
    ASSERT_EQ(bcmp_fn(a, b, 0), 0);

    // Equal buffers: various sizes
    for (std::size_t n : {1u, 2u, 3u, 4u, 5u, 7u, 8u, 12u, 15u, 16u,
                          28u, 31u, 32u, 33u, 48u, 63u, 64u, 65u,
                          128u, 255u, 256u, 512u}) {
        fill_equal();
        ASSERT_EQ(memcmp_fn(a, b, n), 0);
        ASSERT_EQ(bcmp_fn(a, b, n), 0);
    }

    // Diff at first byte
    for (std::size_t n : {1u, 2u, 3u, 4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        fill_equal();
        a[0] = 0x10;
        b[0] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, n) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, n) > 0);
        ASSERT_NZ(bcmp_fn(a, b, n));
    }

    // Diff at last byte
    for (std::size_t n : {1u, 2u, 3u, 4u, 8u, 16u, 32u, 64u, 65u, 128u, 256u}) {
        fill_equal();
        a[n - 1] = 0x10;
        b[n - 1] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, n) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, n) > 0);
        ASSERT_NZ(bcmp_fn(a, b, n));
    }

    // Diff at every position (word-aligned, n=32)
    for (std::size_t pos = 0; pos < 32; ++pos) {
        fill_equal();
        a[pos] = 0x10;
        b[pos] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, 32) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 32) > 0);
        ASSERT_NZ(bcmp_fn(a, b, 32));
    }

    // Diff in second word (positions 4-7)
    for (std::size_t pos = 4; pos < 8; ++pos) {
        fill_equal();
        a[pos] = 0x80;
        b[pos] = 0x01;
        ASSERT_TRUE(memcmp_fn(a, b, 8) > 0);
        ASSERT_TRUE(memcmp_fn(b, a, 8) < 0);
    }

    // Byte-level ordering: 0x00 < 0xFF (unsigned comparison)
    {
        fill_equal();
        a[0] = 0x00;
        b[0] = 0xFF;
        ASSERT_TRUE(memcmp_fn(a, b, 1) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 1) > 0);
    }

    // Unaligned pointers: offset 1
    for (std::size_t n : {1u, 2u, 3u, 4u, 7u, 8u, 15u, 16u, 31u, 32u, 63u}) {
        fill_equal();
        ASSERT_EQ(memcmp_fn(a + 1, b + 1, n), 0);
        ASSERT_EQ(bcmp_fn(a + 1, b + 1, n), 0);
    }

    // Unaligned with diff
    for (std::size_t off : {1u, 2u, 3u}) {
        fill_equal();
        a[off] = 0x10;
        b[off] = 0x20;
        ASSERT_TRUE(memcmp_fn(a + off, b + off, 4) < 0);
    }

    // Cross-alignment: one pointer odd, one even (byte path)
    {
        // Manually set 4 bytes equal at different alignments
        a[1] = 0xAA; a[2] = 0xBB; a[3] = 0xCC; a[4] = 0xDD;
        b[2] = 0xAA; b[3] = 0xBB; b[4] = 0xCC; b[5] = 0xDD;
        ASSERT_EQ(memcmp_fn(a + 1, b + 2, 4), 0);
    }

    // Diff at every position in bulk range (n=128)
    for (std::size_t pos = 0; pos < 128; pos += 13) {
        fill_equal();
        a[pos] = 0x10;
        b[pos] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, 128) < 0);
        ASSERT_NZ(bcmp_fn(a, b, 128));
    }

    // Diff at every position in double-pump boundary region (n=65)
    for (std::size_t pos = 60; pos < 65; ++pos) {
        fill_equal();
        a[pos] = 0x10;
        b[pos] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, 65) < 0);
        ASSERT_NZ(bcmp_fn(a, b, 65));
    }

    // Diff at every byte position in a 64-byte buffer (exercises all 4
    // half-blocks of the double-pumped bulk: first_hi, first_lo, second_hi,
    // second_lo). This stress-tests the register-based bulk diff resolver.
    for (std::size_t pos = 0; pos < 64; ++pos) {
        fill_equal();
        a[pos] = 0x10;
        b[pos] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, 64) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 64) > 0);
        ASSERT_NZ(bcmp_fn(a, b, 64));
    }

    // Diff at every byte position in a 96-byte buffer (exercises bulk exit
    // into the computed-jump word path with a remaining 32 bytes).
    for (std::size_t pos = 0; pos < 96; ++pos) {
        fill_equal();
        a[pos] = 0x01;
        b[pos] = 0xFE;
        ASSERT_TRUE(memcmp_fn(a, b, 96) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 96) > 0);
        ASSERT_NZ(bcmp_fn(a, b, 96));
    }

    // Diff at every byte position in a 33-byte buffer (exercises bulk 32-byte
    // block followed by 1 byte in the tail path).
    for (std::size_t pos = 0; pos < 33; ++pos) {
        fill_equal();
        a[pos] = 0x10;
        b[pos] = 0x20;
        ASSERT_TRUE(memcmp_fn(a, b, 33) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 33) > 0);
    }

    // Computed-jump diff resolver: test all 4 byte positions within a word.
    // Uses n=8 (2 words) to exercise the computed-jump path (not bulk).
    for (std::size_t pos = 0; pos < 8; ++pos) {
        fill_equal();
        a[pos] = 0x00;
        b[pos] = 0xFF;
        ASSERT_TRUE(memcmp_fn(a, b, 8) < 0);
        ASSERT_TRUE(memcmp_fn(b, a, 8) > 0);
    }

    // Large equal data crossing multiple double-pump iterations
    fill_equal();
    ASSERT_EQ(memcmp_fn(a, b, 512), 0);
    ASSERT_EQ(bcmp_fn(a, b, 512), 0);

    test::finalize();
}
