/// @file bits/font_3x5.hpp
/// @brief Shared 3x5 bitmap font (internal header).
///
/// This is an internal header shared by crash_screen and shapes modules.
/// The 3x5 font is MIT-licensed and sourced from:
/// https://hackaday.io/project/6309-vga-graphics-over-spi-and-serial-vgatonic
#pragma once

#include <cstdint>

namespace gba::bits {

    /// 96 glyphs for ASCII 32-127 (3 pixels wide x 5 pixels tall).
    /// Each glyph is 16 bits. Bit 0 of the lower byte indicates a descender.
    inline constexpr std::uint16_t font_3x5_data[96] = {
        0x0000, 0x4908, 0xb400, 0xbef6, 0x7b7a, 0xa594, 0x55b8, 0x4800, 0x2944, 0x442a, 0x15a0, 0x0b42, 0x0050, 0x0302,
        0x0008, 0x2590, 0x76ba, 0x595c, 0xc59e, 0xc538, 0x92e6, 0xf33a, 0x73ba, 0xe590, 0x77ba, 0x773a, 0x0840, 0x0850,
        0x2a44, 0x1ce0, 0x8852, 0xe508, 0x568e, 0x77b6, 0x77b8, 0x728c, 0xd6ba, 0x739e, 0x7392, 0x72ae, 0xb7b6, 0xe95c,
        0x64aa, 0xb7b4, 0x929c, 0xbeb6, 0xd6b6, 0x56aa, 0xd792, 0x76ee, 0x77b4, 0x7138, 0xe948, 0xb6ae, 0xb6aa, 0xb6f6,
        0xb5b4, 0xb548, 0xe59c, 0x694c, 0x9124, 0x642e, 0x5400, 0x001c, 0x4400, 0x0eae, 0x9aba, 0x0e8c, 0x2eae, 0x0ece,
        0x56d0, 0x553B, 0x93b4, 0x4144, 0x4151, 0x97b4, 0x4944, 0x17b6, 0x1ab6, 0x0aaa, 0xd6d3, 0x7667, 0x1790, 0x0f38,
        0x9a8c, 0x16ae, 0x16ba, 0x16f6, 0x15b4, 0xb52b, 0x1c5e, 0x6b4c, 0x4948, 0xc95a, 0x5400, 0x56e2};

    /// @brief Get the pixel mask for a specific line of a glyph (consteval helper).
    constexpr int get_font_line(std::uint16_t glyph, int line_num) {
        const std::uint8_t b0 = glyph >> 8;
        const std::uint8_t b1 = glyph & 0xFF;

        if (b1 & 1) line_num -= 1;

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

} // namespace gba::bits
