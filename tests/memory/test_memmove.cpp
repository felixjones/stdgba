/// @file tests/memory/test_memmove.cpp
/// @brief Unit tests for optimized memmove implementation.
///
/// Tests every code path in memmove.s:
/// - Forward delegation to memcpy (dest <= src, dest >= src + n)
/// - Backward byte copy (n <= 3, unresolvable misalignment)
/// - Backward alignment fixup from end (all 4 offsets: 0, 1, 2, 3)
/// - Backward bulk 32-byte ldmdb/stmdb loop (exact, +1, +2, +3 remainder)
/// - Backward word copy via computed jump (various counts)
/// - Backward joaobapt tail (0-3 bytes at start)
/// - Backward halfword path (with/without byte fixup, trailing byte)
/// - __aeabi_memmove4 / __aeabi_memmove8 direct entry
/// - Overlapping regions: dest < src, dest > src, dest == src
/// - C memmove return value

#include <cstring>

#include <mgba_test.hpp>

// Prevent compiler from replacing memmove calls with builtins
static void* (*volatile memmove_fn)(void*, const void*, std::size_t) = &std::memmove;

// Direct access to the AEABI entries
extern "C" {
    void __aeabi_memmove(void*, const void*, std::size_t);
    void __aeabi_memmove4(void*, const void*, std::size_t);
    void __aeabi_memmove8(void*, const void*, std::size_t);
}

// Function pointers to prevent inlining / constant folding
static void (*volatile aeabi_memmove_fn)(void*, const void*, std::size_t) = &__aeabi_memmove;
static void (*volatile aeabi_memmove4_fn)(void*, const void*, std::size_t) = &__aeabi_memmove4;
static void (*volatile aeabi_memmove8_fn)(void*, const void*, std::size_t) = &__aeabi_memmove8;

namespace {

alignas(8) unsigned char buf[512];

void fill_pattern() {
    for (std::size_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = static_cast<unsigned char>(i);
    }
}

void fill_sentinel() {
    for (auto& b : buf) b = 0xCD;
}

// Reference memmove for verification (naive byte-by-byte)
void ref_memmove(unsigned char* dst, const unsigned char* src, std::size_t n) {
    if (dst <= src) {
        for (std::size_t i = 0; i < n; ++i) dst[i] = src[i];
    } else {
        for (std::size_t i = n; i > 0; --i) dst[i - 1] = src[i - 1];
    }
}

void do_move(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memmove_fn(d, s, n); });
}

void do_move4(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memmove4_fn(d, s, n); });
}

void do_move8(void* d, const void* s, std::size_t n) {
    test::do_not_optimize([&] { aeabi_memmove8_fn(d, s, n); });
}

// Test overlapping move with a reference buffer
// dir: 0 = non-overlapping forward, 1 = dest > src (backward), -1 = dest < src (forward)
void test_overlap(std::size_t src_off, std::size_t dst_off, std::size_t n,
                  void (*fn)(void*, const void*, std::size_t)) {
    // Prepare reference
    alignas(8) unsigned char ref[512];
    fill_pattern();
    for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];

    // Do reference move
    ref_memmove(ref + dst_off, ref + src_off, n);

    // Do real move
    fill_pattern();
    test::do_not_optimize([&] { fn(buf + dst_off, buf + src_off, n); });

    // Verify
    for (std::size_t i = 0; i < sizeof(buf); ++i) {
        ASSERT_EQ(buf[i], ref[i]);
    }
}

} // namespace

int main() {
    // Non-overlapping forward: dest < src (delegates to memcpy)

    // Zero bytes
    {
        fill_sentinel();
        do_move(buf, buf + 64, 0);
        ASSERT_EQ(buf[0], static_cast<unsigned char>(0xCD));
    }

    // Small: 1-3 bytes
    for (std::size_t n = 1; n <= 3; ++n) {
        fill_pattern();
        test_overlap(64, 0, n, aeabi_memmove_fn);
    }

    // Word-aligned various sizes (forward path)
    for (std::size_t n : {4u, 8u, 12u, 16u, 28u, 32u, 48u, 64u, 128u, 256u}) {
        fill_pattern();
        test_overlap(256, 0, n, aeabi_memmove_fn);
    }

    // Non-overlapping: dest > src but no overlap (dest >= src + n)

    for (std::size_t n : {4u, 32u, 64u}) {
        fill_pattern();
        test_overlap(0, 256, n, aeabi_memmove_fn);
    }

    // Overlapping: dest > src (backward copy)
    // This is the core memmove test -- regions overlap and need backward copy

    // Overlap by 1 byte: move [0..n) to [1..n+1)
    for (std::size_t n : {1u, 2u, 3u, 4u, 7u, 8u, 15u, 16u, 28u, 31u, 32u,
                          33u, 48u, 63u, 64u, 65u, 96u, 128u, 255u}) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // Overlap by 2 bytes
    for (std::size_t n : {2u, 3u, 4u, 8u, 16u, 32u, 64u, 128u}) {
        test_overlap(0, 2, n, aeabi_memmove_fn);
    }

    // Overlap by 3 bytes
    for (std::size_t n : {3u, 4u, 8u, 16u, 32u, 64u, 128u}) {
        test_overlap(0, 3, n, aeabi_memmove_fn);
    }

    // Overlap by 4 bytes (word)
    for (std::size_t n : {4u, 8u, 16u, 28u, 32u, 48u, 64u, 128u, 256u}) {
        test_overlap(0, 4, n, aeabi_memmove_fn);
    }

    // Large overlap (dst = src + 8, common for shifting arrays)
    for (std::size_t n : {8u, 16u, 32u, 64u, 128u, 256u}) {
        test_overlap(0, 8, n, aeabi_memmove_fn);
    }

    // Near-total overlap (dst = src + 1, n = large)
    test_overlap(0, 1, 256, aeabi_memmove_fn);

    // Overlapping: dest < src (forward copy, but overlapping)

    for (std::size_t n : {4u, 8u, 16u, 32u, 64u, 128u}) {
        test_overlap(4, 0, n, aeabi_memmove_fn);
    }
    for (std::size_t n : {8u, 32u, 128u}) {
        test_overlap(8, 0, n, aeabi_memmove_fn);
    }

    // dest == src: no-op (but must not corrupt)

    {
        fill_pattern();
        alignas(8) unsigned char ref[512];
        for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
        do_move(buf + 32, buf + 32, 64);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            ASSERT_EQ(buf[i], ref[i]);
        }
    }

    // __aeabi_memmove4: word-aligned entry, backward path

    // Forward (no overlap): word-aligned
    for (std::size_t n : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        test_overlap(256, 0, n, aeabi_memmove4_fn);
    }

    // Backward (overlap): word-aligned offsets
    for (std::size_t n : {4u, 8u, 16u, 28u, 32u, 48u, 64u, 128u, 256u}) {
        test_overlap(0, 4, n, aeabi_memmove4_fn);
    }
    for (std::size_t n : {8u, 32u, 64u, 128u, 256u}) {
        test_overlap(0, 8, n, aeabi_memmove4_fn);
    }

    // Backward bulk: 32 byte boundary tests
    for (std::size_t n : {32u, 33u, 34u, 35u, 36u, 60u, 63u, 64u, 65u,
                          96u, 127u, 128u, 129u, 192u, 256u}) {
        test_overlap(0, 4, n, aeabi_memmove4_fn);
    }

    // __aeabi_memmove8: 8-byte aligned entry

    for (std::size_t n : {8u, 16u, 32u, 64u, 128u}) {
        test_overlap(0, 8, n, aeabi_memmove8_fn);
    }

    // Backward alignment fixup: all 4 end-offsets
    // The end pointer (dest + n) can have offset 0, 1, 2, 3 mod 4.
    // Test each by varying n with overlap at offset 1.

    // End offset 0 (n = 4, 8, 12, ...)
    for (std::size_t n : {4u, 8u, 12u, 16u, 20u, 24u, 28u, 32u, 64u}) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // End offset 1 (n = 5, 9, 13, ...)
    for (std::size_t n : {5u, 9u, 13u, 17u, 21u, 25u, 29u, 33u, 65u}) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // End offset 2 (n = 6, 10, 14, ...)
    for (std::size_t n : {6u, 10u, 14u, 18u, 22u, 26u, 30u, 34u, 66u}) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // End offset 3 (n = 7, 11, 15, ...)
    for (std::size_t n : {7u, 11u, 15u, 19u, 23u, 27u, 31u, 35u, 67u}) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // Backward halfword path: src/dst halfword-compatible but not word

    // Offset 2 from word boundary: e.g. buf+2 to buf+4 (halfword-compatible)
    for (std::size_t n : {2u, 4u, 6u, 8u, 10u, 15u, 16u, 32u, 63u}) {
        test_overlap(2, 4, n, aeabi_memmove_fn);
    }

    // Odd halfword alignment
    for (std::size_t n : {3u, 5u, 7u, 9u, 16u, 31u}) {
        test_overlap(6, 8, n, aeabi_memmove_fn);
    }

    // Backward byte path: unresolvable misalignment

    // Offsets that differ in bit 0 (byte-only)
    for (std::size_t n : {1u, 2u, 3u, 4u, 7u, 8u, 15u, 16u, 31u, 32u, 63u}) {
        test_overlap(1, 4, n, aeabi_memmove_fn);   // off 1 vs 4: bit0 differs
    }
    for (std::size_t n : {1u, 3u, 5u, 8u, 16u, 32u}) {
        test_overlap(3, 6, n, aeabi_memmove_fn);   // off 3 vs 6: bit0 differs
    }

    // Backward word computed jump: each word count 1-7

    for (std::size_t words = 1; words <= 7; ++words) {
        test_overlap(0, 4, words * 4, aeabi_memmove4_fn);
    }

    // With tail bytes: word + 1, 2, 3 tail
    for (std::size_t words = 1; words <= 7; ++words) {
        for (std::size_t tail = 1; tail <= 3; ++tail) {
            test_overlap(0, 1, words * 4 + tail, aeabi_memmove_fn);
        }
    }

    // C memmove return value

    {
        fill_pattern();
        void* ret = memmove_fn(buf + 16, buf, 32);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(ret),
                  reinterpret_cast<std::uintptr_t>(buf + 16));
    }

    // C memmove wrapper: overlap safety with literal constant sizes.
    //
    // These call std::memmove DIRECTLY (not via volatile fn pointer) so
    // the compiler can see the literal size. Without LTO / -O3 these go
    // through the assembly fallback, but they still verify the C standard
    // memmove contract. The inline specialisations are tested separately
    // in test_memmove_inline.cpp (compiled with -O3).

    // Small constant backward overlap: dest > src, n = 1..6
    // These trigger specialisation 2 (inline byte copies, n <= 6).
    for (int trial = 0; trial < 4; ++trial) {
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 1);
            std::memmove(buf + 1, buf + 0, 1);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 2);
            std::memmove(buf + 1, buf + 0, 2);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 3);
            std::memmove(buf + 1, buf + 0, 3);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 4);
            std::memmove(buf + 1, buf + 0, 4);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 5);
            std::memmove(buf + 1, buf + 0, 5);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 1, ref + 0, 6);
            std::memmove(buf + 1, buf + 0, 6);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
    }

    // Small constant forward overlap: dest < src, n = 1..6
    for (int trial = 0; trial < 4; ++trial) {
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 0, ref + 1, 4);
            std::memmove(buf + 0, buf + 1, 4);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
        {
            fill_pattern();
            alignas(8) unsigned char ref[512];
            for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
            ref_memmove(ref + 0, ref + 1, 6);
            std::memmove(buf + 0, buf + 1, 6);
            for (std::size_t i = 0; i < sizeof(buf); ++i) {
                ASSERT_EQ(buf[i], ref[i]);
            }
        }
    }

    // Word-aligned backward overlap with constant sizes that previously
    // triggered the (now removed) inline ldr/str specialisation.
    {
        fill_pattern();
        alignas(8) unsigned char ref[512];
        for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
        ref_memmove(ref + 4, ref + 0, 8);
        std::memmove(buf + 4, buf + 0, 8);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            ASSERT_EQ(buf[i], ref[i]);
        }
    }
    {
        fill_pattern();
        alignas(8) unsigned char ref[512];
        for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
        ref_memmove(ref + 4, ref + 0, 32);
        std::memmove(buf + 4, buf + 0, 32);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            ASSERT_EQ(buf[i], ref[i]);
        }
    }
    {
        fill_pattern();
        alignas(8) unsigned char ref[512];
        for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
        ref_memmove(ref + 4, ref + 0, 60);
        std::memmove(buf + 4, buf + 0, 60);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            ASSERT_EQ(buf[i], ref[i]);
        }
    }

    // dest == src via C memmove with constant size
    {
        fill_pattern();
        alignas(8) unsigned char ref[512];
        for (std::size_t i = 0; i < sizeof(ref); ++i) ref[i] = buf[i];
        std::memmove(buf + 8, buf + 8, 4);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            ASSERT_EQ(buf[i], ref[i]);
        }
    }

    // Exhaustive small sizes: all n from 0 to 64, overlap by 1

    for (std::size_t n = 0; n <= 64; ++n) {
        test_overlap(0, 1, n, aeabi_memmove_fn);
    }

    // Exhaustive small sizes: all n from 0 to 64, overlap by 4 (word-aligned)

    for (std::size_t n = 0; n <= 64; ++n) {
        test_overlap(0, 4, n, aeabi_memmove_fn);
    }

    test::with_logger([] {
        mgba_printf(MGBA_LOG_INFO, "RESULT: OK (%d assertion(s))", test::passes());
    });

    test::exit(test::failures());
    __builtin_unreachable();
}
