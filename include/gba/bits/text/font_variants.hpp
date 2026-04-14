/// @file bits/text/font_variants.hpp
/// @brief Compile-time font variant generation (shadow, outline).
///
/// Transforms a base BDF font into variants with pre-baked effects.
/// Shadow and outline pixels are baked into the glyph bitmaps at compile time,
/// eliminating runtime overhead.
#pragma once

#include <gba/bits/embed/bdf_types.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace gba::text::bits {

    struct expanded_glyph_info {
        unsigned short width{};
        unsigned short height{};
        short x_offset{};
        short y_offset{};
        unsigned short bitmap_byte_width{};
        std::size_t bitmap_offset{};
    };

    consteval expanded_glyph_info expand_bounds_for_shadow(
        const auto& old_g, int shadow_dx, int shadow_dy, std::size_t& total_offset) {
        const int min_x = std::min(0, shadow_dx);
        const int max_x = std::max(static_cast<int>(old_g.width), static_cast<int>(old_g.width) + shadow_dx);
        const int min_y = std::min(0, shadow_dy);
        const int max_y = std::max(static_cast<int>(old_g.height), static_cast<int>(old_g.height) + shadow_dy);
        const int new_w = max_x - min_x;
        const int new_h = max_y - min_y;
        if (new_w <= 0 || new_h <= 0) throw "text font variant: invalid shadow-expanded glyph size";

        const auto byte_width = static_cast<std::size_t>((new_w + 7) / 8);
        const auto bitmap_offset = total_offset;
        total_offset += byte_width * static_cast<std::size_t>(new_h);

        return {
            .width = static_cast<unsigned short>(new_w),
            .height = static_cast<unsigned short>(new_h),
            .x_offset = static_cast<short>(old_g.x_offset + min_x),
            .y_offset = static_cast<short>(old_g.y_offset + min_y),
            .bitmap_byte_width = static_cast<unsigned short>(byte_width),
            .bitmap_offset = bitmap_offset,
        };
    }

    consteval expanded_glyph_info expand_bounds_for_outline(
        const auto& old_g, int thickness, std::size_t& total_offset) {
        const int new_w = static_cast<int>(old_g.width) + 2 * thickness;
        const int new_h = static_cast<int>(old_g.height) + 2 * thickness;
        if (new_w <= 0 || new_h <= 0) throw "text font variant: invalid outline-expanded glyph size";

        const auto byte_width = static_cast<std::size_t>((new_w + 7) / 8);
        const auto bitmap_offset = total_offset;
        total_offset += byte_width * static_cast<std::size_t>(new_h);

        return {
            .width = static_cast<unsigned short>(new_w),
            .height = static_cast<unsigned short>(new_h),
            .x_offset = static_cast<short>(old_g.x_offset - thickness),
            .y_offset = static_cast<short>(old_g.y_offset - thickness),
            .bitmap_byte_width = static_cast<unsigned short>(byte_width),
            .bitmap_offset = bitmap_offset,
        };
    }

    consteval bool read_glyph_bit(const unsigned char* bitmap, unsigned short byte_width,
                                  unsigned short width, unsigned short height, int x, int y) {
        if (x < 0 || y < 0) return false;
        if (x >= static_cast<int>(width) || y >= static_cast<int>(height)) return false;
        const auto byte_idx = static_cast<std::size_t>(y) * byte_width + static_cast<unsigned int>(x / 8);
        return ((bitmap[byte_idx] >> (x % 8)) & 1u) != 0;
    }

    consteval void write_glyph_bit(unsigned char* bitmap, unsigned short byte_width, unsigned short width,
                                   unsigned short height, int x, int y) {
        if (x < 0 || y < 0) return;
        if (x >= static_cast<int>(width) || y >= static_cast<int>(height)) return;

        const auto byte_idx = static_cast<std::size_t>(y) * byte_width + static_cast<unsigned int>(x / 8);
        bitmap[byte_idx] |= static_cast<unsigned char>(1u << (x % 8));
    }

    consteval void prune_decoration_overlap(unsigned char* decoration_bitmap, const unsigned char* foreground_bitmap,
                                            std::size_t bytes) {
        for (std::size_t i = 0; i < bytes; ++i) {
            decoration_bitmap[i] = static_cast<unsigned char>(decoration_bitmap[i] & ~foreground_bitmap[i]);
        }
    }

    struct decoration_bounds {
        unsigned short width{};
        unsigned short height{};
        short x_offset{};
        short y_offset{};
    };

    consteval decoration_bounds find_decoration_bounds(const unsigned char* bitmap, unsigned short byte_width,
                                                       unsigned short width, unsigned short height) {
        int min_x = static_cast<int>(width);
        int min_y = static_cast<int>(height);
        int max_x = -1;
        int max_y = -1;

        for (int y = 0; y < static_cast<int>(height); ++y) {
            for (int x = 0; x < static_cast<int>(width); ++x) {
                if (!read_glyph_bit(bitmap, byte_width, width, height, x, y)) continue;
                if (x < min_x) min_x = x;
                if (y < min_y) min_y = y;
                if (x > max_x) max_x = x;
                if (y > max_y) max_y = y;
            }
        }

        if (max_x < min_x || max_y < min_y) return {};
        return {
            .width = static_cast<unsigned short>(max_x - min_x + 1),
            .height = static_cast<unsigned short>(max_y - min_y + 1),
            .x_offset = static_cast<short>(min_x),
            .y_offset = static_cast<short>(min_y),
        };
    }

} // namespace gba::text::bits

namespace gba::text {

    /// @brief Generate a shadow variant of a font.
    ///
    /// Creates a new font where each glyph bitmap includes shadow pixels
    /// offset by ShadowDX pixels right and ShadowDY pixels down.
    /// The output glyph bounding boxes are expanded to fit both the foreground and shadow.
    ///
    /// @tparam Font Base font type (bdf_font_result).
    /// @tparam ShadowDX Horizontal shadow offset in pixels.
    /// @tparam ShadowDY Vertical shadow offset in pixels.
    /// @param base_font The source font.
    /// @return A new font with expanded glyphs containing shadow.
    ///
    /// Example (compile-time):
    /// @code{.cpp}
    /// static constexpr auto font = gba::embed::bdf([]{
    ///     return std::to_array<unsigned char>({#embed "font.bdf"});
    /// });
    ///
    /// // Create a shadowed variant (1px right, 1px down) at compile time
    /// static constexpr auto font_shadowed = gba::text::with_shadow<1, 1>(font);
    ///
    /// // Use in render code - no runtime effect overhead
    /// layer.draw_stream(font_shadowed, ...);
    /// @endcode
    template<typename FontType>
    struct decorated_font_result {
        static constexpr std::size_t glyph_count = FontType::glyph_count;
        static constexpr std::size_t bitmap_capacity = FontType::bitmap_size * 4 + FontType::glyph_count * 64 + 64;
        struct glyph {
            unsigned int encoding{};
            unsigned short dwidth{};
            unsigned short width{};
            unsigned short height{};
            short x_offset{};
            short y_offset{};
            unsigned int bitmap_offset{};
            unsigned short bitmap_byte_width{};
            unsigned int decoration_offset{};
            unsigned short decoration_width{};
            unsigned short decoration_height{};
            short decoration_x_offset{};
            short decoration_y_offset{};
            unsigned short decoration_byte_width{};

            [[nodiscard]]
            constexpr unsigned int bitmap_bytes() const noexcept {
                return static_cast<unsigned int>(bitmap_byte_width) * height;
            }

            [[nodiscard]]
            constexpr gba::unpack_parameters bitunpack_header(unsigned char dst_bpp = 4, unsigned int dst_ofs = 1,
                                                              bool offset_zero = false) const noexcept {
                return {.src_len = static_cast<unsigned short>(bitmap_bytes()),
                        .src_bpp = 1,
                        .dst_bpp = dst_bpp,
                        .dst_ofs = dst_ofs,
                        .offset_zero = offset_zero};
            }
        };

        struct decoration_view {
            const unsigned char* data{};
            unsigned short width{};
            unsigned short height{};
            short x_offset{};
            short y_offset{};
            unsigned short byte_width{};

            [[nodiscard]]
            constexpr bool empty() const noexcept {
                return data == nullptr || width == 0 || height == 0;
            }
        };

        unsigned short font_width{};
        unsigned short font_height{};
        short font_x{};
        short font_y{};
        unsigned short ascent{};
        unsigned short descent{};
        unsigned int default_char{};
        std::array<glyph, glyph_count> glyphs{};
        alignas(unsigned int) std::array<unsigned char, bitmap_capacity> bitmap{};
        alignas(unsigned int) std::array<unsigned char, bitmap_capacity> decoration_bitmap{};

        constexpr unsigned short line_height() const noexcept {
            return static_cast<unsigned short>(ascent + descent);
        }

        constexpr const glyph* find(unsigned int encoding) const noexcept {
            for (const auto& g : glyphs) {
                if (g.encoding == encoding) return &g;
            }
            return nullptr;
        }

        constexpr const glyph& glyph_or_default(unsigned int encoding) const noexcept {
            if (const auto* g = find(encoding)) return *g;
            if (const auto* g = find(default_char)) return *g;
            return glyphs[0];
        }

        constexpr const unsigned char* bitmap_data(const glyph& g) const noexcept {
            return bitmap.data() + g.bitmap_offset;
        }

        constexpr const unsigned char* bitmap_data(unsigned int encoding) const noexcept {
            return bitmap_data(glyph_or_default(encoding));
        }

        constexpr const unsigned char* decoration_bitmap_data(const glyph& g) const noexcept {
            return decoration_bitmap.data() + g.decoration_offset;
        }

        constexpr const unsigned char* decoration_bitmap_data(unsigned int encoding) const noexcept {
            return decoration_bitmap_data(glyph_or_default(encoding));
        }

        constexpr decoration_view decoration_view_for(const glyph& g) const noexcept {
            return {
                .data = g.decoration_width == 0 ? nullptr : decoration_bitmap.data() + g.decoration_offset,
                .width = g.decoration_width,
                .height = g.decoration_height,
                .x_offset = g.decoration_x_offset,
                .y_offset = g.decoration_y_offset,
                .byte_width = g.decoration_byte_width,
            };
        }

        constexpr gba::unpack_parameters bitunpack_header(
            unsigned int encoding, unsigned char dst_bpp = 4, unsigned int dst_ofs = 1,
            bool offset_zero = false) const noexcept {
            return glyph_or_default(encoding).bitunpack_header(dst_bpp, dst_ofs, offset_zero);
        }
    };

    template<int ShadowDX, int ShadowDY, typename FontType>
    consteval auto with_shadow(const FontType& base_font) {
        if constexpr (ShadowDX == 0 && ShadowDY == 0) throw "text font variant: shadow offset cannot be 0,0";

        using bits::expand_bounds_for_shadow;
        using bits::read_glyph_bit;
        using bits::write_glyph_bit;
        using result_type = decorated_font_result<FontType>;
        constexpr std::size_t GlyphCount = FontType::glyph_count;

        result_type result{
            .font_width = base_font.font_width,
            .font_height = base_font.font_height,
            .font_x = base_font.font_x,
            .font_y = base_font.font_y,
            .ascent = base_font.ascent,
            .descent = base_font.descent,
            .default_char = base_font.default_char,
        };

        // Build new glyph metadata
        std::size_t bitmap_offset = 0;
        for (std::size_t i = 0; i < GlyphCount; ++i) {
            const auto& old_g = base_font.glyphs[i];
            auto& new_g = result.glyphs[i];

            const auto expanded = expand_bounds_for_shadow(old_g, ShadowDX, ShadowDY, bitmap_offset);
            if (bitmap_offset > result_type::bitmap_capacity) throw "text font variant: shadow bitmap capacity exceeded";

            new_g.encoding = old_g.encoding;
            new_g.dwidth = old_g.dwidth;
            new_g.width = expanded.width;
            new_g.height = expanded.height;
            new_g.x_offset = expanded.x_offset;
            new_g.y_offset = expanded.y_offset;
            new_g.bitmap_offset = expanded.bitmap_offset;
            new_g.bitmap_byte_width = expanded.bitmap_byte_width;
            new_g.decoration_offset = expanded.bitmap_offset;
            new_g.decoration_byte_width = expanded.bitmap_byte_width;
        }
        // Rasterize shadow glyphs into separate decoration and foreground masks.
        for (std::size_t gi = 0; gi < GlyphCount; ++gi) {
            const auto& old_g = base_font.glyphs[gi];
            auto& new_g = result.glyphs[gi];
            const auto* old_bitmap = base_font.bitmap_data(old_g);
            auto* fg_bitmap = result.bitmap.data() + new_g.bitmap_offset;
            auto* deco_bitmap = result.decoration_bitmap.data() + new_g.bitmap_offset;

            const int min_x = std::min(0, ShadowDX);
            const int min_y = std::min(0, ShadowDY);
            const int fg_x_in_new = -min_x;
            const int fg_y_in_new = -min_y;
            const int shadow_x_in_new = fg_x_in_new + ShadowDX;
            const int shadow_y_in_new = fg_y_in_new + ShadowDY;

            for (int oy = 0; oy < static_cast<int>(old_g.height); ++oy) {
                for (int ox = 0; ox < static_cast<int>(old_g.width); ++ox) {
                    if (!read_glyph_bit(old_bitmap, old_g.bitmap_byte_width, old_g.width, old_g.height, ox, oy)) continue;

                    write_glyph_bit(fg_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height,
                                    ox + fg_x_in_new, oy + fg_y_in_new);

                    if (read_glyph_bit(old_bitmap, old_g.bitmap_byte_width, old_g.width, old_g.height,
                                       ox + ShadowDX, oy + ShadowDY)) continue;

                    write_glyph_bit(deco_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height,
                                    ox + shadow_x_in_new, oy + shadow_y_in_new);
                }
            }

            bits::prune_decoration_overlap(deco_bitmap, fg_bitmap,
                                           static_cast<std::size_t>(new_g.bitmap_byte_width) * new_g.height);

            const auto bounds = bits::find_decoration_bounds(deco_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height);
            new_g.decoration_width = bounds.width;
            new_g.decoration_height = bounds.height;
            new_g.decoration_x_offset = bounds.x_offset;
            new_g.decoration_y_offset = bounds.y_offset;
        }

        return result;
    }

    template<int OutlineThickness, typename FontType>
    consteval auto with_outline(const FontType& base_font) {
        if constexpr (OutlineThickness <= 0) throw "text font variant: outline thickness must be positive";

        using bits::expand_bounds_for_outline;
        using bits::read_glyph_bit;
        using bits::write_glyph_bit;
        using result_type = decorated_font_result<FontType>;
        constexpr std::size_t GlyphCount = FontType::glyph_count;

        result_type result{
            .font_width = base_font.font_width,
            .font_height = base_font.font_height,
            .font_x = base_font.font_x,
            .font_y = base_font.font_y,
            .ascent = base_font.ascent,
            .descent = base_font.descent,
            .default_char = base_font.default_char,
        };

        std::size_t bitmap_offset = 0;
        for (std::size_t i = 0; i < GlyphCount; ++i) {
            const auto& old_g = base_font.glyphs[i];
            auto& new_g = result.glyphs[i];
            const auto expanded = expand_bounds_for_outline(old_g, OutlineThickness, bitmap_offset);
            if (bitmap_offset > result_type::bitmap_capacity) throw "text font variant: outline bitmap capacity exceeded";

            new_g.encoding = old_g.encoding;
            new_g.dwidth = old_g.dwidth;
            new_g.width = expanded.width;
            new_g.height = expanded.height;
            new_g.x_offset = expanded.x_offset;
            new_g.y_offset = expanded.y_offset;
            new_g.bitmap_offset = expanded.bitmap_offset;
            new_g.bitmap_byte_width = expanded.bitmap_byte_width;
            new_g.decoration_offset = expanded.bitmap_offset;
            new_g.decoration_byte_width = expanded.bitmap_byte_width;
        }

        for (std::size_t gi = 0; gi < GlyphCount; ++gi) {
            const auto& old_g = base_font.glyphs[gi];
            auto& new_g = result.glyphs[gi];
            const auto* old_bitmap = base_font.bitmap_data(old_g);
            auto* fg_bitmap = result.bitmap.data() + new_g.bitmap_offset;
            auto* deco_bitmap = result.decoration_bitmap.data() + new_g.bitmap_offset;

            for (int oy = 0; oy < static_cast<int>(old_g.height); ++oy) {
                for (int ox = 0; ox < static_cast<int>(old_g.width); ++ox) {
                    if (!read_glyph_bit(old_bitmap, old_g.bitmap_byte_width, old_g.width, old_g.height, ox, oy)) continue;

                    for (int dy = -OutlineThickness; dy <= OutlineThickness; ++dy) {
                        for (int dx = -OutlineThickness; dx <= OutlineThickness; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            if (read_glyph_bit(old_bitmap, old_g.bitmap_byte_width, old_g.width, old_g.height,
                                               ox + dx, oy + dy)) continue;
                            write_glyph_bit(deco_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height,
                                            ox + OutlineThickness + dx, oy + OutlineThickness + dy);
                        }
                    }

                    write_glyph_bit(fg_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height,
                                    ox + OutlineThickness, oy + OutlineThickness);
                }
            }

            bits::prune_decoration_overlap(deco_bitmap, fg_bitmap,
                                           static_cast<std::size_t>(new_g.bitmap_byte_width) * new_g.height);

            const auto bounds = bits::find_decoration_bounds(deco_bitmap, new_g.bitmap_byte_width, new_g.width, new_g.height);
            new_g.decoration_width = bounds.width;
            new_g.decoration_height = bounds.height;
            new_g.decoration_x_offset = bounds.x_offset;
            new_g.decoration_y_offset = bounds.y_offset;
        }

        return result;
    }

} // namespace gba::text
