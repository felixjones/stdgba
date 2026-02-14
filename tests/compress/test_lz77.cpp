/**
 * @file test_lz77.cpp
 * @brief Tests for compile-time LZ77 compression and BIOS decompression.
 */
#include <gba/bios>
#include <gba/compress>

#include <mgba_test.hpp>

#include <algorithm>
#include <array>

// Test data with repeated patterns (compresses well with LZ77)
static constexpr auto test_data = std::array<unsigned char, 64>{
    0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33,
    0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
    0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
    0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33,
    0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33,
};

// Compress at compile time
static constexpr auto compressed = gba::lz77_compress([] {
    return test_data;
});

int main() {
    // Verify compression reduced size (test data has repeated patterns)
    EXPECT_TRUE(sizeof(compressed) <= sizeof(test_data));

    // Verify header is correct
    EXPECT_EQ(compressed.comp_algo, gba::comp_algo_lz77);
    EXPECT_TRUE(compressed.dst_len >= test_data.size());

    // Decompress at runtime using BIOS
    alignas(4) std::array<unsigned char, 64> decompressed{};
    gba::LZ77UnCompWram(compressed, decompressed.data());

    // Verify decompressed data matches original
    for (std::size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(decompressed[i], test_data[i]);
    }
}
