/// @file test_huffman.cpp
/// @brief Tests for compile-time Huffman compression and BIOS decompression.
#include <gba/bios>
#include <gba/compress>
#include <gba/testing>

#include <array>

// Test data with skewed frequencies (compresses well with Huffman)
static constexpr auto test_data = std::array<unsigned char, 64>{
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', // 8 'a's
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', // 8 'a's
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', // 8 'a's
    'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', // 8 'b's
    'b', 'b', 'b', 'b', 'c', 'c', 'c', 'c', // 4 'b's, 4 'c's
    'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', // Various
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', // 8 'a's
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', // 8 'a's
};

// Compress at compile time with 8-bit symbols
static constexpr auto compressed = gba::huffman_compress<8>([] { return test_data; });

int main() {
    // Verify header is correct
    gba::test.expect.eq(compressed.comp_algo, gba::comp_algo_huff);
    gba::test.expect.eq(compressed.dst_len, test_data.size());
    gba::test.expect.eq(compressed.src_len, 8u); // 8-bit symbols

    // Decompress at runtime using BIOS
    alignas(4) std::array<unsigned char, 64> decompressed{};
    gba::HuffUnComp(compressed, decompressed.data());

    // Verify decompressed data matches original
    for (std::size_t i = 0; i < test_data.size(); ++i) {
        gba::test.expect.eq(decompressed[i], test_data[i]);
    }
    return gba::test.finish();
}
