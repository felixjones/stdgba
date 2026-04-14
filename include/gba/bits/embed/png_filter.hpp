/// @file bits/embed/png_filter.hpp
/// @brief Compile-time PNG scanline unfiltering (filter types 0–4 + Paeth predictor).
#pragma once

#include <cstddef>

namespace gba::embed::bits {


    consteval unsigned char paeth_predict(unsigned int a, unsigned int b, unsigned int c) {
        int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
        int pa = p > static_cast<int>(a) ? p - static_cast<int>(a) : static_cast<int>(a) - p;
        int pb = p > static_cast<int>(b) ? p - static_cast<int>(b) : static_cast<int>(b) - p;
        int pc = p > static_cast<int>(c) ? p - static_cast<int>(c) : static_cast<int>(c) - p;
        if (pa <= pb && pa <= pc) return static_cast<unsigned char>(a);
        if (pb <= pc) return static_cast<unsigned char>(b);
        return static_cast<unsigned char>(c);
    }

    consteval void png_unfilter(unsigned char* raw, unsigned int width, unsigned int height, unsigned int bpp_bytes) {
        auto stride = width * bpp_bytes;
        for (unsigned int y = 0; y < height; ++y) {
            auto row_start = y * (1 + stride);
            auto filter = raw[row_start];
            auto* row = raw + row_start + 1;
            const unsigned char* prev = (y > 0) ? (raw + (y - 1) * (1 + stride) + 1) : nullptr;

            switch (filter) {
                case 0: // None
                    break;
                case 1: // Sub
                    for (unsigned int x = bpp_bytes; x < stride; ++x)
                        row[x] = static_cast<unsigned char>(row[x] + row[x - bpp_bytes]);
                    break;
                case 2: // Up
                    if (prev) {
                        for (unsigned int x = 0; x < stride; ++x)
                            row[x] = static_cast<unsigned char>(row[x] + prev[x]);
                    }
                    break;
                case 3: // Average
                    for (unsigned int x = 0; x < stride; ++x) {
                        unsigned int a = (x >= bpp_bytes) ? row[x - bpp_bytes] : 0;
                        unsigned int b = prev ? prev[x] : 0;
                        row[x] = static_cast<unsigned char>(row[x] + ((a + b) >> 1));
                    }
                    break;
                case 4: // Paeth
                    for (unsigned int x = 0; x < stride; ++x) {
                        unsigned int a = (x >= bpp_bytes) ? row[x - bpp_bytes] : 0;
                        unsigned int b = prev ? prev[x] : 0;
                        unsigned int c = (prev && x >= bpp_bytes) ? prev[x - bpp_bytes] : 0;
                        row[x] = static_cast<unsigned char>(row[x] + paeth_predict(a, b, c));
                    }
                    break;
                default:
                    throw "PNG: unsupported filter type";
            }
        }
    }

} // namespace gba::embed::bits
