/// @file bits/embed/png.hpp
/// @brief Compile-time PNG decode — umbrella header + full decode entry point.
///
/// Supports non-interlaced PNG with 8-bit channels:
/// - Color type 2 (RGB)
/// - Color type 3 (indexed with PLTE)
/// - Color type 6 (RGBA)
///
/// Consteval performance notes:
/// - Single-pass chunk scan (PLTE, tRNS, IDAT positions collected in one traversal)
/// - Streaming IDAT reader eliminates large intermediate copy buffer
/// - Counting-sort Huffman build: O(n) instead of O(n × max_bits)
/// - Filter type and color type dispatched outside inner pixel loops
#pragma once

#include <gba/bits/embed/png_convert.hpp>
#include <gba/bits/embed/png_deflate.hpp>
#include <gba/bits/embed/png_filter.hpp>

namespace gba::embed::bits {


    template<std::size_t Size>
    consteval void png_decode_all(const std::array<unsigned char, Size>& data, unsigned int width, unsigned int height,
                                  unsigned int color_type, gba::color* pixels, bool* transparent) {
        auto bpp_bytes = (color_type == 2) ? 3u : (color_type == 6) ? 4u : 1u;
        auto stride = width * bpp_bytes;
        auto raw_size = height * (1 + stride);

        // Single-pass chunk scan
        auto info = png_scan_chunks(data);

        // Streaming inflate directly from IDAT spans (no intermediate buffer)
        png_idat_reader<Size> reader{data, info};
        reader.skip_zlib_header();

        unsigned char scanlines[raw_size];
        auto inflated = deflate_inflate(reader, scanlines, raw_size);
        if (inflated != raw_size) throw "PNG: inflated size does not match expected scanline data";

        // Unfilter (filter type hoisted outside inner loop)
        png_unfilter(scanlines, width, height, bpp_bytes);

        // Convert to pixels (color type dispatched outside loops)
        if (color_type == 2) {
            bool has_trns = false;
            unsigned char trns_r = 0, trns_g = 0, trns_b = 0;
            if (info.trns_len >= 6) {
                trns_r = data[info.trns_offset + 1];
                trns_g = data[info.trns_offset + 3];
                trns_b = data[info.trns_offset + 5];
                has_trns = true;
            }
            png_convert_rgb(scanlines, width, height, pixels, transparent, has_trns, trns_r, trns_g, trns_b);
        } else if (color_type == 6) {
            png_convert_rgba(scanlines, width, height, pixels, transparent);
        } else {
            png_convert_indexed(scanlines, width, height, data, info, pixels, transparent);
        }
    }

} // namespace gba::embed::bits
