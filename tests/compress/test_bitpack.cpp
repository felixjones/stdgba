/**
 * @file test_bitpack.cpp
 * @brief Tests for compile-time bit packing and BIOS BitUnPack decompression.
 */
#include <gba/bios>
#include <gba/compress>

#include <array>

#include <mgba_test.hpp>

// Test data: 8-bit values that fit in 4 bits (0-15)
static constexpr auto test_data_8bit = std::array<unsigned char, 16>{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

// Pack at compile time (should pack 8-bit values into 4-bit packed format)
static constexpr auto packed_8bit = gba::bit_pack([] { return test_data_8bit; });

// Test data: 16-bit values that fit in 4 bits (0-15)
static constexpr auto test_data_16bit = std::array<unsigned short, 8>{
    0, 1, 2, 3, 15, 14, 13, 12
};

static constexpr auto packed_16bit = gba::bit_pack([] { return test_data_16bit; });

// Test data: 32-bit values that fit in 2 bits (0-3)
static constexpr auto test_data_32bit = std::array<unsigned int, 8>{
    0, 1, 2, 3, 3, 2, 1, 0
};

static constexpr auto packed_32bit = gba::bit_pack([] { return test_data_32bit; });

// Test data: values that fit in 1 bit (0-1)
static constexpr auto test_data_1bit = std::array<unsigned char, 16>{
    0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1
};

static constexpr auto packed_1bit = gba::bit_pack([] { return test_data_1bit; });

// Test data: values that fit in 2 bits (0-3)
static constexpr auto test_data_2bit = std::array<unsigned char, 16>{
    0, 1, 2, 3, 3, 2, 1, 0, 1, 2, 3, 0, 2, 1, 3, 2
};

static constexpr auto packed_2bit = gba::bit_pack([] { return test_data_2bit; });

int main() {
    // ========================================
    // Test 8-bit source packed to 4-bit
    // ========================================
    {
        // Verify packing reduced size (16 bytes -> 8 bytes for 4bpp)
        EXPECT_TRUE(sizeof(packed_8bit) <= sizeof(test_data_8bit));

        // Verify parameters
        EXPECT_EQ(packed_8bit.src_bpp, 4);
        EXPECT_EQ(packed_8bit.dst_bpp, 8);

        // Decompress using BIOS
        alignas(4) typename decltype(packed_8bit)::unpacked_array decompressed{};
        gba::BitUnPack(packed_8bit.data(), decompressed.data(), packed_8bit);

        // Verify decompressed data matches original
        for (std::size_t i = 0; i < test_data_8bit.size(); ++i) {
            EXPECT_EQ(decompressed[i], test_data_8bit[i]);
        }
    }

    // ========================================
    // Test 16-bit source packed to 4-bit
    // ========================================
    {
        EXPECT_EQ(packed_16bit.src_bpp, 4);
        EXPECT_EQ(packed_16bit.dst_bpp, 16);

        alignas(4) typename decltype(packed_16bit)::unpacked_array decompressed{};
        gba::BitUnPack(packed_16bit.data(), decompressed.data(), packed_16bit);

        for (std::size_t i = 0; i < test_data_16bit.size(); ++i) {
            EXPECT_EQ(decompressed[i], test_data_16bit[i]);
        }
    }

    // ========================================
    // Test 32-bit source packed to 2-bit
    // ========================================
    {
        EXPECT_EQ(packed_32bit.src_bpp, 2);
        EXPECT_EQ(packed_32bit.dst_bpp, 32);

        alignas(4) typename decltype(packed_32bit)::unpacked_array decompressed{};
        gba::BitUnPack(packed_32bit.data(), decompressed.data(), packed_32bit);

        for (std::size_t i = 0; i < test_data_32bit.size(); ++i) {
            EXPECT_EQ(decompressed[i], test_data_32bit[i]);
        }
    }

    // ========================================
    // Test 8-bit source packed to 1-bit
    // ========================================
    {
        EXPECT_EQ(packed_1bit.src_bpp, 1);
        EXPECT_EQ(packed_1bit.dst_bpp, 8);

        alignas(4) typename decltype(packed_1bit)::unpacked_array decompressed{};
        gba::BitUnPack(packed_1bit.data(), decompressed.data(), packed_1bit);

        for (std::size_t i = 0; i < test_data_1bit.size(); ++i) {
            EXPECT_EQ(decompressed[i], test_data_1bit[i]);
        }
    }

    // ========================================
    // Test 8-bit source packed to 2-bit
    // ========================================
    {
        EXPECT_EQ(packed_2bit.src_bpp, 2);
        EXPECT_EQ(packed_2bit.dst_bpp, 8);

        alignas(4) typename decltype(packed_2bit)::unpacked_array decompressed{};
        gba::BitUnPack(packed_2bit.data(), decompressed.data(), packed_2bit);

        for (std::size_t i = 0; i < test_data_2bit.size(); ++i) {
            EXPECT_EQ(decompressed[i], test_data_2bit[i]);
        }
    }
}
