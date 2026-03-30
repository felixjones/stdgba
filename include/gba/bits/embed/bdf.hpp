/// @file bits/embed/bdf.hpp
/// @brief Compile-time BDF embedding entry point for gba::embed.
#pragma once

#include <gba/bits/embed/bdf_parse.hpp>

#include <type_traits>

namespace gba::embed {

    /// @brief Convert a BDF bitmap font into glyph metadata and 1bpp bitmap data.
    ///
    /// Glyph rows are repacked into row-major 1bpp bytes with the leftmost pixel
    /// in bit 0 so BIOS `BitUnPack` can expand them directly into higher bit depths.
    ///
    /// Example:
    /// @code{.cpp}
    /// static constexpr auto font = gba::embed::bdf([] {
    ///     return std::to_array<unsigned char>({
    /// #embed "9x18.bdf"
    ///     });
    /// });
    ///
    /// const auto& glyph = font.glyph_or_default('A');
    /// gba::BitUnPack(font.bitmap_data(glyph), dest, glyph.bitunpack_header());
    /// @endcode
    consteval auto bdf(auto supplier) {
        constexpr auto raw = supplier();
        constexpr auto layout = bits::parse_bdf_layout(raw);

        return [&]<std::size_t GlyphCount, std::size_t BitmapBytes>(
                   std::integral_constant<std::size_t, GlyphCount>,
                   std::integral_constant<std::size_t, BitmapBytes>) consteval {
            return bits::build_bdf_font<GlyphCount, BitmapBytes>(raw, layout.header);
        }(std::integral_constant<std::size_t, static_cast<std::size_t>(layout.header.glyph_count)>{},
               std::integral_constant<std::size_t, layout.bitmap_bytes>{});
    }

} // namespace gba::embed
