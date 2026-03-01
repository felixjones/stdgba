/// @file test_embed.cpp
/// @brief Tests for compile-time image embedding.

#include <gba/embed>

#include <mgba_test.hpp>

// Hand-crafted 8x8 PPM (P6) with 2 colors: red and blue
// "P6\n8 8\n255\n" (11 bytes) + 64 RGB pixels (192 bytes) = 203 bytes
static constexpr auto make_test_ppm() {
    constexpr char header[] = "P6\n8 8\n255\n";
    constexpr int hdr_len = 11; // not including null terminator
    std::array<unsigned char, hdr_len + 8 * 8 * 3> data{};
    for (int i = 0; i < hdr_len; ++i) data[i] = header[i];
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            auto i = hdr_len + (y * 8 + x) * 3;
            if (y < 4) {
                data[i] = 255; data[i + 1] = 0; data[i + 2] = 0;
            } else {
                data[i] = 0; data[i + 1] = 0; data[i + 2] = 255;
            }
        }
    }
    return data;
}

// Hand-crafted 8x8 TGA (type 2, 24bpp, top-left origin)
static constexpr auto make_test_tga_24() {
    std::array<unsigned char, 18 + 8 * 8 * 3> data{};
    data[2] = 2;   // uncompressed true-color
    data[12] = 8;  data[14] = 8;
    data[16] = 24; data[17] = 0x20;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            auto i = 18 + (y * 8 + x) * 3;
            if (y < 4) { data[i + 1] = 255; } // BGR green
            else       { data[i] = 255; }      // BGR blue
        }
    return data;
}

// Hand-crafted 8x8 TGA (type 2, 32bpp alpha, top-left origin)
static constexpr auto make_test_tga_32() {
    std::array<unsigned char, 18 + 8 * 8 * 4> data{};
    data[2] = 2;
    data[12] = 8; data[14] = 8;
    data[16] = 32; data[17] = 0x28;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            auto i = 18 + (y * 8 + x) * 4;
            if (y < 4) { data[i + 2] = 255; data[i + 3] = 255; } // red, opaque
        }
    return data;
}

// Hand-crafted 8x8 TGA (type 10, RLE 24bpp, top-left origin)
static constexpr auto make_test_tga_rle() {
    std::array<unsigned char, 18 + 1 + 3> data{};
    data[2] = 10;  data[12] = 8; data[14] = 8;
    data[16] = 24; data[17] = 0x20;
    data[18] = 0xBF; // repeat 64 times
    data[19] = 255; data[20] = 255; data[21] = 255; // white
    return data;
}

// Hand-crafted 8x8 TGA (type 3, grayscale 8bpp, top-left origin)
static constexpr auto make_test_tga_gray() {
    std::array<unsigned char, 18 + 8 * 8> data{};
    data[2] = 3; data[12] = 8; data[14] = 8;
    data[16] = 8; data[17] = 0x20;
    for (int i = 0; i < 64; ++i) data[18 + i] = 128;
    return data;
}

int main() {
    // bitmap15: basic PPM parsing
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_ppm(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.height, 8u);
        ASSERT_EQ(result.data[0].red, 31u);
        ASSERT_EQ(result.data[0].green, 0u);
        ASSERT_EQ(result.data[0].blue, 0u);
        ASSERT_EQ(result.data[4 * 8].red, 0u);
        ASSERT_EQ(result.data[4 * 8].green, 0u);
        ASSERT_EQ(result.data[4 * 8].blue, 31u);
    }

    // indexed4: PPM with few colors
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_ppm(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.height, 8u);
        ASSERT_EQ(result.palette.size(), 3u);
        ASSERT_EQ(result.palette[1].red, 31u);
        ASSERT_EQ(result.palette[1].blue, 0u);
        ASSERT_EQ(result.palette[2].red, 0u);
        ASSERT_EQ(result.palette[2].blue, 31u);
        ASSERT_EQ(result.tiles.size(), 1u);
        ASSERT_EQ(result.map.size(), 1024u); // one screenblock
        ASSERT_EQ(result.map[0].tile_index, 0u);
    }

    // indexed8: PPM
    {
        constexpr auto result = gba::embed::indexed8([] { return make_test_ppm(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.height, 8u);
        ASSERT_EQ(result.palette.size(), 3u);
        ASSERT_EQ(result.tiles.size(), 1u);
    }

    // TGA 24bpp uncompressed
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_24(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.height, 8u);
        ASSERT_EQ(result.data[0].red, 0u);
        ASSERT_EQ(result.data[0].green, 31u);
        ASSERT_EQ(result.data[0].blue, 0u);
        ASSERT_EQ(result.data[4 * 8].red, 0u);
        ASSERT_EQ(result.data[4 * 8].green, 0u);
        ASSERT_EQ(result.data[4 * 8].blue, 31u);
    }

    // TGA 32bpp with alpha transparency
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_tga_32(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.height, 8u);
        ASSERT_EQ(result.palette[0].red, 0u);  // transparent
        ASSERT_EQ(result.palette[1].red, 31u);  // red
    }

    // TGA RLE compressed
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_rle(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.data[0].red, 31u);
        ASSERT_EQ(result.data[0].green, 31u);
        ASSERT_EQ(result.data[0].blue, 31u);
        ASSERT_EQ(result.data[63].red, 31u);
    }

    // TGA grayscale
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_gray(); });
        ASSERT_EQ(result.width, 8u);
        ASSERT_EQ(result.data[0].red, 16u);
        ASSERT_EQ(result.data[0].green, 16u);
        ASSERT_EQ(result.data[0].blue, 16u);
    }

    // indexed4: obj() pre-fills OAM attributes for 8x8 sprite
    {
        constexpr auto result = gba::embed::indexed4<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj(5);
        static_assert(result.width == 8);
        static_assert(result.height == 8);
        ASSERT_EQ(static_cast<unsigned>(obj.depth) & 1, 0u); // depth_4bpp
        ASSERT_EQ(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        ASSERT_EQ(static_cast<unsigned>(obj.size) & 3, 0u);  // 8x8
        ASSERT_EQ(obj.tile_index, 5u);
    }

    // indexed4: obj_aff() pre-fills affine OAM attributes
    {
        constexpr auto result = gba::embed::indexed4<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj_aff(3);
        ASSERT_EQ(static_cast<unsigned>(obj.depth) & 1, 0u); // depth_4bpp
        ASSERT_EQ(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        ASSERT_EQ(static_cast<unsigned>(obj.size) & 3, 0u);  // 8x8
        ASSERT_EQ(obj.tile_index, 3u);
    }

    // indexed8: obj() pre-fills OAM attributes for 8x8 sprite
    {
        constexpr auto result = gba::embed::indexed8<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj(7);
        ASSERT_EQ(static_cast<unsigned>(obj.depth) & 1, 1u); // depth_8bpp
        ASSERT_EQ(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        ASSERT_EQ(obj.tile_index, 7u);
    }

    // indexed8: obj_aff() pre-fills affine OAM attributes
    {
        constexpr auto result = gba::embed::indexed8<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj_aff(2);
        ASSERT_EQ(static_cast<unsigned>(obj.depth) & 1, 1u); // depth_8bpp
        ASSERT_EQ(obj.tile_index, 2u);
    }

    return 0;
}
