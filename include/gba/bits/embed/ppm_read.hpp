/// @file bits/embed/ppm_read.hpp
/// @brief Compile-time PPM pixel reading and full-image decode.
#pragma once

#include <gba/bits/embed/ppm_header.hpp>

namespace gba::embed::bits {

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
