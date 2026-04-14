/// @file bits/embed/image.hpp
/// @brief Shared compile-time image dispatch helpers for embed.
#pragma once

#include <gba/bits/embed/common.hpp>
#include <gba/bits/embed/png.hpp>
#include <gba/bits/embed/ppm.hpp>
#include <gba/bits/embed/tga.hpp>

namespace gba::embed::bits {

    template<std::size_t Size>
    consteval image_header parse_header(const std::array<unsigned char, Size>& data) {
        if (Size >= 2 && data[0] == 'P' && data[1] == '6') {
            return parse_ppm_header(data);
        }
        if (png_check_signature(data)) {
            auto ihdr = png_parse_ihdr(data);
            return {ihdr.width, ihdr.height, 0, false, true, 0, 0, false, 0, 0, 0, ihdr.color_type};
        }
        if (Size >= 18) {
            return parse_tga_header(data);
        }
        throw "embed: unrecognized image format";
    }

    template<std::size_t Size>
    consteval void decode_image_all(const std::array<unsigned char, Size>& data, const image_header& hdr,
                                    gba::color* pixels, bool* transparent) {
        if (hdr.is_png) {
            png_decode_all(data, hdr.width, hdr.height, hdr.png_color_type, pixels, transparent);
        } else if (hdr.is_tga) {
            tga_decode_all(data, hdr, pixels, transparent);
        } else {
            ppm_decode_all(data, hdr, pixels, transparent);
        }
    }

    template<std::size_t MaxPixels>
    struct indexed_workspace {
        unsigned int width;
        unsigned int height;
        unsigned int palette_count;
        gba::color palette[256];
        unsigned char indices[MaxPixels];
    };

    template<std::size_t BufSize, std::size_t Size>
    consteval auto build_indexed(const std::array<unsigned char, Size>& data, const image_header& hdr) {
        indexed_workspace<BufSize> ws{};
        ws.width = hdr.width;
        ws.height = hdr.height;
        ws.palette_count = 1; // index 0 = transparent
        ws.palette[0] = {};

        gba::color pixels[BufSize]{};
        bool transparent[BufSize]{};
        decode_image_all(data, hdr, pixels, transparent);

        for (unsigned int y = 0; y < hdr.height; ++y) {
            for (unsigned int x = 0; x < hdr.width; ++x) {
                auto pi = y * hdr.width + x;
                if (transparent[pi]) {
                    ws.indices[pi] = 0;
                    continue;
                }
                auto c = pixels[pi];
                unsigned int idx = 0;
                for (unsigned int i = 1; i < ws.palette_count; ++i) {
                    if (color_eq(ws.palette[i], c)) {
                        idx = i;
                        break;
                    }
                }
                if (idx == 0) {
                    idx = ws.palette_count++;
                    if (idx >= 256) throw "embed: image has more than 256 unique colors";
                    ws.palette[idx] = c;
                }
                ws.indices[pi] = static_cast<unsigned char>(idx);
            }
        }
        return ws;
    }

    consteval unsigned int flip_nibbles(unsigned int word) {
        unsigned int out = 0;
        for (int c = 0; c < 8; ++c) out |= ((word >> (c * 4)) & 0xF) << ((7 - c) * 4);
        return out;
    }

    /// @brief Convert tile coordinates to GBA screenblock-order map index.
    consteval unsigned int sb_index(unsigned int tx, unsigned int ty, unsigned int map_w) {
        const auto sb_cols = (map_w + 31) / 32;
        const auto sb_x = tx / 32;
        const auto sb_y = ty / 32;
        return (sb_y * sb_cols + sb_x) * 1024 + (ty % 32) * 32 + (tx % 32);
    }

} // namespace gba::embed::bits
