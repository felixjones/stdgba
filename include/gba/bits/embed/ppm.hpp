/// @file bits/embed/ppm.hpp
/// @brief Compile-time PPM (P6) parsing for image embedding.
#pragma once

#include <gba/bits/embed/common.hpp>

namespace gba::embed::bits {

    template<std::size_t Size>
    consteval image_header parse_ppm_header(const std::array<unsigned char, Size>& data) {
        std::size_t pos = 2;
        auto w = parse_uint(data.data(), Size, pos);
        auto h = parse_uint(data.data(), Size, pos);
        auto maxval = parse_uint(data.data(), Size, pos);
        if (w == 0 || h == 0) throw "PPM: invalid dimensions";
        if (maxval != 255) throw "PPM: only maxval 255 supported";
        if (pos < Size && is_ws(data[pos])) ++pos;
        return {w, h, pos, false, false, 0, 24, false, 0, 0, 0, 0};
    }

    template<std::size_t Size>
    consteval gba::color ppm_read_pixel(const std::array<unsigned char, Size>& data, const image_header& hdr,
                                        unsigned int x, unsigned int y) {
        auto i = hdr.pixel_offset + (y * hdr.width + x) * 3;
        return to_gba_color(data[i], data[i + 1], data[i + 2]);
    }

    template<std::size_t Size>
    consteval void ppm_decode_all(const std::array<unsigned char, Size>& data, const image_header& hdr,
                                  gba::color* pixels, bool* transparent) {
        for (unsigned int y = 0; y < hdr.height; ++y) {
            for (unsigned int x = 0; x < hdr.width; ++x) {
                auto pi = y * hdr.width + x;
                pixels[pi] = ppm_read_pixel(data, hdr, x, y);
                transparent[pi] = false;
            }
        }
    }

} // namespace gba::embed::bits
