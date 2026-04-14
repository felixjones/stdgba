/// @file bits/embed/tga.hpp
/// @brief Compile-time TGA parsing and decoding for image embedding.
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
