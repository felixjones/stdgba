/// @file bits/embed/bdf_pack.hpp
/// @brief Internal BDF bitmap row packing helpers for gba::embed.
#pragma once

#include <cstddef>
#include <cstdint>

namespace gba::embed::bits {

    struct bdf_line {
        const unsigned char* data{};
        std::size_t size{};
    };

    consteval bool is_bdf_ws(unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r';
    }

    consteval std::size_t cstrlen(const char* str) {
        std::size_t len = 0;
        while (str[len] != '\0') ++len;
        return len;
    }

    consteval unsigned int hex_value(unsigned char c) {
        if (c >= '0' && c <= '9') return static_cast<unsigned int>(c - '0');
        if (c >= 'A' && c <= 'F') return 10u + static_cast<unsigned int>(c - 'A');
        if (c >= 'a' && c <= 'f') return 10u + static_cast<unsigned int>(c - 'a');
        throw "bdf: invalid hex digit";
    }

    /// @brief Pack one BDF BITMAP row into BitUnPack-friendly 1bpp bytes.
    ///
    /// Leftmost pixels map to lower source bits so BIOS BitUnPack expands them
    /// in left-to-right order when reading source bits from LSB to MSB.
    consteval void pack_bdf_row(bdf_line line, unsigned int width, unsigned char* dest, unsigned int stride) {
        for (unsigned int i = 0; i < stride; ++i) dest[i] = 0;

        unsigned int x = 0;
        unsigned int out_index = 0;
        unsigned int out_bit = 0;
        unsigned char out_byte = 0;

        for (std::size_t i = 0; i < line.size && x < width; ++i) {
            const auto c = line.data[i];
            if (is_bdf_ws(c)) continue;

            const auto nibble = hex_value(c);
            for (unsigned int bit = 0; bit < 4 && x < width; ++bit) {
                const bool set = (nibble & (1u << (3u - bit))) != 0;
                if (set) {
                    out_byte = static_cast<unsigned char>(out_byte | (1u << out_bit));
                }

                ++out_bit;
                ++x;
                if (out_bit == 8) {
                    if (out_index >= stride) throw "bdf: packed row exceeds stride";
                    dest[out_index++] = out_byte;
                    out_byte = 0;
                    out_bit = 0;
                }
            }
        }

        if (x < width) throw "bdf: bitmap row is shorter than glyph width";
        if (out_bit != 0) {
            if (out_index >= stride) throw "bdf: packed row exceeds stride";
            dest[out_index] = out_byte;
        }
    }

} // namespace gba::embed::bits
