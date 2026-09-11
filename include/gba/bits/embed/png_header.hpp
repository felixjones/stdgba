/// @file bits/embed/png_header.hpp
/// @brief Compile-time PNG signature check and IHDR parsing.
#pragma once

#include <gba/bits/constexpr_assert.hpp>
#include <gba/color>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::embed::bits {

    struct png_ihdr {
        unsigned int width;
        unsigned int height;
        unsigned int bit_depth;
        unsigned int color_type;
    };

    template<std::size_t Size>
    consteval unsigned int png_read_u32(const std::array<unsigned char, Size>& data, std::size_t pos) {
        return (static_cast<unsigned int>(data[pos]) << 24) | (static_cast<unsigned int>(data[pos + 1]) << 16) |
               (static_cast<unsigned int>(data[pos + 2]) << 8) | static_cast<unsigned int>(data[pos + 3]);
    }

    template<std::size_t Size>
    consteval bool png_check_signature(const std::array<unsigned char, Size>& data) {
        if (Size < 8) return false;
        return data[0] == 137 && data[1] == 80 && data[2] == 78 && data[3] == 71 && data[4] == 13 && data[5] == 10 &&
               data[6] == 26 && data[7] == 10;
    }

    template<std::size_t Size>
    consteval bool png_chunk_is(const std::array<unsigned char, Size>& data, std::size_t pos, unsigned char a,
                                unsigned char b, unsigned char c, unsigned char d) {
        return data[pos] == a && data[pos + 1] == b && data[pos + 2] == c && data[pos + 3] == d;
    }

    template<std::size_t Size>
    consteval png_ihdr png_parse_ihdr(const std::array<unsigned char, Size>& data) {
        ::gba::bits::constexpr_assert(Size < 33, "PNG: file too small for IHDR");
        ::gba::bits::constexpr_assert(png_read_u32(data, 8) != 13, "PNG: IHDR chunk length must be 13");
        ::gba::bits::constexpr_assert(!png_chunk_is(data, 12, 'I', 'H', 'D', 'R'), "PNG: first chunk must be IHDR");

        png_ihdr hdr{};
        hdr.width = png_read_u32(data, 16);
        hdr.height = png_read_u32(data, 20);
        hdr.bit_depth = data[24];
        hdr.color_type = data[25];
        ::gba::bits::constexpr_assert(data[26] != 0, "PNG: unsupported compression method");
        ::gba::bits::constexpr_assert(data[27] != 0, "PNG: unsupported filter method");
        ::gba::bits::constexpr_assert(data[28] != 0, "PNG: interlaced images not supported");

        ::gba::bits::constexpr_assert(hdr.width == 0 || hdr.height == 0, "PNG: invalid dimensions");
        ::gba::bits::constexpr_assert(hdr.bit_depth != 8, "PNG: only 8-bit depth supported");
        ::gba::bits::constexpr_assert(hdr.color_type != 2 && hdr.color_type != 3 && hdr.color_type != 6,
                                      "PNG: unsupported color type (only 2=RGB, 3=indexed, 6=RGBA)");

        return hdr;
    }

} // namespace gba::embed::bits
