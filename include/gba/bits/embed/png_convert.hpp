/// @file bits/embed/png_convert.hpp
/// @brief Compile-time PNG pixel conversion (RGB, RGBA, indexed).
#pragma once

#include <gba/bits/embed/png_chunks.hpp>
#include <gba/color>

namespace gba::embed::bits {


    consteval void png_convert_rgb(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                   gba::color* pixels, bool* transparent,
                                   bool has_trns, unsigned char trns_r, unsigned char trns_g, unsigned char trns_b) {
        auto stride = width * 3;
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto off = x * 3;
                auto r = row[off];
                auto g = row[off + 1];
                auto b = row[off + 2];
                auto di = y * width + x;
                pixels[di] = gba::bits::from_rgb((static_cast<unsigned int>(r) << 16) |
                                                 (static_cast<unsigned int>(g) << 8)  |
                                                  static_cast<unsigned int>(b));
                transparent[di] = has_trns && r == trns_r && g == trns_g && b == trns_b;
            }
        }
    }

    consteval void png_convert_rgba(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                    gba::color* pixels, bool* transparent) {
        auto stride = width * 4;
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto off = x * 4;
                auto r = row[off];
                auto g = row[off + 1];
                auto b = row[off + 2];
                auto di = y * width + x;
                pixels[di] = gba::bits::from_rgb((static_cast<unsigned int>(r) << 16) |
                                                 (static_cast<unsigned int>(g) << 8)  |
                                                  static_cast<unsigned int>(b));
                transparent[di] = (row[off + 3] < 128);
            }
        }
    }

    template<std::size_t Size>
    consteval void png_convert_indexed(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                       const std::array<unsigned char, Size>& data, const png_chunk_info& info,
                                       gba::color* pixels, bool* transparent) {
        if (info.plte_count == 0) throw "PNG: indexed image requires PLTE chunk";
        gba::color plte[256]{};
        for (unsigned int i = 0; i < info.plte_count && i < 256; ++i) {
            auto off = info.plte_offset + i * 3;
            plte[i] = gba::bits::from_rgb((static_cast<unsigned int>(data[off]) << 16) |
                                          (static_cast<unsigned int>(data[off + 1]) << 8) |
                                           static_cast<unsigned int>(data[off + 2]));
        }
        bool plte_trans[256]{};
        if (info.trns_len > 0) {
            for (unsigned int i = 0; i < info.trns_len && i < 256; ++i)
                plte_trans[i] = (data[info.trns_offset + i] < 128);
        }
        auto stride = width;
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto idx = row[x];
                auto di = y * width + x;
                if (idx < info.plte_count) {
                    pixels[di] = plte[idx];
                    transparent[di] = plte_trans[idx];
                } else {
                    pixels[di] = {};
                    transparent[di] = true;
                }
            }
        }
    }

} // namespace gba::embed::bits
