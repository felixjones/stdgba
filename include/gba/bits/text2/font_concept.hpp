/// @file bits/text2/font_concept.hpp
/// @brief text2 font type concept.

#pragma once

#include <cstdint>

namespace gba::text2 {
    template<typename T>
    concept GlyphFont = requires(const T& font, unsigned int encoding) {
        font.find(encoding);
        font.glyph_or_default(encoding);
        font.bitmap_data(encoding);
        { font.font_width };
        { font.font_height };
        { font.ascent };
        { font.descent };
        { font.line_height() };
        { font.default_char };
        { font.glyph_or_default(encoding).x_offset };
        { font.glyph_or_default(encoding).y_offset };
        { font.glyph_or_default(encoding).width };
        { font.glyph_or_default(encoding).height };
        { font.glyph_or_default(encoding).dwidth };
        { font.glyph_or_default(encoding).bitmap_byte_width };
    };
} // namespace gba::text2
