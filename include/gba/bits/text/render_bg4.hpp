/// @file bits/text/render_bg4.hpp
/// @brief BG 4bpp bitplane text rendering with nibble merge logic.
#pragma once

#include <gba/bits/text/types.hpp>
#include <gba/video>

#include <array>
#include <cstddef>
#include <limits>

namespace gba::text {

    static constexpr unsigned short no_tile = 0xFFFFu;
    static constexpr unsigned char no_plane = 255u;

    struct glyph_style {
        bool draw_shadow = false;
        int shadow_dx = 1;
        int shadow_dy = 1;
    };

    struct draw_metrics {
        unsigned short letter_spacing_px = 0;
        unsigned short line_spacing_px = 0;
        unsigned short wrap_width_px = 240;
        break_policy break_chars = break_policy::whitespace;
    };

    struct linear_tile_allocator {
        unsigned short next_tile = 0;
        unsigned short end_tile = 1024;

        constexpr unsigned short allocate(unsigned short count) noexcept {
            if (count == 0 || next_tile + count > end_tile) return no_tile;
            const auto base = next_tile;
            next_tile = static_cast<unsigned short>(next_tile + count);
            return base;
        }
    };

    template<unsigned short WidthTiles = 32, unsigned short HeightTiles = 32,
             typename Allocator = linear_tile_allocator>
    class bg4_text_layer;

    /// @brief Incremental character-by-character drawing cursor.
    template<typename Layer, typename Font, typename Stream>
    class draw_cursor {
    public:
        constexpr draw_cursor(Layer& layer, const Font& font, Stream stream, int start_x, int start_y,
                              draw_metrics metrics, glyph_style style)
            : m_layer(&layer), m_font(&font), m_stream(static_cast<Stream&&>(stream)), m_startX(start_x),
              m_cursorX(start_x), m_baselineY(start_y + font.ascent), m_metrics(metrics), m_style(style) {}

        /// @brief Draw the next character. Returns true if a character was drawn.
        bool next() {
            if (m_done) return false;

            auto ch = m_stream.next();
            if (!ch) {
                m_done = true;
                return false;
            }

            if (*ch == '\n') {
                m_cursorX = m_startX;
                m_baselineY += static_cast<int>(m_font->line_height() + m_metrics.line_spacing_px);
                m_prevBreak = true;
                ++m_emitted;
                return true;
            }

            if (*ch == '\r') {
                m_prevBreak = true;
                ++m_emitted;
                return true;
            }

            const bool breakChar = is_break_char(*ch, m_metrics.break_chars);
            if (!breakChar && m_prevBreak && m_cursorX != m_startX) {
                const auto wordWidth = static_cast<int>(m_stream.until_break_px(m_metrics.break_chars));
                if (m_cursorX + wordWidth > m_startX + static_cast<int>(m_metrics.wrap_width_px)) {
                    m_cursorX = m_startX;
                    m_baselineY += static_cast<int>(m_font->line_height() + m_metrics.line_spacing_px);
                }
            }

            if (*ch == ' ') {
                const auto& s = m_font->glyph_or_default(' ');
                m_cursorX += static_cast<int>(s.dwidth + m_metrics.letter_spacing_px);
                m_prevBreak = true;
                ++m_emitted;
                return true;
            }

            if (*ch == '\t') {
                m_cursorX += 16;
                m_prevBreak = true;
                ++m_emitted;
                return true;
            }

            const auto advance = m_layer->draw_char(*m_font, static_cast<unsigned char>(*ch), m_cursorX, m_baselineY,
                                                    m_style);
            m_cursorX += static_cast<int>(advance + m_metrics.letter_spacing_px);
            m_prevBreak = breakChar;
            ++m_emitted;
            return true;
        }

        /// @brief Draw the next visible glyph, skipping whitespace and control characters.
        ///
        /// Advances through spaces/tabs/newlines/CRs in the same call so a typewriter
        /// effect does not waste a frame on empty output.
        ///
        /// Returns true if a non-whitespace glyph was drawn, false if the stream was
        /// exhausted.
        bool next_visible() {
            while (true) {
                if (m_done) return false;

                auto ch = m_stream.next();
                if (!ch) {
                    m_done = true;
                    return false;
                }

                if (*ch == '\n') {
                    m_cursorX = m_startX;
                    m_baselineY += static_cast<int>(m_font->line_height() + m_metrics.line_spacing_px);
                    m_prevBreak = true;
                    ++m_emitted;
                    continue;
                }

                if (*ch == '\r') {
                    m_prevBreak = true;
                    ++m_emitted;
                    continue;
                }

                const bool breakChar = is_break_char(*ch, m_metrics.break_chars);
                if (!breakChar && m_prevBreak && m_cursorX != m_startX) {
                    const auto wordWidth = static_cast<int>(m_stream.until_break_px(m_metrics.break_chars));
                    if (m_cursorX + wordWidth > m_startX + static_cast<int>(m_metrics.wrap_width_px)) {
                        m_cursorX = m_startX;
                        m_baselineY += static_cast<int>(m_font->line_height() + m_metrics.line_spacing_px);
                    }
                }

                if (*ch == ' ') {
                    const auto& s = m_font->glyph_or_default(' ');
                    m_cursorX += static_cast<int>(s.dwidth + m_metrics.letter_spacing_px);
                    m_prevBreak = true;
                    ++m_emitted;
                    continue;
                }

                if (*ch == '\t') {
                    m_cursorX += 16;
                    m_prevBreak = true;
                    ++m_emitted;
                    continue;
                }

                const auto advance = m_layer->draw_char(*m_font, static_cast<unsigned char>(*ch), m_cursorX,
                                                        m_baselineY, m_style);
                m_cursorX += static_cast<int>(advance + m_metrics.letter_spacing_px);
                m_prevBreak = breakChar;
                ++m_emitted;
                return true;
            }
        }

        /// @brief Shorthand for `next()`.
        bool operator()() { return next(); }

        [[nodiscard]] std::size_t emitted() const { return m_emitted; }
        [[nodiscard]] bool done() const { return m_done; }

    private:
        Layer* m_layer;
        const Font* m_font;
        Stream m_stream;
        int m_startX;
        int m_cursorX;
        int m_baselineY;
        draw_metrics m_metrics;
        glyph_style m_style;
        std::size_t m_emitted = 0;
        bool m_done = false;
        bool m_prevBreak = true;
    };

    /// @brief Encode (vram_tile, plane_index) into a cell state word.
    [[nodiscard]]
    constexpr unsigned short pack_cell_state(unsigned short vram_tile, unsigned char plane) noexcept {
        if (vram_tile == no_tile) return 0xFFFFu;
        return static_cast<unsigned short>((vram_tile & 0x3FFu) | ((plane & 0x3u) << 10));
    }

    /// @brief Decode VRAM tile from cell state.
    [[nodiscard]]
    constexpr unsigned short unpack_tile(unsigned short state) noexcept {
        if (state == 0xFFFFu) return no_tile;
        return state & 0x3FFu;
    }

    /// @brief Decode plane index from cell state.
    [[nodiscard]]
    constexpr unsigned char unpack_plane(unsigned short state) noexcept {
        if (state == 0xFFFFu) return no_plane;
        return static_cast<unsigned char>((state >> 10) & 0x3u);
    }

    /// @brief BG4 bitplane text rendering layer with nibble merge logic.
    template<unsigned short WidthTiles, unsigned short HeightTiles, typename Allocator>
    class bg4_text_layer {
    public:
        /// @brief Construct a text layer with bitplane config and tile allocator.
        ///
        /// Uses a single background tile allocated at the time of clear().
        /// Plane allocations fill the current VRAM tile before requesting new tiles.
        ///
        /// @param screenblock Screenblock index (0-31).
        /// @param config Bitplane encoding/palette config.
        /// @param allocator Tile allocator for glyph tiles.
        /// @param start_tile_x Region origin in tile coordinates (0-31).
        /// @param start_tile_y Region origin in tile coordinates (0-31).
        ///
        /// Notes:
        /// - WidthTiles/HeightTiles describe the region size, not the full map size.
        /// - Pixel coordinates passed to draw_char/draw_stream are relative to the
        ///   region origin.
        constexpr bg4_text_layer(unsigned short screenblock, const bitplane_config& config, Allocator allocator,
                                 unsigned short start_tile_x = 0, unsigned short start_tile_y = 0)
            : m_screenblock(screenblock), m_config(config), m_allocator(allocator), m_initial_allocator(allocator),
              m_start_tile_x(start_tile_x), m_start_tile_y(start_tile_y) {
            clear();
        }

        /// @brief clear() and re-initialize tilemap and bitplane state.
        void clear() {
            m_allocator = m_initial_allocator;
            m_current_vram_tile = no_tile;
            m_current_plane = no_plane;

            // Clear cell state LUT
            for (auto& cell : m_cell_state) {
                cell = 0xFFFFu;
            }

            // Fill tilemap with tile 0 (background).
            // Tile 0 is assumed to be the default background or user-controlled.
            // When glyphs are rendered, cells are allocated to new tiles starting from next_tile.
            for (unsigned short ty = 0; ty < HeightTiles; ++ty) {
                for (unsigned short tx = 0; tx < WidthTiles; ++tx) {
                    const auto map_x = static_cast<unsigned short>(m_start_tile_x + tx);
                    const auto map_y = static_cast<unsigned short>(m_start_tile_y + ty);
                    if (map_x >= map_width_tiles || map_y >= map_height_tiles) continue;

                    const auto idx = static_cast<unsigned int>(map_y) * map_width_tiles + map_x;
                    gba::mem_se[m_screenblock][idx] = {
                        .tile_index = 0,
                        .palette_index = static_cast<unsigned short>((m_config.palbank_0 != 255u) ? m_config.palbank_0
                                                                                                  : 0u),
                    };
                }
            }
        }

        /// @brief Draw a single character using bitplane rendering.
        template<typename Font>
        unsigned short draw_char(const Font& font, unsigned int encoding, int pen_x, int baseline_y,
                                 const glyph_style& style = {}) {
            const auto& g = font.glyph_or_default(encoding);

            const int glyph_x = pen_x + g.x_offset;
            const int glyph_y = baseline_y - static_cast<int>(g.height) - g.y_offset;

            if (style.draw_shadow) {
                draw_glyph_bitmap(font, g, glyph_x + style.shadow_dx, glyph_y + style.shadow_dy,
                                  static_cast<unsigned char>(bitplane_role::shadow));
            }
            draw_glyph_bitmap(font, g, glyph_x, glyph_y, static_cast<unsigned char>(bitplane_role::foreground));
            return g.dwidth;
        }

        /// @brief Draw a stream of characters with text layout.
        template<typename Font, typename Stream>
        std::size_t draw_stream(const Font& font, Stream& stream, int start_x, int start_y, draw_metrics metrics,
                                glyph_style style = {},
                                std::size_t max_chars = std::numeric_limits<std::size_t>::max()) {
            int cursor_x = start_x;
            int baseline_y = start_y + font.ascent;
            std::size_t emitted = 0;
            bool prev_break = true;

            while (emitted < max_chars) {
                auto ch = stream.next();
                if (!ch) break;
                if (*ch == '\n') {
                    cursor_x = start_x;
                    baseline_y += static_cast<int>(font.line_height() + metrics.line_spacing_px);
                    prev_break = true;
                    ++emitted;
                    continue;
                }

                if (*ch == '\r') {
                    prev_break = true;
                    ++emitted;
                    continue;
                }

                const bool breakChar = is_break_char(*ch, metrics.break_chars);
                if (!breakChar && prev_break && cursor_x != start_x) {
                    const auto wordWidth = static_cast<int>(stream.until_break_px(metrics.break_chars));
                    if (cursor_x + wordWidth > start_x + static_cast<int>(metrics.wrap_width_px)) {
                        cursor_x = start_x;
                        baseline_y += static_cast<int>(font.line_height() + metrics.line_spacing_px);
                    }
                }

                if (*ch == ' ') {
                    const auto& s = font.glyph_or_default(' ');
                    cursor_x += static_cast<int>(s.dwidth + metrics.letter_spacing_px);
                    prev_break = true;
                    ++emitted;
                    continue;
                }

                if (*ch == '\t') {
                    cursor_x += 16;
                    prev_break = true;
                    ++emitted;
                    continue;
                }

                const auto advance = draw_char(font, static_cast<unsigned char>(*ch), cursor_x, baseline_y, style);
                cursor_x += static_cast<int>(advance + metrics.letter_spacing_px);
                prev_break = breakChar;
                ++emitted;
            }

            return emitted;
        }

        /// @brief Create an incremental drawing cursor.
        template<typename Font, typename Stream>
        auto make_cursor(const Font& font, Stream stream, int start_x, int start_y, draw_metrics metrics,
                         glyph_style style = {}) {
            using layer_type = bg4_text_layer<WidthTiles, HeightTiles, Allocator>;
            return draw_cursor<layer_type, Font, Stream>{*this,   font, static_cast<Stream&&>(stream), start_x, start_y,
                                                         metrics, style};
        }

    private:
        static constexpr unsigned short map_width_tiles = 32;
        static constexpr unsigned short map_height_tiles = 32;

        unsigned short m_screenblock = 31;
        bitplane_config m_config{};
        Allocator m_allocator{};
        Allocator m_initial_allocator{};
        std::array<unsigned short, WidthTiles * HeightTiles> m_cell_state{};
        unsigned short m_current_vram_tile = no_tile;
        unsigned char m_current_plane = no_plane;
        unsigned short m_start_tile_x = 0;
        unsigned short m_start_tile_y = 0;

        static inline void set_tile_pixel(gba::tile4bpp& tile, int x, int y, unsigned char nibble) {
            auto& row = tile[static_cast<unsigned int>(y)];
            const auto shift = static_cast<unsigned int>(x * 4);
            row = (row & ~(0xFu << shift)) | ((nibble & 0xFu) << shift);
        }

        [[nodiscard]]
        static inline unsigned char get_tile_pixel(const gba::tile4bpp& tile, int x, int y) {
            const auto& row = tile[static_cast<unsigned int>(y)];
            const auto shift = static_cast<unsigned int>(x * 4);
            return static_cast<unsigned char>((row >> shift) & 0xFu);
        }

        [[nodiscard]]
        static inline bool glyph_bit(const unsigned char* bitmap, unsigned short byte_width, int x, int y) {
            const auto b = bitmap[static_cast<unsigned int>(y) * byte_width + static_cast<unsigned int>(x / 8)];
            return ((b >> (x % 8)) & 1u) != 0;
        }

        void draw_pixel(int x, int y, unsigned char role) {
            if (x < 0 || y < 0) return;
            const auto tx = static_cast<unsigned short>(x / 8);
            const auto ty = static_cast<unsigned short>(y / 8);
            if (tx >= WidthTiles || ty >= HeightTiles) return;

            // Allocate cell if needed
            const auto cell_idx = static_cast<unsigned int>(ty) * WidthTiles + tx;
            auto& cell = m_cell_state[cell_idx];

            if (cell == 0xFFFFu) {
                // Cell not yet allocated: Allocate a (tile, plane) pair
                allocate_cell(cell_idx);
            }

            if (cell == 0xFFFFu) return; // allocation failed

            const auto vram_tile = unpack_tile(cell);
            const auto plane_idx = unpack_plane(cell);
            if (vram_tile == no_tile || plane_idx == no_plane) return;

            // Get tile and perform merge
            auto* tiles = gba::memory_map(gba::mem_tile_4bpp);
            auto& tile_data = tiles[vram_tile >> 9][vram_tile & 511u];

            const int lx = x % 8;
            const int ly = y % 8;
            const auto old_nibble = get_tile_pixel(tile_data, lx, ly);

            // Decode all plane roles
            const auto new_nibble = m_config.update_role(old_nibble, plane_idx, role);
            set_tile_pixel(tile_data, lx, ly, new_nibble);
        }

        void allocate_cell(unsigned int cell_idx) {
            if (m_current_plane == no_plane || m_current_plane >= profile_plane_count(m_config.profile)) {
                // Need a fresh VRAM tile
                m_current_vram_tile = m_allocator.allocate(1);
                if (m_current_vram_tile == no_tile) return;

                m_current_plane = 0;

                // Initialize new tile with background nibbles
                auto* tiles = gba::memory_map(gba::mem_tile_4bpp);
                auto& tile_data = tiles[m_current_vram_tile >> 9][m_current_vram_tile & 511u];
                const auto bg_nibble = m_config.background_nibble();
                const auto row_val = static_cast<unsigned int>(0x11111111u) * bg_nibble;
                for (auto& row : tile_data) {
                    row = row_val;
                }
            }

            // Allocate plane on current tile
            const auto plane_idx = m_current_plane;
            m_current_plane = static_cast<unsigned char>(m_current_plane + 1);

            m_cell_state[cell_idx] = pack_cell_state(m_current_vram_tile, plane_idx);

            // Emit screen entry for this cell with correct palette bank
            const auto ty = static_cast<unsigned short>(cell_idx / WidthTiles);
            const auto tx = static_cast<unsigned short>(cell_idx % WidthTiles);
            const auto map_x = static_cast<unsigned short>(m_start_tile_x + tx);
            const auto map_y = static_cast<unsigned short>(m_start_tile_y + ty);
            if (map_x >= map_width_tiles || map_y >= map_height_tiles) return;

            const auto bank = m_config.bank_for_plane(plane_idx);

            const auto idx = static_cast<unsigned int>(map_y) * map_width_tiles + map_x;
            gba::mem_se[m_screenblock][idx] = {
                .tile_index = m_current_vram_tile,
                .palette_index = static_cast<unsigned short>((bank != 255u) ? bank : 0u),
            };
        }

        template<typename Font, typename Glyph>
        void draw_glyph_bitmap(const Font& font, const Glyph& glyph, int glyph_x, int glyph_y, unsigned char role) {
            const auto* bitmap = font.bitmap_data(glyph);
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) {
                    if (!glyph_bit(bitmap, glyph.bitmap_byte_width, x, y)) continue;
                    draw_pixel(glyph_x + x, glyph_y + y, role);
                }
            }
        }
    };

} // namespace gba::text
