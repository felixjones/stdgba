/// @file bits/embed/common.hpp
/// @brief Shared compile-time image embed helpers.
#pragma once

#include <gba/video>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::embed::bits {

    consteval gba::color to_gba_color(unsigned char r, unsigned char g, unsigned char b) {
        return {.red = static_cast<unsigned short>(r >> 3),
                .green = static_cast<unsigned short>(g >> 3),
                .blue = static_cast<unsigned short>(b >> 3),
                .grn_lo = static_cast<unsigned short>((g >> 2) & 1)};
    }

    consteval bool color_eq(gba::color a, gba::color b) {
        return a.red == b.red && a.green == b.green && a.blue == b.blue && a.grn_lo == b.grn_lo;
    }

    consteval bool is_ws(unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    consteval unsigned int parse_uint(const unsigned char* d, std::size_t size, std::size_t& pos) {
        while (pos < size && is_ws(d[pos])) ++pos;
        if (pos < size && d[pos] == '#')
            while (pos < size && d[pos] != '\n') ++pos;
        if (pos < size && is_ws(d[pos])) ++pos;
        unsigned int v = 0;
        while (pos < size && d[pos] >= '0' && d[pos] <= '9') v = v * 10 + (d[pos++] - '0');
        return v;
    }

    struct image_header {
        unsigned int width;
        unsigned int height;
        std::size_t pixel_offset; // PPM only: offset to raw RGB data
        bool is_tga;
        bool is_png;
        // TGA fields
        unsigned int tga_image_type;
        unsigned int tga_bpp;
        bool tga_origin_top;
        unsigned int tga_cmap_offset;
        unsigned int tga_cmap_length;
        unsigned int tga_cmap_bpp;
        // PNG fields
        unsigned int png_color_type;
    };

} // namespace gba::embed::bits
