/// @file test_embed.cpp
/// @brief Tests for compile-time image embedding.

#include <gba/embed>
#include <gba/testing>

#include <type_traits>

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
                data[i] = 255;
                data[i + 1] = 0;
                data[i + 2] = 0;
            } else {
                data[i] = 0;
                data[i + 1] = 0;
                data[i + 2] = 255;
            }
        }
    }
    return data;
}

// Hand-crafted 16x8 PPM with two 8x8 tiles.
// Each tile uses 15 opaque colors, forcing indexed4 to use two palette banks.
static constexpr auto make_test_ppm_two_bank() {
    constexpr char header[] = "P6\n16 8\n255\n";
    constexpr int hdr_len = 12; // not including null terminator
    std::array<unsigned char, hdr_len + 16 * 8 * 3> data{};
    for (int i = 0; i < hdr_len; ++i) data[i] = header[i];

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            auto i = hdr_len + (y * 16 + x) * 3;
            const unsigned char level = static_cast<unsigned char>((1 + ((y * 8 + x) % 15)) * 8);
            if (x < 8) {
                data[i] = level;
                data[i + 1] = 0;
                data[i + 2] = 0;
            } else {
                data[i] = 0;
                data[i + 1] = level;
                data[i + 2] = 0;
            }
        }
    }
    return data;
}

// Hand-crafted 8x8 TGA (type 2, 24bpp, top-left origin)
static constexpr auto make_test_tga_24() {
    std::array<unsigned char, 18 + 8 * 8 * 3> data{};
    data[2] = 2; // uncompressed true-color
    data[12] = 8;
    data[14] = 8;
    data[16] = 24;
    data[17] = 0x20;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            auto i = 18 + (y * 8 + x) * 3;
            if (y < 4) {
                data[i + 1] = 255;
            } // BGR green
            else {
                data[i] = 255;
            } // BGR blue
        }
    return data;
}

// Hand-crafted 8x8 TGA (type 2, 32bpp alpha, top-left origin)
static constexpr auto make_test_tga_32() {
    std::array<unsigned char, 18 + 8 * 8 * 4> data{};
    data[2] = 2;
    data[12] = 8;
    data[14] = 8;
    data[16] = 32;
    data[17] = 0x28;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            auto i = 18 + (y * 8 + x) * 4;
            if (y < 4) {
                data[i + 2] = 255;
                data[i + 3] = 255;
            } // red, opaque
        }
    return data;
}

// Hand-crafted 8x8 TGA (type 10, RLE 24bpp, top-left origin)
static constexpr auto make_test_tga_rle() {
    std::array<unsigned char, 18 + 1 + 3> data{};
    data[2] = 10;
    data[12] = 8;
    data[14] = 8;
    data[16] = 24;
    data[17] = 0x20;
    data[18] = 0xBF; // repeat 64 times
    data[19] = 255;
    data[20] = 255;
    data[21] = 255; // white
    return data;
}

// Hand-crafted 8x8 TGA (type 3, grayscale 8bpp, top-left origin)
static constexpr auto make_test_tga_gray() {
    std::array<unsigned char, 18 + 8 * 8> data{};
    data[2] = 3;
    data[12] = 8;
    data[14] = 8;
    data[16] = 8;
    data[17] = 0x20;
    for (int i = 0; i < 64; ++i) data[18 + i] = 128;
    return data;
}

// -- PNG test helpers --

// Helper: write a 4-byte big-endian unsigned int at a position
static consteval void png_put_u32(unsigned char* d, unsigned int v) {
    d[0] = static_cast<unsigned char>((v >> 24) & 0xFF);
    d[1] = static_cast<unsigned char>((v >> 16) & 0xFF);
    d[2] = static_cast<unsigned char>((v >> 8) & 0xFF);
    d[3] = static_cast<unsigned char>(v & 0xFF);
}

// Build a minimal valid PNG file. Scanline data is embedded via stored deflate block.
// All parameters are templated to keep everything consteval-friendly.
template<std::size_t OutSize, std::size_t ScanLen, std::size_t PlteLen = 0, std::size_t TrnsLen = 0>
static consteval auto make_png_impl(unsigned int w, unsigned int h, unsigned int color_type,
                                    const std::array<unsigned char, ScanLen>& scanlines,
                                    const std::array<unsigned char, PlteLen>& plte = {},
                                    unsigned int plte_entries = 0,
                                    const std::array<unsigned char, TrnsLen>& trns = {},
                                    unsigned int trns_len = 0) {
    std::array<unsigned char, OutSize> data{};
    std::size_t pos = 0;

    // Signature
    constexpr unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (int i = 0; i < 8; ++i) data[pos++] = sig[i];

    // IHDR chunk (13 bytes data)
    png_put_u32(data.data() + pos, 13);
    pos += 4;
    data[pos++] = 'I'; data[pos++] = 'H'; data[pos++] = 'D'; data[pos++] = 'R';
    png_put_u32(data.data() + pos, w); pos += 4;
    png_put_u32(data.data() + pos, h); pos += 4;
    data[pos++] = 8; // bit depth
    data[pos++] = static_cast<unsigned char>(color_type);
    data[pos++] = 0; // compression
    data[pos++] = 0; // filter
    data[pos++] = 0; // interlace
    pos += 4; // CRC placeholder

    // PLTE chunk (for indexed)
    if constexpr (PlteLen > 0) {
        if (plte_entries > 0) {
            auto plen = plte_entries * 3;
            png_put_u32(data.data() + pos, plen); pos += 4;
            data[pos++] = 'P'; data[pos++] = 'L'; data[pos++] = 'T'; data[pos++] = 'E';
            for (unsigned int i = 0; i < plen; ++i) data[pos++] = plte[i];
            pos += 4; // CRC
        }
    }

    // tRNS chunk
    if constexpr (TrnsLen > 0) {
        if (trns_len > 0) {
            png_put_u32(data.data() + pos, trns_len); pos += 4;
            data[pos++] = 't'; data[pos++] = 'R'; data[pos++] = 'N'; data[pos++] = 'S';
            for (unsigned int i = 0; i < trns_len; ++i) data[pos++] = trns[i];
            pos += 4; // CRC
        }
    }

    // IDAT chunk: zlib header + stored deflate block wrapping scanlines
    // zlib: CMF=0x78, FLG=0x01
    // stored block: BFINAL=1(bit0), BTYPE=00(bits1-2) -> byte=0x01
    auto scan_len = static_cast<unsigned int>(ScanLen);
    auto idat_payload_len = 2 + 1 + 4 + scan_len + 4; // zlib(2) + bfinal(1) + len+nlen(4) + data + adler(4)
    png_put_u32(data.data() + pos, static_cast<unsigned int>(idat_payload_len)); pos += 4;
    data[pos++] = 'I'; data[pos++] = 'D'; data[pos++] = 'A'; data[pos++] = 'T';
    // zlib header
    data[pos++] = 0x78;
    data[pos++] = 0x01;
    // stored block
    data[pos++] = 0x01; // BFINAL=1, BTYPE=00
    data[pos++] = static_cast<unsigned char>(scan_len & 0xFF);
    data[pos++] = static_cast<unsigned char>((scan_len >> 8) & 0xFF);
    data[pos++] = static_cast<unsigned char>(~scan_len & 0xFF);
    data[pos++] = static_cast<unsigned char>((~scan_len >> 8) & 0xFF);
    for (std::size_t i = 0; i < ScanLen; ++i) data[pos++] = scanlines[i];
    // Adler32 placeholder
    data[pos++] = 0; data[pos++] = 0; data[pos++] = 0; data[pos++] = 1;
    pos += 4; // CRC

    // IEND chunk
    png_put_u32(data.data() + pos, 0); pos += 4;
    data[pos++] = 'I'; data[pos++] = 'E'; data[pos++] = 'N'; data[pos++] = 'D';
    pos += 4; // CRC

    return data;
}

// Hand-crafted 8x8 PNG (RGB, no filter, red top / blue bottom)
static consteval auto make_test_png_rgb() {
    // 8x8 RGB: each scanline = filter(1) + 8*3 = 25 bytes, total = 200
    std::array<unsigned char, 8 * 25> scanlines{};
    for (int y = 0; y < 8; ++y) {
        scanlines[y * 25] = 0; // filter: None
        for (int x = 0; x < 8; ++x) {
            auto off = y * 25 + 1 + x * 3;
            if (y < 4) {
                scanlines[off] = 255; // R
            } else {
                scanlines[off + 2] = 255; // B
            }
        }
    }
    return make_png_impl<512>(8, 8, 2, scanlines);
}

// Hand-crafted 8x8 PNG (RGBA, alpha transparency: top=red opaque, bottom=transparent)
static consteval auto make_test_png_rgba() {
    // 8x8 RGBA: each scanline = filter(1) + 8*4 = 33 bytes, total = 264
    std::array<unsigned char, 8 * 33> scanlines{};
    for (int y = 0; y < 8; ++y) {
        scanlines[y * 33] = 0; // filter: None
        for (int x = 0; x < 8; ++x) {
            auto off = y * 33 + 1 + x * 4;
            if (y < 4) {
                scanlines[off] = 255;     // R
                scanlines[off + 3] = 255; // A (opaque)
            } else {
                scanlines[off + 2] = 255; // B
                scanlines[off + 3] = 0;   // A (transparent)
            }
        }
    }
    return make_png_impl<600>(8, 8, 6, scanlines);
}

// Hand-crafted 8x8 PNG (indexed, 2-color palette: red and blue)
static consteval auto make_test_png_indexed() {
    std::array<unsigned char, 6> plte = {255, 0, 0, 0, 0, 255}; // red, blue
    std::array<unsigned char, 8 * 9> scanlines{};
    for (int y = 0; y < 8; ++y) {
        scanlines[y * 9] = 0; // filter: None
        for (int x = 0; x < 8; ++x) {
            scanlines[y * 9 + 1 + x] = (y < 4) ? 0 : 1; // palette index
        }
    }
    return make_png_impl<400>(8, 8, 3, scanlines, plte, 2);
}

// Hand-crafted 8x8 PNG (indexed with tRNS: index 0 = transparent)
static consteval auto make_test_png_indexed_trns() {
    std::array<unsigned char, 9> plte = {255, 0, 0, 0, 255, 0, 0, 0, 255}; // red, green, blue
    std::array<unsigned char, 3> trns = {0, 255, 255}; // index 0 = transparent
    std::array<unsigned char, 8 * 9> scanlines{};
    for (int y = 0; y < 8; ++y) {
        scanlines[y * 9] = 0;
        for (int x = 0; x < 8; ++x) {
            if (y < 4)
                scanlines[y * 9 + 1 + x] = 0; // transparent
            else
                scanlines[y * 9 + 1 + x] = 1; // green (opaque)
        }
    }
    return make_png_impl<400>(8, 8, 3, scanlines, plte, 3, trns, 3);
}

// Hand-crafted 8x8 PNG (RGB, Sub filter on all rows, solid red)
static consteval auto make_test_png_sub_filter() {
    std::array<unsigned char, 8 * 25> scanlines{};
    for (int y = 0; y < 8; ++y) {
        scanlines[y * 25] = 1; // filter: Sub
        // First pixel: Sub(255,0,0) = (255,0,0)
        scanlines[y * 25 + 1] = 255;
        scanlines[y * 25 + 2] = 0;
        scanlines[y * 25 + 3] = 0;
        // Remaining: difference = 0 (same color)
    }
    return make_png_impl<512>(8, 8, 2, scanlines);
}

// Hand-crafted 8x8 PNG (RGB, Up filter: all rows same solid green)
static consteval auto make_test_png_up_filter() {
    std::array<unsigned char, 8 * 25> scanlines{};
    // Row 0: filter None, solid green
    scanlines[0] = 0;
    for (int x = 0; x < 8; ++x) scanlines[1 + x * 3 + 1] = 255;
    // Rows 1-7: filter Up, all zeros
    for (int y = 1; y < 8; ++y) scanlines[y * 25] = 2;
    return make_png_impl<512>(8, 8, 2, scanlines);
}

static constexpr auto make_test_bdf() {
    constexpr char source[] = R"(STARTFONT 2.1
FONT test
SIZE 12 100 100
FONTBOUNDINGBOX 9 18 0 -4
STARTPROPERTIES 3
FONT_ASCENT 14
FONT_DESCENT 4
DEFAULT_CHAR 65
ENDPROPERTIES
CHARS 3
STARTCHAR space
ENCODING 32
SWIDTH 540 0
DWIDTH 9 0
BBX 9 18 0 -4
BITMAP
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
ENDCHAR
STARTCHAR A
ENCODING 65
SWIDTH 540 0
DWIDTH 9 0
BBX 9 18 0 -4
BITMAP
0000
0000
0000
0000
0800
1400
1400
1400
2200
3E00
2200
4100
4100
4100
0000
0000
0000
0000
ENDCHAR
STARTCHAR edge
ENCODING 66
SWIDTH 540 0
DWIDTH 9 0
BBX 9 18 0 -4
BITMAP
0080
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
0000
ENDCHAR
ENDFONT
)";

    std::array<unsigned char, sizeof(source) - 1> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(source[i]);
    return data;
}

int main() {
    // bitmap15: basic PPM parsing
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_ppm(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.data[0].red, 31u);
        gba::test.eq(result.data[0].green, 0u);
        gba::test.eq(result.data[0].blue, 0u);
        gba::test.eq(result.data[4 * 8].red, 0u);
        gba::test.eq(result.data[4 * 8].green, 0u);
        gba::test.eq(result.data[4 * 8].blue, 31u);
    }

    // indexed4: PPM with few colors
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_ppm(); });
        static_assert(std::is_same_v<decltype(result.sprite), gba::sprite4<8, 8, 1>>);
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette.size(), 3u);
        gba::test.eq(result.palette[1].red, 31u);
        gba::test.eq(result.palette[1].blue, 0u);
        gba::test.eq(result.palette[2].red, 0u);
        gba::test.eq(result.palette[2].blue, 31u);
        gba::test.eq(result.sprite.tile_count, 1u);
        gba::test.eq(result.map.size(), 1024u); // one screenblock
        gba::test.eq(result.map[0].tile_index, 0u);
    }

    // indexed4: allows >16 global colors by using per-tile palette banks
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_ppm_two_bank(); });
        gba::test.eq(result.width, 16u);
        gba::test.eq(result.height, 8u);
        gba::test.is_true(result.sprite.tile_count >= 1u);
        gba::test.eq(result.map[0].palette_index, 0u);
        gba::test.eq(result.map[1].palette_index, 1u);
        gba::test.is_true(result.palette.size() > 16u);
    }

    // indexed8: PPM
    {
        constexpr auto result = gba::embed::indexed8([] { return make_test_ppm(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette.size(), 3u);
        gba::test.eq(result.tiles.size(), 1u);
    }

    // TGA 24bpp uncompressed
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_24(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.data[0].red, 0u);
        gba::test.eq(result.data[0].green, 31u);
        gba::test.eq(result.data[0].blue, 0u);
        gba::test.eq(result.data[4 * 8].red, 0u);
        gba::test.eq(result.data[4 * 8].green, 0u);
        gba::test.eq(result.data[4 * 8].blue, 31u);
    }

    // TGA 32bpp with alpha transparency
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_tga_32(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette[0].red, 0u);  // transparent
        gba::test.eq(result.palette[1].red, 31u); // red
    }

    // TGA RLE compressed
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_rle(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.data[0].red, 31u);
        gba::test.eq(result.data[0].green, 31u);
        gba::test.eq(result.data[0].blue, 31u);
        gba::test.eq(result.data[63].red, 31u);
    }

    // TGA grayscale
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_tga_gray(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.data[0].red, 16u);
        gba::test.eq(result.data[0].green, 16u);
        gba::test.eq(result.data[0].blue, 16u);
    }

    // indexed4: obj() pre-fills OAM attributes for 8x8 sprite
    {
        constexpr auto result = gba::embed::indexed4<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.sprite.obj(5);
        static_assert(result.width == 8);
        static_assert(result.height == 8);
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, 0u); // depth_4bpp
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        gba::test.eq(static_cast<unsigned>(obj.size) & 3, 0u);  // 8x8
        gba::test.eq(obj.tile_index, 5u);
    }

    // indexed4: obj_aff() pre-fills affine OAM attributes
    {
        constexpr auto result = gba::embed::indexed4<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.sprite.obj_aff(3);
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, 0u); // depth_4bpp
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        gba::test.eq(static_cast<unsigned>(obj.size) & 3, 0u);  // 8x8
        gba::test.eq(obj.tile_index, 3u);
    }

    // indexed8: obj() pre-fills OAM attributes for 8x8 sprite
    {
        constexpr auto result = gba::embed::indexed8<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj(7);
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, 1u); // depth_8bpp
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, 0u); // square
        gba::test.eq(obj.tile_index, 7u);
    }

    // indexed8: obj_aff() pre-fills affine OAM attributes
    {
        constexpr auto result = gba::embed::indexed8<gba::embed::dedup::none>([] { return make_test_ppm(); });
        constexpr auto obj = result.obj_aff(2);
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, 1u); // depth_8bpp
        gba::test.eq(obj.tile_index, 2u);
    }

    // PNG RGB: basic parsing
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_png_rgb(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.data[0].red, 31u);   // red pixel
        gba::test.eq(result.data[0].green, 0u);
        gba::test.eq(result.data[0].blue, 0u);
        gba::test.eq(result.data[4 * 8].red, 0u); // blue pixel
        gba::test.eq(result.data[4 * 8].green, 0u);
        gba::test.eq(result.data[4 * 8].blue, 31u);
    }

    // PNG RGBA: alpha transparency
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_png_rgba(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette[0].red, 0u);  // transparent
        gba::test.eq(result.palette[1].red, 31u);  // red (opaque)
    }

    // PNG indexed: PLTE palette
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_png_indexed(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette[1].red, 31u);  // red
        gba::test.eq(result.palette[2].blue, 31u);  // blue
    }

    // PNG indexed with tRNS: transparent palette entry
    {
        constexpr auto result = gba::embed::indexed4([] { return make_test_png_indexed_trns(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        // Index 0 is transparent, so palette[0] = transparent marker
        // The visible color should be green (from PLTE index 1)
        gba::test.eq(result.palette[1].green, 31u);
    }

    // PNG RGB with Sub filter
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_png_sub_filter(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        // All pixels should be solid red after Sub filter reconstruction
        gba::test.eq(result.data[0].red, 31u);
        gba::test.eq(result.data[0].green, 0u);
        gba::test.eq(result.data[7].red, 31u);
        gba::test.eq(result.data[63].red, 31u);
    }

    // PNG RGB with Up filter
    {
        constexpr auto result = gba::embed::bitmap15([] { return make_test_png_up_filter(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        // All pixels should be solid green
        gba::test.eq(result.data[0].green, 31u);
        gba::test.eq(result.data[0].red, 0u);
        gba::test.eq(result.data[63].green, 31u);
    }

    // PNG indexed8: basic parsing
    {
        constexpr auto result = gba::embed::indexed8([] { return make_test_png_indexed(); });
        gba::test.eq(result.width, 8u);
        gba::test.eq(result.height, 8u);
        gba::test.eq(result.palette.size(), 3u);
        gba::test.eq(result.tiles.size(), 1u);
    }

    // bdf: parse glyph metadata and pack 1bpp rows for BitUnPack
    {
        static constexpr auto font = gba::embed::bdf([] { return make_test_bdf(); });

        static_assert(font.glyph_count == 3);
        static_assert(font.bitmap_size == 108);
        static_assert(font.font_width == 9);
        static_assert(font.font_height == 18);
        static_assert(font.font_x == 0);
        static_assert(font.font_y == -4);
        static_assert(font.ascent == 14);
        static_assert(font.descent == 4);
        static_assert(font.line_height() == 18);
        static_assert(font.default_char == 65);

        static_assert(font.glyphs[0].encoding == 32);
        static_assert(font.glyphs[0].bitmap_offset == 0);
        static_assert(font.glyphs[0].bitmap_byte_width == 2);

        static_assert(font.glyphs[1].encoding == 65);
        static_assert(font.glyphs[1].dwidth == 9);
        static_assert(font.glyphs[1].width == 9);
        static_assert(font.glyphs[1].height == 18);
        static_assert(font.glyphs[1].x_offset == 0);
        static_assert(font.glyphs[1].y_offset == -4);
        static_assert(font.glyphs[1].bitmap_offset == 36);
        static_assert(font.glyphs[1].bitmap_byte_width == 2);
        static_assert(font.glyphs[1].bitmap_bytes() == 36);

        static_assert(font.bitmap[36 + 8] == 0x10);
        static_assert(font.bitmap[36 + 9] == 0x00);
        static_assert(font.bitmap[36 + 10] == 0x28);
        static_assert(font.bitmap[36 + 11] == 0x00);
        static_assert(font.bitmap[36 + 16] == 0x44);
        static_assert(font.bitmap[36 + 17] == 0x00);
        static_assert(font.bitmap[36 + 18] == 0x7C);
        static_assert(font.bitmap[36 + 19] == 0x00);
        static_assert(font.bitmap[36 + 22] == 0x82);
        static_assert(font.bitmap[36 + 23] == 0x00);

        static_assert(font.glyphs[2].encoding == 66);
        static_assert(font.glyphs[2].bitmap_offset == 72);
        static_assert(font.bitmap[72] == 0x00);
        static_assert(font.bitmap[73] == 0x01);

        const auto* glyphA = font.find(65);
        gba::test.is_true(glyphA != nullptr);
        gba::test.eq(glyphA->dwidth, 9u);
        gba::test.eq(glyphA->width, 9u);
        gba::test.eq(glyphA->height, 18u);
        gba::test.eq(glyphA->bitmap_byte_width, 2u);
        gba::test.eq(glyphA->bitmap_bytes(), 36u);

        const auto* glyphSpace = font.find(32);
        gba::test.is_true(glyphSpace != nullptr);
        gba::test.eq(glyphSpace->bitmap_offset, 0u);

        gba::test.is_true(font.find(999) == nullptr);
        gba::test.eq(font.glyph_or_default(999).encoding, 65u);

        const auto header = glyphA->bitunpack_header();
        gba::test.eq(header.src_len, 36u);
        gba::test.eq(header.src_bpp, 1u);
        gba::test.eq(header.dst_bpp, 4u);
        gba::test.eq(header.dst_ofs, 1u);
        gba::test.is_false(header.offset_zero);
        gba::test.eq(header.dst_len(), 144u);

        const auto* glyphABitmap = font.bitmap_data(*glyphA);
        gba::test.eq(glyphABitmap[8], 0x10u);
        gba::test.eq(glyphABitmap[10], 0x28u);
        gba::test.eq(glyphABitmap[16], 0x44u);
        gba::test.eq(glyphABitmap[18], 0x7Cu);
        gba::test.eq(glyphABitmap[22], 0x82u);

        const auto* glyphEdgeBitmap = font.bitmap_data(66);
        gba::test.eq(glyphEdgeBitmap[0], 0x00u);
        gba::test.eq(glyphEdgeBitmap[1], 0x01u);
    }

    return gba::test.finish();
}
