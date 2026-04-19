/// @file bits/text2/render_layer.hpp
/// @brief text2 main text rendering layer — plane-aware, screenblock-aware.
///
/// Matches the rendering model of gba::text::bg4_text_layer:
///  - Cell state map tracking (vram_tile, plane) per screen cell.
///  - Plane packing: multiple cells share one VRAM tile (mixed-radix encoding).
///  - Screenblock tilemap writes on every newly allocated cell.
///  - Pen/baseline pixel coordinate model with x_offset/y_offset from BDF glyph metrics.
///  - draw_stream() with draw_metrics (letter spacing, line spacing, word wrap).
///  - Color escape gating: only active for one_plane_full_color.
///  - start_index applied via bitplane_config::update_role() on every pixel.
///  - Decoration pass for decorated_font_result (shadow/outline).

#pragma once

#include <gba/video>
#include <gba/bits/text2/glyph_blit.hpp>
#include <gba/bits/text2/stream.hpp>
#include <gba/bits/text2/tile_allocator.hpp>
#include <gba/bits/text2/types.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <variant>

namespace gba::text2 {

    template<typename FontType, unsigned short Width, unsigned short Height>
    struct bg4bpp_text_layer {
    private:
        static constexpr unsigned short tile_grid_width  = (Width  + 7) / 8;
        static constexpr unsigned short tile_grid_height = (Height + 7) / 8;
        static constexpr unsigned short max_tiles        = tile_grid_width * tile_grid_height;
        static constexpr unsigned short map_dim          = 32;

        unsigned short m_screenblock = 31;
        const FontType* m_font = nullptr;
        bitplane_config m_config{};
        linear_tile_allocator m_allocator{};
        linear_tile_allocator m_initial_allocator{};
        std::array<std::uint16_t, max_tiles> m_cell_state{};
        std::uint16_t m_current_vram_tile = no_tile;
        std::uint8_t  m_current_plane     = no_plane;
        tile_plane_cache m_cache{};
        int m_pen_x = 0;
        int m_pen_y = 0;
        unsigned char m_foreground_nibble = static_cast<unsigned char>(bitplane_role::foreground);

        [[nodiscard]]
        static constexpr bool glyph_bit(const unsigned char* bitmap, unsigned short byte_width,
                                        int x, int y) noexcept {
            const auto b = bitmap[static_cast<unsigned>(y) * byte_width + static_cast<unsigned>(x / 8)];
            return ((b >> (x % 8)) & 1u) != 0;
        }

        void draw_pixel(int x, int y, unsigned char role) noexcept {
            if (x < 0 || y < 0) return;
            const auto ux = static_cast<unsigned>(x);
            const auto uy = static_cast<unsigned>(y);
            const auto tx = static_cast<unsigned short>(ux >> 3u);
            const auto ty = static_cast<unsigned short>(uy >> 3u);
            if (tx >= tile_grid_width || ty >= tile_grid_height) return;

            const auto cell_idx = static_cast<unsigned>(ty) * tile_grid_width + tx;
            const bool is_new = (m_cell_state[cell_idx] == 0xFFFFu);
            if (is_new) {
                allocate_cell(cell_idx);
                if (m_cell_state[cell_idx] == 0xFFFFu) return;
            }

            const auto vram_tile = unpack_tile(m_cell_state[cell_idx]);
            const auto plane_idx = unpack_plane(m_cell_state[cell_idx]);
            if (vram_tile == no_tile || plane_idx == no_plane) return;

            if (m_cache.vram_tile != vram_tile) {
                m_cache.flush();
                if (is_new) {
                    m_cache.init_to_background(vram_tile, m_config);
                } else {
                    m_cache.load_from_vram(vram_tile);
                }
            }

            m_cache.set_pixel(static_cast<int>(ux & 7u), static_cast<int>(uy & 7u),
                              plane_idx, role, m_config);
        }

        void allocate_cell(unsigned int cell_idx) noexcept {
            const auto plane_count = profile_plane_count(m_config.profile);
            if (m_current_plane == no_plane || m_current_plane >= plane_count) {
                m_current_vram_tile = m_allocator.allocate(1);
                if (m_current_vram_tile == no_tile) return;
                m_current_plane = 0;
            }

            const auto plane_idx = m_current_plane;
            m_current_plane = static_cast<std::uint8_t>(m_current_plane + 1);
            m_cell_state[cell_idx] = pack_cell_state(m_current_vram_tile, plane_idx);

            const auto ty = static_cast<unsigned short>(cell_idx / tile_grid_width);
            const auto tx = static_cast<unsigned short>(cell_idx % tile_grid_width);
            if (tx >= map_dim || ty >= map_dim) return;

            const auto bank = m_config.bank_for_plane(plane_idx);
            const auto map_idx = static_cast<unsigned>(ty) * map_dim + tx;
            gba::mem_se[m_screenblock][map_idx] = {
                .tile_index    = m_current_vram_tile,
                .palette_index = static_cast<unsigned short>((bank != 255u) ? bank : 0u),
            };
        }

        template<typename Glyph>
        void draw_glyph_bitmap(const unsigned char* bitmap, const Glyph& g,
                               int glyph_x, int glyph_y, unsigned char role) noexcept {
            for (int row = 0; row < g.height; ++row) {
                for (int col = 0; col < g.width; ++col) {
                    if (!glyph_bit(bitmap, g.bitmap_byte_width, col, row)) continue;
                    draw_pixel(glyph_x + col, glyph_y + row, role);
                }
            }
        }

        template<typename Font, typename Glyph>
        void draw_decoration_if_present(const Font& font, const Glyph& g,
                                        int glyph_x, int glyph_y,
                                        unsigned char role) noexcept {
            if constexpr (requires { font.decoration_view_for(g); }) {
                const auto view = font.decoration_view_for(g);
                if (view.empty()) return;
                const int sx = view.x_offset;
                const int ex = sx + static_cast<int>(view.width);
                for (int y = 0; y < static_cast<int>(view.height); ++y) {
                    const auto row_ptr = view.data +
                        static_cast<std::size_t>(view.y_offset + y) * view.byte_width;
                    const int sb = sx / 8;
                    const int eb = (ex - 1) / 8;
                    for (int bi = sb; bi <= eb; ++bi) {
                        const auto bits = row_ptr[bi];
                        if (!bits) continue;
                        for (int bit = 0; bit < 8; ++bit) {
                            if (!((bits >> bit) & 1u)) continue;
                            const int px = bi * 8 + bit;
                            if (px < sx || px >= ex) continue;
                            draw_pixel(glyph_x + px, glyph_y + y, role);
                        }
                    }
                }
            }
        }

    public:
        /// @brief Construct a text layer.
        /// @param screenblock  GBA screenblock index (0-31).
        /// @param font         Font pointer (may be null; set later via set_font).
        /// @param cfg          Bitplane encoding/palette config.
        /// @param allocator    Tile allocator for glyph tiles.
        bg4bpp_text_layer(unsigned short screenblock, const FontType* font,
                          const bitplane_config& cfg,
                          linear_tile_allocator allocator = {.next_tile = 1, .end_tile = 512})
            : m_screenblock(screenblock), m_font(font), m_config(cfg),
              m_allocator(allocator), m_initial_allocator(allocator) {
            clear();
        }

        /// @brief Construct without screenblock (uses default 31, legacy compat).
        bg4bpp_text_layer(const FontType* font, const bitplane_config& cfg)
            : bg4bpp_text_layer(31, font, cfg) {}

        void set_font(const FontType* f) noexcept { m_font = f; }

        void set_palette(unsigned char nibble) noexcept {
            if (nibble > 15) return;
            m_foreground_nibble = nibble;
        }

        /// @brief Reset tilemap, cell state and cursor to initial condition.
        void clear() noexcept {
            m_allocator = m_initial_allocator;
            m_current_vram_tile = no_tile;
            m_current_plane = no_plane;
            m_cache.invalidate();
            m_pen_x = 0;
            m_pen_y = 0;
            m_foreground_nibble = static_cast<unsigned char>(bitplane_role::foreground);
            for (auto& c : m_cell_state) c = 0xFFFFu;
            for (unsigned short ty = 0; ty < tile_grid_height; ++ty) {
                for (unsigned short tx = 0; tx < tile_grid_width; ++tx) {
                    if (tx >= map_dim || ty >= map_dim) continue;
                    const auto idx = static_cast<unsigned>(ty) * map_dim + tx;
                    gba::mem_se[m_screenblock][idx] = {
                        .tile_index = 0,
                        .palette_index = static_cast<unsigned short>(
                            (m_config.palbank_0 != 255u) ? m_config.palbank_0 : 0u),
                    };
                }
            }
        }

        void reset() noexcept { clear(); }

        void flush_cache() noexcept { m_cache.flush(); }

        /// @brief Draw a single character, returning the advance width in pixels.
        ///
        /// Matches the legacy draw_char() placement model:
        ///   glyph_x = pen_x + g.x_offset
        ///   glyph_y = baseline_y - g.height - g.y_offset
        template<typename Font = FontType>
        unsigned short draw_char(const Font& font, unsigned int encoding,
                                  int pen_x, int baseline_y,
                                  unsigned char fg_nibble =
                                      static_cast<unsigned char>(bitplane_role::foreground)) noexcept {
            const auto& g = font.glyph_or_default(encoding);
            const int glyph_x = pen_x + g.x_offset;
            const int glyph_y = baseline_y - static_cast<int>(g.height) - g.y_offset;

            const auto shadow_role = static_cast<unsigned char>(bitplane_role::shadow);
            const auto fg_role = uses_full_color() ? fg_nibble
                                                   : static_cast<unsigned char>(bitplane_role::foreground);

            draw_decoration_if_present(font, g, glyph_x, glyph_y, shadow_role);
            draw_glyph_bitmap(font.bitmap_data(g), g, glyph_x, glyph_y, fg_role);
            return g.dwidth;
        }

        /// @brief Draw a null-terminated string with layout metrics.
        ///
        /// Matches the legacy draw_stream() behaviour:
        ///  - Handles \\n, \\r, word-wrap, letter/line spacing.
        ///  - Color escapes (\\x1B + nibble) only processed in one_plane_full_color mode.
        template<typename Font = FontType>
        std::size_t draw_stream(const Font& font, const char* str, int start_x, int start_y,
                                const stream_metrics& metrics = {},
                                std::size_t max_chars = std::numeric_limits<std::size_t>::max()) noexcept {
            if (!str) return 0;
            int cursor_x = start_x;
            int baseline_y = start_y + font.ascent;
            const auto full_color = uses_full_color();
            auto fg_nibble = static_cast<unsigned char>(bitplane_role::foreground);
            std::size_t emitted = 0;
            bool prev_break = true;

            while (emitted < max_chars && *str) {
                const char ch = *str++;

                if (ch == '\n') {
                    cursor_x = start_x;
                    baseline_y += static_cast<int>(font.line_height() + metrics.line_spacing_px);
                    prev_break = true;
                    ++emitted;
                    continue;
                }
                if (ch == '\r') { prev_break = true; ++emitted; continue; }

                if (full_color && ch == color_escape_prefix) {
                    if (*str) {
                        const auto nibble = static_cast<unsigned char>(*str++) & 0x0Fu;
                        if (nibble != 0) fg_nibble = nibble;
                    }
                    continue;
                }

                const bool is_space = (ch == ' ' || ch == '\t');
                if (!is_space && prev_break && cursor_x != start_x) {
                    const auto word_px = measure_word_px(font, str - 1, metrics);
                    if (cursor_x + word_px > start_x + static_cast<int>(metrics.wrap_width_px)) {
                        cursor_x = start_x;
                        baseline_y += static_cast<int>(font.line_height() + metrics.line_spacing_px);
                    }
                }

                if (ch == ' ') {
                    cursor_x += static_cast<int>(font.glyph_or_default(' ').dwidth +
                                                metrics.letter_spacing_px);
                    prev_break = true;
                    ++emitted;
                    continue;
                }
                if (ch == '\t') {
                    cursor_x += static_cast<int>(metrics.tab_width_px);
                    prev_break = true;
                    ++emitted;
                    continue;
                }

                const auto advance = draw_char(font, static_cast<unsigned char>(ch),
                                               cursor_x, baseline_y, fg_nibble);
                cursor_x += static_cast<int>(advance + metrics.letter_spacing_px);
                prev_break = false;
                ++emitted;
            }

            m_cache.flush();
            m_pen_x = cursor_x;
            m_pen_y = baseline_y - font.ascent;
            return emitted;
        }

        template<typename Font = FontType>
        std::size_t draw_stream(const Font& font, const char* str, int start_x, int start_y,
                                std::size_t max_chars) noexcept {
            return draw_stream(font, str, start_x, start_y, stream_metrics{}, max_chars);
        }

        [[nodiscard]] bool uses_full_color() const noexcept {
            return m_config.profile == bitplane_profile::one_plane_full_color;
        }

        [[nodiscard]] unsigned short cursor_column() const noexcept {
            return static_cast<unsigned short>(m_pen_x < 0 ? 0 : m_pen_x);
        }
        [[nodiscard]] unsigned short cursor_row() const noexcept {
            return static_cast<unsigned short>(m_pen_y < 0 ? 0 : m_pen_y);
        }
        [[nodiscard]] unsigned char palette() const noexcept { return m_foreground_nibble; }

        [[nodiscard]] constexpr std::uint16_t tile_index_from_row_col(
            unsigned short row, unsigned short col) const noexcept {
            return static_cast<std::uint16_t>(row * tile_grid_width + col);
        }
        [[nodiscard]] unsigned short current_tile_count() const noexcept {
            return static_cast<unsigned short>(m_allocator.current() - m_initial_allocator.next_tile);
        }
        [[nodiscard]] constexpr unsigned short max_tile_count() const noexcept { return max_tiles; }

        [[nodiscard]] unsigned char plane_count() const noexcept { return m_config.plane_count(); }
        [[nodiscard]] unsigned char role_count()  const noexcept { return m_config.role_count(); }

    private:
        template<typename Font>
        int measure_word_px(const Font& font, const char* p, const stream_metrics& metrics) noexcept {
            int w = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                if (*p == color_escape_prefix && *(p + 1)) { p += 2; continue; }
                w += static_cast<int>(font.glyph_or_default(static_cast<unsigned char>(*p)).dwidth +
                                      metrics.letter_spacing_px);
                ++p;
            }
            return w;
        }
    };

    template<typename FontType, unsigned short Width, unsigned short Height>
    using bg4_text_layer = bg4bpp_text_layer<FontType, Width, Height>;

} // namespace gba::text2
