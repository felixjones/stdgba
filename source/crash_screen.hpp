/// @file crash_screen.hpp
/// @brief Shared crash screen drawing primitives.
///
/// Internal header used by assert.cpp and divzero.cpp. Not part of the public API.
/// Provides the 3x5 bitmap font, Mode 3 drawing helpers, and hex formatting.
#pragma once

#include <gba/bits/font_3x5.hpp>

#include <cstdint>

namespace crash {

    inline constexpr int width = 240;
    inline constexpr int height = 160;
    inline constexpr int font_w = 3;
    inline constexpr int font_h = 6; // 5 rows + 1 for descenders

    inline constexpr std::uint16_t color_bg = 0x4000; // Dark blue
    inline constexpr std::uint16_t color_white = 0x7FFF;
    inline constexpr std::uint16_t color_red = 0x001F;
    inline constexpr std::uint16_t color_yellow = 0x03FF;
    inline constexpr std::uint16_t color_gray = 0x294A;
    inline constexpr std::uint16_t color_cyan = 0x7FE0;

    inline auto vram() {
        return reinterpret_cast<volatile std::uint16_t*>(0x6000000);
    }

    inline void draw_rect(volatile std::uint16_t* vram, int x, int y, int w, int h, std::uint16_t color) {
        for (int row = y; row < y + h && row < height; ++row) {
            if (row < 0) continue;
            volatile std::uint16_t* rowPtr = vram + (row * width);
            for (int col = x; col < x + w && col < width; ++col) {
                if (col >= 0) rowPtr[col] = color;
            }
        }
    }

    inline void draw_char(volatile std::uint16_t* vram, int x, int y, char c, std::uint16_t color) {
        if (c < 32 || c > 127) c = '?';
        // The clamp above keeps the index inside the 96-entry table.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        const std::uint16_t glyph = gba::bits::font_3x5_data[c - 32];

        for (int row = 0; row < font_h; ++row) {
            const int py = y + row;
            if (py < 0 || py >= height) continue;

            const unsigned pixels = gba::bits::get_font_line(glyph, row);
            volatile std::uint16_t* rowPtr = vram + (py * width);

            if (((pixels & 0x08u) != 0u) && x >= 0 && x < width) rowPtr[x] = color;
            if (((pixels & 0x04u) != 0u) && x + 1 >= 0 && x + 1 < width) rowPtr[x + 1] = color;
            if (((pixels & 0x02u) != 0u) && x + 2 >= 0 && x + 2 < width) rowPtr[x + 2] = color;
        }
    }

    inline int draw_string(volatile std::uint16_t* vram, int x, int y, const char* str, std::uint16_t color) {
        const int startX = x;
        while ((*str) != 0) {
            if (*str == '\n') {
                x = startX;
                y += font_h + 1;
            } else {
                if (x + font_w > width) {
                    x = startX;
                    y += font_h + 1;
                }
                draw_char(vram, x, y, *str, color);
                x += font_w + 1;
            }
            ++str;
        }
        return y;
    }

    inline const char* itoa(char* buf, int value) {
        char* p = buf + 15;
        *p = '\0';

        const bool negative = value < 0;
        unsigned int magnitude = 0;
        if (negative) {
            // Avoid UB for INT_MIN.
            magnitude = static_cast<unsigned int>(-(value + 1)) + 1u;
        } else {
            magnitude = static_cast<unsigned int>(value);
        }

        if (magnitude == 0u) {
            *--p = '0';
        } else {
            while (magnitude > 0u) {
                *--p = static_cast<char>('0' + (magnitude % 10u));
                magnitude /= 10u;
            }
        }

        if (negative) *--p = '-';
        return p;
    }

    inline constexpr const char* hex_digits = "0123456789ABCDEF";

    inline const char* hex32(char* buf, std::uint32_t value) {
        buf[0] = '0';
        buf[1] = 'x';
        for (unsigned i = 0; i < 8u; ++i) {
            buf[2u + i] = hex_digits[(value >> ((7u - i) * 4u)) & 0xFu];
        }
        buf[10] = '\0';
        return buf;
    }

    inline const char* hex16(char* buf, std::uint16_t value) {
        buf[0] = '0';
        buf[1] = 'x';
        for (unsigned i = 0; i < 4u; ++i) {
            buf[2u + i] = hex_digits[(static_cast<std::uint32_t>(value) >> ((3u - i) * 4u)) & 0xFu];
        }
        buf[6] = '\0';
        return buf;
    }

    /// @brief Draw a labeled hex32 value at the given position.
    /// @return The y coordinate after drawing.
    inline int draw_label_hex32(volatile std::uint16_t* vram, int x, int y, const char* label, std::uint32_t value,
                                char* buf) {
        int labelWidth = 0;
        for (const char* p = label; (*p) != 0; ++p) ++labelWidth;
        draw_string(vram, x, y, label, color_gray);
        draw_string(vram, x + (labelWidth * (font_w + 1)), y, hex32(buf, value), color_white);
        return y;
    }

} // namespace crash
