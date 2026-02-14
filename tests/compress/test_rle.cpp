/**
 * @file test_rle.cpp
 * @brief Tests for compile-time RLE compression and BIOS decompression.
 */
#include <gba/bios>
#include <gba/compress>

#include <mgba_test.hpp>

#include <array>

// Test data with long runs (compresses well with RLE)
static constexpr auto test_data = std::array<unsigned char, 64>{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8 zeros
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8 zeros
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 8 FFs
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 8 FFs
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, // 8 AAs
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, // 8 AAs
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, // 8 55s
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, // 8 55s
};

// Compress at compile time
static constexpr auto compressed = gba::rle_compress([] {
    return test_data;
});

int main() {
    // Verify compression reduced size significantly (runs compress well)
    EXPECT_TRUE(sizeof(compressed) < sizeof(test_data));

    // Verify header is correct
    EXPECT_EQ(compressed.comp_algo, gba::comp_algo_rl);
    EXPECT_EQ(compressed.dst_len, test_data.size());

    // Decompress at runtime using BIOS
    alignas(4) std::array<unsigned char, 64> decompressed{};
    gba::RLUnCompWram(compressed, decompressed.data());

    // Verify decompressed data matches original
    for (std::size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(decompressed[i], test_data[i]);
    }
}
