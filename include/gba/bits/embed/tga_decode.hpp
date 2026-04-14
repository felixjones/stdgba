/// @file bits/embed/tga_decode.hpp
/// @brief Compile-time TGA full-image decode (uncompressed and RLE).
#pragma once

#include <gba/bits/embed/tga_header.hpp>

namespace gba::embed::bits {

    template<std::size_t Size>
    consteval void tga_decode_all(const std::array<unsigned char, Size>& data, const image_header& hdr,
                                  gba::color* pixels, bool* transparent) {
        auto total = hdr.width * hdr.height;

        bool is_rle = (hdr.tga_image_type >= 9);
        bool is_cmap = (hdr.tga_image_type == 1 || hdr.tga_image_type == 9);
        bool has_alpha = (hdr.tga_bpp == 32) || (hdr.tga_bpp == 16 && !is_cmap);
        unsigned int cmap_entry_bytes = (hdr.tga_cmap_bpp + 7) / 8;
        unsigned int pixel_bytes = (hdr.tga_bpp + 7) / 8;

        std::size_t pos = hdr.pixel_offset;
        unsigned int pixel_idx = 0;

        auto decode_one = [&](std::size_t src) {
            if (pixel_idx >= total) return;
            unsigned int actual_y = hdr.tga_origin_top ? (pixel_idx / hdr.width)
                                                       : (hdr.height - 1 - pixel_idx / hdr.width);
            unsigned int actual_x = pixel_idx % hdr.width;
            auto di = actual_y * hdr.width + actual_x;

            if (is_cmap) {
                unsigned int ci = data[src];
                if (pixel_bytes == 2) ci = data[src] | (data[src + 1] << 8);
                auto cmap_pos = hdr.tga_cmap_offset + ci * cmap_entry_bytes;
                pixels[di] = tga_decode_color(data, cmap_pos, hdr.tga_cmap_bpp);
                transparent[di] = (hdr.tga_cmap_bpp == 32 || hdr.tga_cmap_bpp == 16)
                                       ? tga_decode_alpha(data, cmap_pos, hdr.tga_cmap_bpp)
                                       : false;
            } else {
                pixels[di] = tga_decode_color(data, src, hdr.tga_bpp);
                transparent[di] = has_alpha ? tga_decode_alpha(data, src, hdr.tga_bpp) : false;
            }
            ++pixel_idx;
        };

        if (is_rle) {
            while (pixel_idx < total && pos < Size) {
                auto packet = data[pos++];
                auto count = (packet & 0x7F) + 1;
                if (packet & 0x80) {
                    auto src = pos;
                    pos += pixel_bytes;
                    for (unsigned int j = 0; j < count; ++j) decode_one(src);
                } else {
                    for (unsigned int j = 0; j < count; ++j) {
                        decode_one(pos);
                        pos += pixel_bytes;
                    }
                }
            }
        } else {
            for (unsigned int j = 0; j < total; ++j) {
                decode_one(pos);
                pos += pixel_bytes;
            }
        }
    }

} // namespace gba::embed::bits
