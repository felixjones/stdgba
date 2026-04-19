/// @file bits/text/render_layer.hpp
/// @brief text main rendering layer - plane-aware and screenblock-aware.
///
/// Matches the rendering model of gba::text::bg4bpp_text_layer:
///  - Cell state map tracking (vram_tile, plane) per screen cell.
///  - Plane packing: multiple cells share one VRAM tile (mixed-radix encoding).
///  - Screenblock tilemap writes on every newly allocated cell.
///  - Pen/baseline pixel coordinate model with x_offset/y_offset from BDF glyph metrics.
///  - draw_stream() with stream_metrics (letter spacing, line spacing, wrap width).
///  - Color escape gating: only active for one_plane_full_color.
///  - start_index applied via bitplane_config::update_role() on every pixel.
///  - Decoration pass for decorated_font_result (shadow/outline).

#pragma once

#include <gba/video>
#include <gba/bits/text/glyph_blit.hpp>
#include <gba/bits/text/stream.hpp>
#include <gba/bits/text/tile_allocator.hpp>
#include <gba/bits/text/types.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <variant>

namespace gba::text {

    template<std::size_t CellCount>
    struct packed_cell_state_map {
        static constexpr std::uint16_t empty_state = 0x0FFFu;
        static constexpr std::size_t storage_size = (CellCount * 12u + 7u) / 8u;

        std::array<std::uint8_t, storage_size> bytes{};

        [[nodiscard]]
        std::uint16_t get(std::size_t index) const noexcept {
            const auto bit_offset = index * 12u;
            const auto byte_index = bit_offset >> 3u;
            const auto shift = static_cast<unsigned>(bit_offset & 7u);
            std::uint32_t chunk = static_cast<std::uint32_t>(bytes[byte_index]);
            if (byte_index + 1u < storage_size) chunk |= static_cast<std::uint32_t>(bytes[byte_index + 1u]) << 8u;
            if (byte_index + 2u < storage_size) chunk |= static_cast<std::uint32_t>(bytes[byte_index + 2u]) << 16u;
            return static_cast<std::uint16_t>((chunk >> shift) & 0x0FFFu);
        }

        void set(std::size_t index, std::uint16_t value) noexcept {
            const auto bit_offset = index * 12u;
            const auto byte_index = bit_offset >> 3u;
            const auto shift = static_cast<unsigned>(bit_offset & 7u);
            const auto masked = static_cast<std::uint32_t>(value & 0x0FFFu);
            std::uint32_t chunk = static_cast<std::uint32_t>(bytes[byte_index]);
            if (byte_index + 1u < storage_size) chunk |= static_cast<std::uint32_t>(bytes[byte_index + 1u]) << 8u;
            if (byte_index + 2u < storage_size) chunk |= static_cast<std::uint32_t>(bytes[byte_index + 2u]) << 16u;
            const auto clear_mask = ~(0x0FFFu << shift);
            chunk = (chunk & clear_mask) | (masked << shift);
            bytes[byte_index] = static_cast<std::uint8_t>(chunk & 0xFFu);
            if (byte_index + 1u < storage_size) bytes[byte_index + 1u] = static_cast<std::uint8_t>((chunk >> 8u) & 0xFFu);
            if (byte_index + 2u < storage_size) bytes[byte_index + 2u] = static_cast<std::uint8_t>((chunk >> 16u) & 0xFFu);
        }

        void clear() noexcept {
            std::memset(bytes.data(), 0xFF, bytes.size());
        }
    };

    template<typename Layer, typename Font, typename Stream>
    class draw_cursor {
    public:
        struct no_reveal_runs {
            static constexpr bool has_runtime_runs = true;
            static constexpr bool literal_only = false;
            static constexpr bool literal_has_escape_prefix = true;
            static constexpr std::size_t literal_char_count = 0;
            static constexpr std::size_t literal_prefix_entry_count = 0;
        };

        template<typename T>
        struct has_stream_reveal_runs {
            static constexpr bool value = false;
        };

        template<typename T>
        struct stream_reveal_runs {
            using type = no_reveal_runs;
        };

        template<typename T>
            requires requires { typename T::source_type::reveal_runs_type; }
        struct has_stream_reveal_runs<T> {
            static constexpr bool value = true;
        };

        template<typename T>
            requires requires { typename T::source_type::reveal_runs_type; }
        struct stream_reveal_runs<T> {
            using type = T::source_type::reveal_runs_type;
        };

        using reveal_runs_type = stream_reveal_runs<Stream>::type;

        static constexpr bool has_compiled_reveal_runs = has_stream_reveal_runs<Stream>::value;

        constexpr draw_cursor(Layer& layer, const Font& font, Stream stream, int start_x, int start_y,
                              stream_metrics metrics)
            : m_layer(&layer), m_font(&font), m_stream(static_cast<Stream&&>(stream)), m_start_x(start_x),
              m_cursor_x(start_x), m_baseline_y(start_y + font.ascent), m_metrics(metrics),
              m_full_color(layer_uses_full_color(layer)) {}

        bool next() {
            while (true) {
                if (m_done) return false;
                m_last_visible = false;

                if constexpr (requires (Stream s) { s.in_literal_run(); s.next_literal_char(); }) {
                    if (m_stream.in_literal_run()) {
                        auto ch = m_stream.next_literal_char();
                        if (!ch) {
                            m_done = true;
                            return false;
                        }
                        return consume_char(*ch);
                    }
                }

                auto token = m_stream.next();
                if (!token) {
                    m_done = true;
                    return false;
                }

                if (const auto* color = std::get_if<color_token>(&*token)) {
                    if (m_full_color && color->palette_index != 0) m_foreground_nibble = color->palette_index;
                    continue;
                }

                const auto ch = static_cast<char>(std::get<char_token>(*token).codepoint);
                return consume_char(ch);
            }
        }

        bool next_visible() {
            if constexpr (requires (Stream s) { s.in_literal_run(); s.next_literal_char(); }) {
                while (!m_done && m_stream.in_literal_run()) {
                    m_last_visible = false;
                    auto ch = m_stream.next_literal_char();
                    if (!ch) {
                        m_done = true;
                        return false;
                    }
                    consume_char(*ch);
                    if (m_last_visible) return true;
                }
            }

            while (next()) {
                if (m_last_visible) return true;
            }
            return false;
        }

        bool operator()() { return next(); }

        [[nodiscard]] std::size_t emitted() const noexcept { return m_emitted; }
        [[nodiscard]] bool done() const noexcept { return m_done; }

    private:
        [[nodiscard]]
        static constexpr bool layer_uses_full_color(const Layer& layer) {
            if constexpr (requires { layer.uses_full_color(); }) {
                return layer.uses_full_color();
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_break_char(char ch) noexcept {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        }

        [[nodiscard]] int glyph_advance(char ch) const noexcept {
            if (ch == '\t') return static_cast<int>(m_metrics.tab_width_px);
            if (ch == '\n' || ch == '\r') return 0;
            const auto uc = static_cast<unsigned char>(ch);
            if (uc < m_ascii_advances.size()) {
                return static_cast<int>(ascii_advance(uc));
            }
            return static_cast<int>(m_font->glyph_or_default(uc).dwidth);
        }

        [[nodiscard]]
        unsigned short ascii_advance(unsigned char encoding) const noexcept {
            const auto word = static_cast<std::size_t>(encoding >> 5u);
            const auto mask = static_cast<std::uint32_t>(1u) << (encoding & 31u);
            if ((m_ascii_advance_known[word] & mask) == 0u) {
                m_ascii_advances[encoding] = m_font->glyph_or_default(encoding).dwidth;
                m_ascii_advance_known[word] |= mask;
            }
            return m_ascii_advances[encoding];
        }

        [[nodiscard]]
        int line_advance_px() const noexcept {
            return static_cast<int>(m_font->line_height() + m_metrics.line_spacing_px);
        }

        [[nodiscard]]
        int literal_span_width_px(const glyph_run& run, std::uint16_t local_offset,
                                  std::uint16_t span_len, bool saw_glyph) const noexcept {
            if (span_len == 0) return 0;
            int width = 0;
            for (std::uint16_t i = 0; i < span_len; ++i) {
                const auto idx = static_cast<std::size_t>(run.lit_start) + local_offset + i;
                width += glyph_advance(reveal_runs_type::ast.format_str.data[idx]);
            }
            const int spacing_slots = static_cast<int>(span_len - 1) + (saw_glyph ? 1 : 0);
            if (spacing_slots > 0) {
                width += spacing_slots * static_cast<int>(m_metrics.letter_spacing_px);
            }
            return width;
        }

        [[nodiscard]] int measure_until_break_px() const {
            auto lookahead = m_stream;
            int width = 0;
            bool saw_glyph = false;
            char first_break = '\0';

            while (true) {
                if constexpr (has_compiled_reveal_runs && !reveal_runs_type::literal_has_escape_prefix) {
                    if (auto pos = lookahead.current_literal_position()) {
                        const auto& run = reveal_runs_type::runs[pos->segment_index];
                        const auto dense_idx = static_cast<std::size_t>(run.literal_dense_start) + pos->char_offset;
                        const auto span_len = reveal_runs_type::literal_non_break_spans[dense_idx];
                        if (span_len != 0) {
                            width += literal_span_width_px(run, pos->char_offset, span_len, saw_glyph);
                            saw_glyph = true;
                            lookahead.skip_literal_chars(span_len);
                            continue;
                        }
                        first_break = reveal_runs_type::ast.format_str.data[run.lit_start + pos->char_offset];
                        break;
                    }
                }

                auto token = lookahead.next();
                if (!token) break;
                if (std::holds_alternative<color_token>(*token)) continue;
                const auto ch = static_cast<char>(std::get<char_token>(*token).codepoint);
                if (is_break_char(ch)) {
                    first_break = ch;
                    break;
                }
                if (saw_glyph) width += static_cast<int>(m_metrics.letter_spacing_px);
                width += glyph_advance(ch);
                saw_glyph = true;
            }
            if (!saw_glyph && first_break == '\t') return static_cast<int>(m_metrics.tab_width_px);
            return width;
        }

        bool consume_char(char ch) {
            if (ch == '\n') {
                m_cursor_x = m_start_x;
                m_baseline_y += line_advance_px();
                m_prev_break = true;
                ++m_emitted;
                return true;
            }
            if (ch == '\r') {
                m_prev_break = true;
                ++m_emitted;
                return true;
            }

            const bool break_char = is_break_char(ch);
            if (!break_char && m_prev_break && m_cursor_x != m_start_x) {
                const auto word_width = measure_until_break_px();
                if (m_cursor_x + word_width > m_start_x + static_cast<int>(m_metrics.wrap_width_px)) {
                    m_cursor_x = m_start_x;
                    m_baseline_y += line_advance_px();
                }
            }

            if (ch == ' ') {
                m_cursor_x += static_cast<int>(ascii_advance(static_cast<unsigned char>(' ')) +
                                              m_metrics.letter_spacing_px);
                m_prev_break = true;
                ++m_emitted;
                return true;
            }
            if (ch == '\t') {
                m_cursor_x += static_cast<int>(m_metrics.tab_width_px);
                m_prev_break = true;
                ++m_emitted;
                return true;
            }

            const auto advance = m_layer->draw_char(*m_font, static_cast<unsigned char>(ch),
                                                    m_cursor_x, m_baseline_y, m_foreground_nibble);
            m_cursor_x += static_cast<int>(advance + m_metrics.letter_spacing_px);
            m_prev_break = break_char;
            ++m_emitted;
            m_last_visible = true;
            m_layer->flush_cache();
            return true;
        }

        Layer* m_layer;
        const Font* m_font;
        Stream m_stream;
        int m_start_x;
        int m_cursor_x;
        int m_baseline_y;
        stream_metrics m_metrics;
        bool m_full_color = false;
        unsigned char m_foreground_nibble = static_cast<unsigned char>(bitplane_role::foreground);
        mutable std::array<unsigned short, 128> m_ascii_advances{};
        mutable std::array<std::uint32_t, 4> m_ascii_advance_known{};
        std::size_t m_emitted = 0;
        bool m_done = false;
        bool m_prev_break = true;
        bool m_last_visible = false;
    };

    template<unsigned short Width, unsigned short Height>
    struct bg4bpp_text_layer {
    private:
        static constexpr unsigned short tile_grid_width  = (Width  + 7) / 8;
        static constexpr unsigned short tile_grid_height = (Height + 7) / 8;
        static constexpr unsigned short max_tiles        = tile_grid_width * tile_grid_height;
        static constexpr unsigned short map_dim          = 32;
        using cell_state_map_type = packed_cell_state_map<max_tiles>;
        static constexpr std::uint16_t unallocated_cell_state = cell_state_map_type::empty_state;
        inline static cell_state_map_type shared_cell_state{};

        unsigned short m_screenblock = 31;
        bitplane_config m_config{};
        linear_tile_allocator m_allocator{};
        linear_tile_allocator m_initial_allocator{};
        cell_state_map_type* m_cell_state = nullptr;
        std::uint16_t m_current_vram_tile = no_tile;
        std::uint8_t  m_current_plane     = no_plane;
        tile_plane_cache m_cache{};
        int m_pen_x = 0;
        int m_pen_y = 0;
        unsigned char m_foreground_nibble = static_cast<unsigned char>(bitplane_role::foreground);

        [[nodiscard]]
        static constexpr std::uint16_t encode_cell_state(std::uint16_t vram_tile, std::uint8_t plane_idx) noexcept {
            return static_cast<std::uint16_t>((vram_tile & 0x03FFu) | ((plane_idx & 0x03u) << 10u));
        }

        [[nodiscard]]
        static constexpr std::uint16_t decode_tile(std::uint16_t state) noexcept {
            if (state == unallocated_cell_state) return no_tile;
            return static_cast<std::uint16_t>(state & 0x03FFu);
        }

        [[nodiscard]]
        static constexpr std::uint8_t decode_plane(std::uint16_t state) noexcept {
            if (state == unallocated_cell_state) return no_plane;
            return static_cast<std::uint8_t>((state >> 10u) & 0x03u);
        }

        [[nodiscard]]
        bool prepare_cache_for_cell(unsigned int cell_idx, std::uint8_t& plane_idx) noexcept {
            auto state = m_cell_state->get(cell_idx);
            const bool is_new = (state == unallocated_cell_state);
            if (is_new) {
                allocate_cell(cell_idx);
                state = m_cell_state->get(cell_idx);
                if (state == unallocated_cell_state) return false;
            }

            const auto vram_tile = decode_tile(state);
            plane_idx = decode_plane(state);
            if (vram_tile == no_tile || plane_idx == no_plane) return false;

            if (m_cache.vram_tile != vram_tile) {
                m_cache.flush();
                const bool fresh_tile = is_new && plane_idx == 0u;
                if (fresh_tile) {
                    m_cache.init_to_background(vram_tile, m_config);
                } else {
                    m_cache.load_from_vram(vram_tile);
                }
            }
            return true;
        }

        void draw_bitmap_segment(int x, int y, std::uint8_t bits, int bit_count,
                                 unsigned char role) noexcept {
            if (bit_count <= 0 || bits == 0 || y < 0 || y >= Height) return;

            if (x < 0) {
                const int skip = -x;
                if (skip >= bit_count) return;
                bits = static_cast<std::uint8_t>(bits >> skip);
                bit_count -= skip;
                x = 0;
            }

            if (x >= Width) return;

            const int max_visible = static_cast<int>(Width) - x;
            if (bit_count > max_visible) {
                bit_count = max_visible;
                if (bit_count <= 0) return;
                bits &= static_cast<std::uint8_t>((1u << bit_count) - 1u);
            }

            int remaining = bit_count;
            int px = x;
            auto row_bits = bits;
            const auto ty = static_cast<unsigned short>(static_cast<unsigned>(y) >> 3u);
            const auto ly = static_cast<int>(static_cast<unsigned>(y) & 7u);

            while (remaining > 0 && row_bits != 0) {
                const auto tx = static_cast<unsigned short>(static_cast<unsigned>(px) >> 3u);
                if (tx >= tile_grid_width || ty >= tile_grid_height) return;

                const int tile_lx = px & 7;
                const int chunk = ((8 - tile_lx) < remaining) ? (8 - tile_lx) : remaining;
                const auto chunk_mask = static_cast<std::uint8_t>((1u << chunk) - 1u);
                const auto chunk_bits = static_cast<std::uint8_t>(row_bits & chunk_mask);
                if (chunk_bits != 0) {
                    const auto cell_idx = static_cast<unsigned>(ty) * tile_grid_width + tx;
                    std::uint8_t plane_idx = no_plane;
                    if (!prepare_cache_for_cell(cell_idx, plane_idx)) return;
                    m_cache.apply_row(ly, chunk_bits, tile_lx, plane_idx, role, m_config);
                }
                row_bits = static_cast<std::uint8_t>(row_bits >> chunk);
                px += chunk;
                remaining -= chunk;
            }
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
            m_cell_state->set(cell_idx, encode_cell_state(m_current_vram_tile, plane_idx));

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
                const auto row_ptr = bitmap + static_cast<unsigned>(row) * g.bitmap_byte_width;
                for (int byte_idx = 0; byte_idx < g.bitmap_byte_width; ++byte_idx) {
                    const int src_x = byte_idx * 8;
                    if (src_x >= g.width) break;
                    const int bit_count = ((g.width - src_x) < 8) ? (g.width - src_x) : 8;
                    auto bits = row_ptr[byte_idx];
                    if (bit_count < 8) {
                        bits &= static_cast<unsigned char>((1u << bit_count) - 1u);
                    }
                    if (bits == 0) continue;
                    draw_bitmap_segment(glyph_x + src_x, glyph_y + row, bits, bit_count, role);
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
                const int start_x = view.x_offset;
                const int start_y = view.y_offset;
                const int end_x = start_x + static_cast<int>(view.width);
                for (int y = 0; y < static_cast<int>(view.height); ++y) {
                    const auto row_ptr = view.data +
                        static_cast<std::size_t>(start_y + y) * view.byte_width;
                    const int row_y = glyph_y + start_y + y;
                    const int start_byte = start_x / 8;
                    const int end_byte = (end_x - 1) / 8;
                    for (int byte_idx = start_byte; byte_idx <= end_byte; ++byte_idx) {
                        auto bits = row_ptr[byte_idx];
                        if (bits == 0) continue;

                        const int byte_start_x = byte_idx * 8;
                        const int clip_left = (byte_start_x < start_x) ? (start_x - byte_start_x) : 0;
                        const int clip_right = (byte_start_x + 8 > end_x) ? (byte_start_x + 8 - end_x) : 0;
                        const int bit_count = 8 - clip_left - clip_right;
                        if (bit_count <= 0) continue;

                        bits >>= static_cast<unsigned>(clip_left);
                        if (bit_count < 8) {
                            bits &= static_cast<unsigned char>((1u << bit_count) - 1u);
                        }
                        if (bits == 0) continue;

                        draw_bitmap_segment(glyph_x + byte_start_x + clip_left, row_y,
                                            bits, bit_count, role);
                    }
                }
            }
        }

    public:
        using cell_state_map = cell_state_map_type;

        /// @brief Construct a text layer.
        /// @param screenblock  GBA screenblock index (0-31).
        /// @param cfg          Bitplane encoding/palette config.
        /// @param allocator    Tile allocator for glyph tiles.
        /// @param cell_state   External packed cell-state storage.
        bg4bpp_text_layer(unsigned short screenblock, const bitplane_config& cfg,
                          linear_tile_allocator allocator,
                          cell_state_map_type& cell_state)
            : m_screenblock(screenblock), m_config(cfg),
              m_allocator(allocator), m_initial_allocator(allocator), m_cell_state(&cell_state) {
            clear();
        }

        /// @brief Construct a text layer using shared fallback cell-state storage.
        bg4bpp_text_layer(unsigned short screenblock, const bitplane_config& cfg,
                          linear_tile_allocator allocator = {.next_tile = 1, .end_tile = 512})
            : bg4bpp_text_layer(screenblock, cfg, allocator, shared_cell_state) {}

        /// @brief Construct without screenblock (uses default 31, legacy compat).
        explicit bg4bpp_text_layer(const bitplane_config& cfg)
            : bg4bpp_text_layer(31, cfg) {}

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
            clear_background_tile();
            m_pen_x = 0;
            m_pen_y = 0;
            m_foreground_nibble = static_cast<unsigned char>(bitplane_role::foreground);
            m_cell_state->clear();
            clear_screenblock();
        }

        /// @brief Alias for clear().
        void reset() noexcept { clear(); }

        void flush_cache() noexcept { m_cache.flush(); }

        /// @brief Draw a single character, returning the advance width in pixels.
        ///
        /// Matches the legacy draw_char() placement model:
        ///   glyph_x = pen_x + g.x_offset
        ///   glyph_y = baseline_y - g.height - g.y_offset
        template<typename Font>
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
        template<typename Font>
        std::size_t draw_stream(const Font& font, const char* str, int start_x, int start_y,
                                const stream_metrics& metrics = {},
                                std::size_t max_chars = std::numeric_limits<std::size_t>::max()) noexcept {
            if (!str) return 0;
            int cursor_x = start_x;
            int baseline_y = start_y + font.ascent;
            const int line_advance = static_cast<int>(font.line_height() + metrics.line_spacing_px);
            const auto full_color = uses_full_color();
            auto fg_nibble = static_cast<unsigned char>(bitplane_role::foreground);
            std::size_t emitted = 0;
            bool prev_break = true;

            while (emitted < max_chars && *str) {
                const char ch = *str++;

                if (ch == '\n') {
                    cursor_x = start_x;
                    baseline_y += line_advance;
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
                        baseline_y += line_advance;
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

        template<typename Font>
        std::size_t draw_stream(const Font& font, const char* str, int start_x, int start_y,
                                std::size_t max_chars) noexcept {
            return draw_stream(font, str, start_x, start_y, stream_metrics{}, max_chars);
        }

        template<typename Font, typename Stream>
        auto make_cursor(const Font& font, Stream stream, int start_x, int start_y,
                         const stream_metrics& metrics = {}) {
            using layer_type = bg4bpp_text_layer<Width, Height>;
            return draw_cursor<layer_type, Font, Stream>{*this, font, static_cast<Stream&&>(stream),
                                                         start_x, start_y, metrics};
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
        void clear_background_tile() noexcept {
            auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
            const auto bg_nibble = m_config.background_nibble();
            const auto bg_row = static_cast<std::uint32_t>(0x11111111u) * bg_nibble;
            for (auto& row : tiles[0]) {
                row = bg_row;
            }
        }

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

        void clear_screenblock() noexcept {
            const auto palbank_0 = static_cast<unsigned short>((m_config.palbank_0 != 255u) ? m_config.palbank_0 : 0u);
            if (palbank_0 == 0u) {
                for (unsigned int i = 0; i < map_dim * map_dim; ++i) {
                    gba::mem_se[m_screenblock][i] = {
                        .tile_index = 0,
                        .palette_index = 0,
                    };
                }
                return;
            }

            for (unsigned int i = 0; i < map_dim * map_dim; ++i) {
                gba::mem_se[m_screenblock][i] = {
                    .tile_index = 0,
                    .palette_index = palbank_0,
                };
            }
        }
    };

} // namespace gba::text
