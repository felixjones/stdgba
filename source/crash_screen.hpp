/**
 * @file crash_screen.hpp
 * @brief Shared crash screen drawing primitives.
 *
 * Internal header used by assert.cpp and divzero.cpp. Not part of the public API.
 * Provides the 3x5 bitmap font, Mode 3 drawing helpers, and hex formatting.
 */
#pragma once

#include <cstdint>

namespace crash {

// ============================================================================
// Tiny 3x5 Font (ASCII 32-127, 96 characters)
// ============================================================================
// MIT License font from: https://hackaday.io/project/6309-vga-graphics-over-spi-and-serial-vgatonic
// Each glyph: 16 bits, 3 wide x 5 tall, bit 0 is descender flag

inline const std::uint16_t font_data[96] = {
    0x0000,  /*SPACE*/
    0x4908,  /*'!'*/
    0xb400,  /*'"'*/
    0xbef6,  /*'#'*/
    0x7b7a,  /*'$'*/
    0xa594,  /*'%'*/
    0x55b8,  /*'&'*/
    0x4800,  /*'''*/
    0x2944,  /*'('*/
    0x442a,  /*')'*/
    0x15a0,  /*'*'*/
    0x0b42,  /*'+'*/
    0x0050,  /*','*/
    0x0302,  /*'-'*/
    0x0008,  /*'.'*/
    0x2590,  /*'/'*/
    0x76ba,  /*'0'*/
    0x595c,  /*'1'*/
    0xc59e,  /*'2'*/
    0xc538,  /*'3'*/
    0x92e6,  /*'4'*/
    0xf33a,  /*'5'*/
    0x73ba,  /*'6'*/
    0xe590,  /*'7'*/
    0x77ba,  /*'8'*/
    0x773a,  /*'9'*/
    0x0840,  /*':'*/
    0x0850,  /*';'*/
    0x2a44,  /*'<'*/
    0x1ce0,  /*'='*/
    0x8852,  /*'>'*/
    0xe508,  /*'?'*/
    0x568e,  /*'@'*/
    0x77b6,  /*'A'*/
    0x77b8,  /*'B'*/
    0x728c,  /*'C'*/
    0xd6ba,  /*'D'*/
    0x739e,  /*'E'*/
    0x7392,  /*'F'*/
    0x72ae,  /*'G'*/
    0xb7b6,  /*'H'*/
    0xe95c,  /*'I'*/
    0x64aa,  /*'J'*/
    0xb7b4,  /*'K'*/
    0x929c,  /*'L'*/
    0xbeb6,  /*'M'*/
    0xd6b6,  /*'N'*/
    0x56aa,  /*'O'*/
    0xd792,  /*'P'*/
    0x76ee,  /*'Q'*/
    0x77b4,  /*'R'*/
    0x7138,  /*'S'*/
    0xe948,  /*'T'*/
    0xb6ae,  /*'U'*/
    0xb6aa,  /*'V'*/
    0xb6f6,  /*'W'*/
    0xb5b4,  /*'X'*/
    0xb548,  /*'Y'*/
    0xe59c,  /*'Z'*/
    0x694c,  /*'['*/
    0x9124,  /*'\'*/
    0x642e,  /*']'*/
    0x5400,  /*'^'*/
    0x001c,  /*'_'*/
    0x4400,  /*'`'*/
    0x0eae,  /*'a'*/
    0x9aba,  /*'b'*/
    0x0e8c,  /*'c'*/
    0x2eae,  /*'d'*/
    0x0ece,  /*'e'*/
    0x56d0,  /*'f'*/
    0x553B,  /*'g' descender*/
    0x93b4,  /*'h'*/
    0x4144,  /*'i'*/
    0x4151,  /*'j' descender*/
    0x97b4,  /*'k'*/
    0x4944,  /*'l'*/
    0x17b6,  /*'m'*/
    0x1ab6,  /*'n'*/
    0x0aaa,  /*'o'*/
    0xd6d3,  /*'p' descender*/
    0x7667,  /*'q' descender*/
    0x1790,  /*'r'*/
    0x0f38,  /*'s'*/
    0x9a8c,  /*'t'*/
    0x16ae,  /*'u'*/
    0x16ba,  /*'v'*/
    0x16f6,  /*'w'*/
    0x15b4,  /*'x'*/
    0xb52b,  /*'y' descender*/
    0x1c5e,  /*'z'*/
    0x6b4c,  /*'{'*/
    0x4948,  /*'|'*/
    0xc95a,  /*'}'*/
    0x5400,  /*'~'*/
    0x56e2   /*DEL*/
};

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

inline int get_font_line(std::uint16_t glyph, int line_num) {
    const std::uint8_t b0 = glyph >> 8;
    const std::uint8_t b1 = glyph & 0xFF;

    if (b1 & 1) {
        line_num -= 1;
    }

    int pixel = 0;
    if (line_num == 0) {
        pixel = b0 >> 4;
    } else if (line_num == 1) {
        pixel = b0 >> 1;
    } else if (line_num == 2) {
        return ((b0 & 0x03) << 2) | (b1 & 0x02);
    } else if (line_num == 3) {
        pixel = b1 >> 4;
    } else if (line_num == 4) {
        pixel = b1 >> 1;
    }
    return pixel & 0xE;
}

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
    const std::uint16_t glyph = font_data[c - 32];

    for (int row = 0; row < FONT_H; ++row) {
        const int py = y + row;
        if (py < 0 || py >= HEIGHT) continue;

        const int pixels = get_font_line(glyph, row);
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
