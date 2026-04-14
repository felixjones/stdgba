/// @file bits/embed/tga_header.hpp
/// @brief Compile-time TGA header parsing and per-pixel color/alpha decoding.
#pragma once

#include <gba/bits/embed/common.hpp>

namespace gba::embed::bits {

    template<std::size_t Size>
    consteval image_header parse_tga_header(const std::array<unsigned char, Size>& data) {
        unsigned int id_length = data[0];
        unsigned int cmap_type = data[1];
        unsigned int image_type = data[2];
        unsigned int cmap_length = data[5] | (data[6] << 8);
        unsigned int cmap_bpp = data[7];
        unsigned int w = data[12] | (data[13] << 8);
        unsigned int h = data[14] | (data[15] << 8);
        unsigned int bpp = data[16];
        bool top = (data[17] & 0x20) != 0;

        if (w == 0 || h == 0) throw "TGA: invalid dimensions";

        bool is_cmap = (image_type == 1 || image_type == 9);
        bool is_true = (image_type == 2 || image_type == 10);
        bool is_gray = (image_type == 3 || image_type == 11);
        if (!is_cmap && !is_true && !is_gray) throw "TGA: unsupported image type";

        if (is_cmap && cmap_type != 1) throw "TGA: color-mapped image requires color map";
        if (is_true && bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32)
            throw "TGA: unsupported true-color bit depth";
        if (is_gray && bpp != 8) throw "TGA: grayscale must be 8bpp";
        if (is_cmap && bpp != 8 && bpp != 16) throw "TGA: color-mapped index must be 8 or 16 bpp";

        std::size_t pixel_offset = 18 + id_length;
        unsigned int cmap_off = 0;
        if (cmap_type == 1) {
            cmap_off = pixel_offset;
            pixel_offset += cmap_length * ((cmap_bpp + 7) / 8);
        }

        return {w, h, pixel_offset, true, false, image_type, bpp, top, cmap_off, cmap_length, cmap_bpp, 0};
    }

    template<std::size_t Size>
    consteval gba::color tga_decode_color(const std::array<unsigned char, Size>& data, std::size_t i,
                                          unsigned int bpp) {
        if (bpp == 32 || bpp == 24) return to_gba_color(data[i + 2], data[i + 1], data[i]);
        if (bpp == 16 || bpp == 15) {
            unsigned int v = data[i] | (data[i + 1] << 8);
            return {.red = static_cast<unsigned short>(v & 0x1F),
                    .green = static_cast<unsigned short>((v >> 5) & 0x1F),
                    .blue = static_cast<unsigned short>((v >> 10) & 0x1F),
                    .grn_lo = 0};
        }
        auto g = data[i];
        return to_gba_color(g, g, g);
    }

    template<std::size_t Size>
    consteval bool tga_decode_alpha(const std::array<unsigned char, Size>& data, std::size_t i, unsigned int bpp) {
        if (bpp == 32) return data[i + 3] < 128;
        if (bpp == 16) return (data[i + 1] & 0x80) == 0;
        return false;
    }

} // namespace gba::embed::bits
