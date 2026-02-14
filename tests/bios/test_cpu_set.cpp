#include <gba/bios>
#include <gba/interrupt>
#include <gba/peripherals>

#include <mgba_test.hpp>

int main() {
    /// gba::CpuSet()

    // Test: CpuSet copy 16-bit
    {
        alignas(4) unsigned short src[4] = {1, 2, 3, 4};
        alignas(4) unsigned short dst[4] = {0, 0, 0, 0};
        gba::CpuSet(src, dst, {.count = 4}); // 4 halfwords, 16-bit mode
        ASSERT_EQ(dst, src);
    }

    // Test: CpuSet fill 16-bit
    {
        alignas(4) unsigned short src[1] = {0xABCD};
        alignas(4) unsigned short dst[4] = {0, 0, 0, 0};
        gba::CpuSet(src, dst, {.count = 4, .fill = true}); // Fill 4 halfwords
        for (int i = 0; i < 4; ++i) {
            ASSERT_EQ(dst[i], 0xABCD);
        }
    }

    // Test: CpuSet copy 32-bit
    {
        alignas(4) unsigned int src[2] = {0x12345678, 0x9ABCDEF0};
        alignas(4) unsigned int dst[2] = {0, 0};
        gba::CpuSet(src, dst, {.count = 2, .set_32bit = true}); // 2 words, 32-bit mode
        ASSERT_EQ(dst, src);
    }

    // Test: CpuSet fill 32-bit
    {
        alignas(4) unsigned int src[1] = {0xCAFEBABE};
        alignas(4) unsigned int dst[2] = {0, 0};
        gba::CpuSet(src, dst, {.count = 2, .fill = true, .set_32bit = true}); // Fill 2 words
        for (int i = 0; i < 2; ++i) {
            ASSERT_EQ(dst[i], 0xCAFEBABE);
        }
    }

    // Test: CpuSet zero length
    {
        alignas(4) unsigned short src[1] = {0x1234};
        alignas(4) unsigned short dst[1] = {0xFFFF};
        gba::CpuSet(src, dst, {}); // 0 halfwords
        ASSERT_EQ(dst[0], 0xFFFF); // Should remain unchanged
    }

    /// gba::CpuFastSet()

    // Test: CpuFastSet copy 32-bit
    {
        alignas(4) unsigned int src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        alignas(4) unsigned int dst[8] = {0};
        gba::CpuFastSet(src, dst, {.count = 8}); // 8 words
        ASSERT_EQ(dst, src);
    }

    // Test: CpuFastSet fill 32-bit
    {
        alignas(4) unsigned int src[1] = {0xDEADBEEF};
        alignas(4) unsigned int dst[8] = {0};
        gba::CpuFastSet(src, dst, {.count = 8, .fill = true}); // Fill 8 words
        for (int i = 0; i < 8; ++i) {
            ASSERT_EQ(dst[i], 0xDEADBEEF);
        }
    }

    // Test: CpuFastSet zero length
    {
        alignas(4) unsigned int src[1] = {0x12345678};
        alignas(4) unsigned int dst[1] = {0xFFFFFFFF};
        gba::CpuFastSet(src, dst, {});
        ASSERT_EQ(dst[0], 0xFFFFFFFF);
    }
}
