/// @file bits/embed/bdf_types.hpp
/// @brief Internal BDF font result types for gba::embed.
#pragma once

#include <gba/bios>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::embed {

    /// @brief Compile-time BDF font result with glyph metadata and packed 1bpp bitmap rows.
    /// @tparam GlyphCount Number of parsed glyphs.
    /// @tparam BitmapBytes Total number of packed bitmap bytes across all glyphs.
    template<std::size_t GlyphCount, std::size_t BitmapBytes>
    struct bdf_font_result {
        static constexpr std::size_t glyph_count = GlyphCount;
        static constexpr std::size_t bitmap_size = BitmapBytes;
        static constexpr unsigned int encoding_none = 0xFFFFFFFFu;

        /// @brief Per-glyph metadata and offset into the shared bitmap buffer.
        struct glyph {
            unsigned int encoding{};
            unsigned short dwidth{};
            unsigned short width{};
            unsigned short height{};
            short x_offset{};
            short y_offset{};
            unsigned int bitmap_offset{};
            unsigned short bitmap_byte_width{};

            /// @brief Number of packed 1bpp source bytes for this glyph.
            [[nodiscard]]
            constexpr unsigned int bitmap_bytes() const noexcept {
                return static_cast<unsigned int>(bitmap_byte_width) * height;
            }

            /// @brief Build a BIOS BitUnPack header for this glyph's 1bpp bitmap rows.
            /// @param dst_bpp Destination bit depth.
            /// @param dst_ofs Palette/value offset for non-zero pixels.
            /// @param offset_zero Whether zero pixels should also receive the offset.
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

        unsigned short font_width{};
        unsigned short font_height{};
        short font_x{};
        short font_y{};
        unsigned short ascent{};
        unsigned short descent{};
        unsigned int default_char{};
        std::array<glyph, GlyphCount> glyphs{};
        alignas(unsigned int) std::array<unsigned char, BitmapBytes> bitmap{};

        /// @brief Total nominal line height from ascent and descent.
        [[nodiscard]]
        constexpr unsigned short line_height() const noexcept {
            return static_cast<unsigned short>(ascent + descent);
        }

        /// @brief Find a glyph by encoding.
        [[nodiscard]]
        constexpr const glyph* find(unsigned int encoding) const noexcept {
            for (const auto& g : glyphs) {
                if (g.encoding == encoding) return &g;
            }
            return nullptr;
        }

        /// @brief Find a glyph by encoding, falling back to DEFAULT_CHAR then glyph 0.
        [[nodiscard]]
        constexpr const glyph& glyph_or_default(unsigned int encoding) const noexcept {
            if (const auto* g = find(encoding)) return *g;
            if (const auto* g = find(default_char)) return *g;
            return glyphs[0];
        }

        /// @brief Get the packed bitmap pointer for a glyph.
        [[nodiscard]]
        constexpr const unsigned char* bitmap_data(const glyph& g) const noexcept {
            return bitmap.data() + g.bitmap_offset;
        }

        /// @brief Get the packed bitmap pointer for an encoding, with default fallback.
        [[nodiscard]]
        constexpr const unsigned char* bitmap_data(unsigned int encoding) const noexcept {
            return bitmap_data(glyph_or_default(encoding));
        }

        /// @brief Build a BIOS BitUnPack header for an encoding, with default fallback.
        [[nodiscard]]
        constexpr gba::unpack_parameters bitunpack_header(unsigned int encoding, unsigned char dst_bpp = 4,
                                                          unsigned int dst_ofs = 1,
                                                          bool offset_zero = false) const noexcept {
            return glyph_or_default(encoding).bitunpack_header(dst_bpp, dst_ofs, offset_zero);
        }
    };

} // namespace gba::embed
