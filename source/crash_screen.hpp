/// @file crash_screen.hpp
/// @brief Shared crash screen drawing primitives.
///
/// Internal header used by assert.cpp and divzero.cpp. Not part of the public API.
/// Provides the 3x5 bitmap font, Mode 3 drawing helpers, and hex formatting.
#pragma once

#include <cstdint>
#include <gba/bits/font_3x5.hpp>

namespace crash {


    inline constexpr int WIDTH = 240;
    inline constexpr int HEIGHT = 160;
    inline constexpr int FONT_W = 3;
    inline constexpr int FONT_H = 6;  // 5 rows + 1 for descenders

    inline constexpr std::uint16_t BG = 0x4000;      // Dark blue
    inline constexpr std::uint16_t WHITE = 0x7FFF;
    inline constexpr std::uint16_t RED = 0x001F;
    inline constexpr std::uint16_t YELLOW = 0x03FF;
    inline constexpr std::uint16_t GRAY = 0x294A;
    inline constexpr std::uint16_t CYAN = 0x7FE0;

    inline auto vram() { return reinterpret_cast<volatile std::uint16_t*>(0x6000000); }

    inline void draw_rect(volatile std::uint16_t* vram, int x, int y, int w, int h, std::uint16_t color) {
    for (int row = y; row < y + h && row < HEIGHT; ++row) {
        if (row < 0) continue;
        volatile std::uint16_t* row_ptr = vram + row * WIDTH;
        for (int col = x; col < x + w && col < WIDTH; ++col) {
            if (col >= 0) row_ptr[col] = color;
        }
    }
}

    inline void draw_char(volatile std::uint16_t* vram, int x, int y, char c, std::uint16_t color) {
    if (c < 32 || c > 127) c = '?';
    const std::uint16_t glyph = gba::bits::font_3x5_data[c - 32];

    for (int row = 0; row < FONT_H; ++row) {
        const int py = y + row;
        if (py < 0 || py >= HEIGHT) continue;

        const int pixels = gba::bits::get_font_line(glyph, row);
        volatile std::uint16_t* row_ptr = vram + py * WIDTH;

        if ((pixels & 0x08) && x >= 0 && x < WIDTH)
            row_ptr[x] = color;
        if ((pixels & 0x04) && x + 1 >= 0 && x + 1 < WIDTH)
            row_ptr[x + 1] = color;
        if ((pixels & 0x02) && x + 2 >= 0 && x + 2 < WIDTH)
            row_ptr[x + 2] = color;
    }
}

    inline int draw_string(volatile std::uint16_t* vram, int x, int y, const char* str, std::uint16_t color) {
    const int start_x = x;
    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += FONT_H + 1;
        } else {
            if (x + FONT_W > WIDTH) {
                x = start_x;
                y += FONT_H + 1;
            }
            draw_char(vram, x, y, *str, color);
            x += FONT_W + 1;
        }
        ++str;
    }
    return y;
}

    inline const char* itoa(char* buf, int value) {
    char* p = buf + 15;
    *p = '\0';
    const bool negative = value < 0;
    if (negative) value = -value;
    do {
        *--p = '0' + (value % 10);
        value /= 10;
    } while (value > 0);
    if (negative) *--p = '-';
    return p;
}

    inline const char* hex32(char* buf, std::uint32_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; --i) {
        buf[2 + (7 - i)] = digits[(value >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
    return buf;
}

    inline const char* hex16(char* buf, std::uint16_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 3; i >= 0; --i) {
        buf[2 + (3 - i)] = digits[(value >> (i * 4)) & 0xF];
    }
    buf[6] = '\0';
    return buf;
}

    /// @brief Draw a labeled hex32 value at the given position.
    /// @return The y coordinate after drawing.
    inline int draw_label_hex32(volatile std::uint16_t* vram, int x, int y,
                            const char* label, std::uint32_t value, char* buf) {
    int label_width = 0;
    for (const char* p = label; *p; ++p) ++label_width;
    draw_string(vram, x, y, label, GRAY);
    draw_string(vram, x + label_width * (FONT_W + 1), y, hex32(buf, value), WHITE);
    return y;
}

} // namespace crash
