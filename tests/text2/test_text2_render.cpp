/// @file tests/text2/test_text2_render.cpp
/// @brief Rendering engine tests for text2.

#include <gba/embed>
#include <gba/testing>
#include <gba/text2>
#include <gba/bits/text/font_variants.hpp>

#include <array>

using namespace gba::literals;

namespace {
    constexpr auto make_cursor_test_bdf() {
        constexpr char source[] = R"(STARTFONT 2.1
FONT cursor-test
SIZE 8 75 75
FONTBOUNDINGBOX 5 7 0 0
STARTPROPERTIES 3
FONT_ASCENT 7
FONT_DESCENT 0
DEFAULT_CHAR 65
ENDPROPERTIES
CHARS 3
STARTCHAR space
ENCODING 32
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
00
00
00
00
00
00
00
ENDCHAR
STARTCHAR A
ENCODING 65
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
20
50
88
F8
88
88
88
ENDCHAR
STARTCHAR B
ENCODING 66
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
F0
88
88
F0
88
88
F0
ENDCHAR
ENDFONT
)";

        std::array<unsigned char, sizeof(source) - 1> data{};
        for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(source[i]);
        return data;
    }

    static constexpr auto cursor_font = gba::embed::bdf([] { return make_cursor_test_bdf(); });
    static constexpr auto cursor_font_shadowed = gba::text::with_shadow<1, 1>(cursor_font);
    static constexpr auto cursor_font_outlined = gba::text::with_outline<1>(cursor_font);

    static_assert(gba::text2::GlyphFont<decltype(cursor_font_shadowed)>,
                  "decorated_font_result (shadow) must satisfy text2::GlyphFont");
    static_assert(gba::text2::GlyphFont<decltype(cursor_font_outlined)>,
                  "decorated_font_result (outline) must satisfy text2::GlyphFont");

    struct mock_layer {
        struct call {
            unsigned int codepoint = 0;
            int x = 0;
            int baseline_y = 0;
            unsigned char fg_nibble = 0;
        };

        std::array<call, 8> calls{};
        unsigned int count = 0;
        bool full_color = false;

        template<typename Font>
        unsigned short draw_char(const Font& font, unsigned int codepoint, int x, int baseline_y,
                                 unsigned char fg_nibble) {
            if (count < calls.size()) {
                calls[count] = {.codepoint = codepoint, .x = x, .baseline_y = baseline_y, .fg_nibble = fg_nibble};
            }
            ++count;
            return font.glyph_or_default(codepoint).dwidth;
        }

        void flush_cache() {}

        [[nodiscard]] bool uses_full_color() const noexcept { return full_color; }
    };
}

int main() {
    // Test: Layer instantiation
    {
        auto config = gba::text2::bitplane_config{
            .profile = gba::text2::bitplane_profile::two_plane_three_color,
            .palbank_0 = 1,
            .palbank_1 = 2,
            .start_index = 1,
        };
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
        gba::test.eq(layer.palette(), 1u);
    }

    // Test: Palette setting
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        layer.set_palette(5);
        gba::test.eq(layer.palette(), 5u);
    }

    // Test: Palette bounds (invalid palette should clamp)
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        layer.set_palette(5);
        layer.set_palette(20);
        gba::test.eq(layer.palette(), 5u);
    }

    // Test: Reset clears state
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        layer.set_palette(7);
        layer.reset();
        gba::test.eq(layer.palette(), 1u);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
    }

    // Test: Max tile count calculation (240x160 = 30×20 tiles)
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        gba::test.eq(layer.max_tile_count(), 600u);
    }

    // Test: Different layer dimensions
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<120, 80> layer(config);
        gba::test.eq(layer.max_tile_count(), 150u);
    }

    // Test: Cstr stream tokenization
    {
        gba::text2::cstr_stream stream("Hello");
        auto token = stream.next();
        gba::test.is_true(token.has_value());
        auto* char_ptr = std::get_if<gba::text2::char_token>(&*token);
        gba::test.is_true(char_ptr != nullptr);
        if (char_ptr) {
            gba::test.eq(char_ptr->codepoint, static_cast<unsigned int>('H'));
        }
    }

    // Test: Color escape tokenization
    {
        const char with_escape[] = "\x1B""5Test";
        gba::text2::cstr_stream stream(with_escape);
        auto token = stream.next();
        gba::test.is_true(token.has_value());
        auto* color_ptr = std::get_if<gba::text2::color_token>(&*token);
        gba::test.is_true(color_ptr != nullptr);
        if (color_ptr) {
            gba::test.eq(color_ptr->palette_index, 5u);
        }

        auto next_token = stream.next();
        gba::test.is_true(next_token.has_value());
        auto* char_ptr = std::get_if<gba::text2::char_token>(&*next_token);
        gba::test.is_true(char_ptr != nullptr);
        if (char_ptr) {
            gba::test.eq(char_ptr->codepoint, static_cast<unsigned int>('T'));
        }
    }

    // Test: Text2 format with palette specifier
    {
        auto fmt = gba::text2::text2_format<"Pal: {p:pal}">{};
        char buf[16]{};
        fmt.to(buf, "p"_arg = 3);
        gba::test.eq(buf[0], 'P');
        gba::test.eq(buf[1], 'a');
        gba::test.eq(buf[2], 'l');
        gba::test.eq(buf[3], ':');
        gba::test.eq(buf[4], ' ');
        gba::test.eq(static_cast<unsigned char>(buf[5]), 0x1Bu);
        gba::test.eq(static_cast<unsigned char>(buf[6]), 3u);
    }

    // Test: Bitplane profile helpers
    {
        constexpr auto prof = gba::text2::bitplane_profile::two_plane_three_color;
        constexpr auto planes = gba::text2::profile_plane_count(prof);
        constexpr auto roles = gba::text2::profile_role_count(prof);
        gba::test.eq(planes, 2u);
        gba::test.eq(roles, 3u);
    }

    // Test: Config helpers
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile = gba::text2::bitplane_profile::three_plane_binary,
        };
        gba::test.eq(cfg.plane_count(), 3u);
        gba::test.eq(cfg.role_count(), 2u);
    }

    // Test: Tile index calculation
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        auto idx = layer.tile_index_from_row_col(0, 0);
        gba::test.eq(idx, 0u);
        idx = layer.tile_index_from_row_col(0, 1);
        gba::test.eq(idx, 1u);
        idx = layer.tile_index_from_row_col(1, 0);
        gba::test.eq(idx, 30u); // 240/8 = 30 columns
    }

    // Test: Multiple tiles on same row
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        auto idx1 = layer.tile_index_from_row_col(2, 5);
        auto idx2 = layer.tile_index_from_row_col(2, 6);
        gba::test.eq(idx2 - idx1, 1u);
    }

    // Test: Current tile count tracking
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        gba::test.eq(layer.current_tile_count(), 0u);
        gba::test.eq(layer.max_tile_count(), 600u); // 30 × 20 tiles
    }

    // Test: Reset clears tile count
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        layer.reset();
        gba::test.eq(layer.current_tile_count(), 0u);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
    }

    // Test: Color escape with palette clamp
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<240, 160> layer(config);
        layer.set_palette(5);
        gba::test.eq(layer.palette(), 5u);
        layer.set_palette(20); // Out of range
        gba::test.eq(layer.palette(), 5u); // Should retain previous
    }

    // Test: Cursor next_visible skips whitespace and wraps at word boundaries
    {
        mock_layer layer{};
        gba::text2::stream_metrics metrics{
            .letter_spacing_px = 1,
            .line_spacing_px = 2,
            .tab_width_px = 16,
            .wrap_width_px = 20,
        };
        auto stream = gba::text2::cstr_stream("AA AA");
        gba::text2::draw_cursor<mock_layer, decltype(cursor_font), decltype(stream)> cursor{
            layer, cursor_font, stream, 0, 0, metrics};

        while (cursor.next_visible()) {}

        gba::test.eq(layer.count, 4u);
        gba::test.eq(layer.calls[0].x, 0);
        gba::test.eq(layer.calls[1].x, 9);
        gba::test.eq(layer.calls[2].x, 0);
        gba::test.eq(layer.calls[3].x, 9);
        gba::test.eq(layer.calls[2].baseline_y,
                     layer.calls[0].baseline_y + static_cast<int>(cursor_font.line_height()) + 2);
        gba::test.eq(cursor.emitted(), 5u);
        gba::test.is_true(cursor.done());
    }

    // Test: Cursor next_visible applies full-color escape without consuming a visible step
    {
        mock_layer layer{.full_color = true};
        gba::text2::stream_metrics metrics{};
        auto stream = gba::text2::cstr_stream("\x1B" "5A");
        gba::text2::draw_cursor<mock_layer, decltype(cursor_font), decltype(stream)> cursor{
            layer, cursor_font, stream, 0, 0, metrics};

        gba::test.is_true(cursor.next_visible());
        gba::test.eq(layer.count, 1u);
        gba::test.eq(layer.calls[0].codepoint, static_cast<unsigned int>('A'));
        gba::test.eq(layer.calls[0].fg_nibble, 5u);
        gba::test.eq(cursor.emitted(), 1u);
    }

    // Test: Layer make_cursor is available and can draw incrementally
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<32, 32> layer(config);
        auto cursor = layer.make_cursor(cursor_font, gba::text2::cstr_stream("A"), 0, 0);
        gba::test.is_true(cursor.next_visible());
        gba::test.eq(cursor.emitted(), 1u);
    }

    // Test: Shadow font decoration view is non-empty for a glyph with set pixels
    {
        const auto& g = cursor_font_shadowed.glyph_or_default('A');
        const auto view = cursor_font_shadowed.decoration_view_for(g);
        gba::test.is_true(view.width > 0);
        gba::test.is_true(view.height > 0);
        gba::test.is_true(view.data != nullptr);
    }

    // Test: Outline font decoration view is non-empty for a glyph with set pixels
    {
        const auto& g = cursor_font_outlined.glyph_or_default('A');
        const auto view = cursor_font_outlined.decoration_view_for(g);
        gba::test.is_true(view.width > 0);
        gba::test.is_true(view.height > 0);
        gba::test.is_true(view.data != nullptr);
    }

    // Test: Outline font expands glyph bounds relative to plain font
    {
        const auto& plain_g   = cursor_font.glyph_or_default('A');
        const auto& outline_g = cursor_font_outlined.glyph_or_default('A');
        gba::test.eq(static_cast<unsigned int>(outline_g.width),
                     static_cast<unsigned int>(plain_g.width + 2));
        gba::test.eq(static_cast<unsigned int>(outline_g.height),
                     static_cast<unsigned int>(plain_g.height + 2));
    }

    // Section: Multi-tile glyph spanning

    // Test: Horizontal span - glyph at pen_x=5 crosses tile column boundary (cols 0 and 1)
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile     = gba::text2::bitplane_profile::one_plane_full_color,
            .start_index = 1,
        };
        constexpr gba::text2::linear_tile_allocator alloc{.next_tile = 100, .end_tile = 200};
        gba::text2::bg4bpp_text_layer<32, 32> layer(31, cfg, alloc);

        layer.draw_char(cursor_font, static_cast<unsigned int>('A'),
                        5, cursor_font.ascent, 1);
        layer.flush_cache();

        gba::test.eq(layer.current_tile_count(), 2u);

        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        const auto tile0 = static_cast<unsigned int>(gba::mem_se[31u][0u].value().tile_index);
        const auto tile1 = static_cast<unsigned int>(gba::mem_se[31u][1u].value().tile_index);

        // glyph row 3 spans abs_x = 5..9; abs_y = 3
        // abs(7,3) -> tile0, lx=7, ly=3 -> shift = 28
        // abs(8,3) -> tile1, lx=0, ly=3 -> shift =  0
        const auto n0 = (tiles[tile0][3u] >> 28u) & 0xFu;
        const auto n1 = (tiles[tile1][3u] >>  0u) & 0xFu;
        gba::test.eq(n0, 2u);
        gba::test.eq(n1, 2u);
    }

    // Test: Vertical span - glyph at baseline_y=11 crosses tile row boundary (rows 0 and 1)
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile     = gba::text2::bitplane_profile::one_plane_full_color,
            .start_index = 1,
        };
        constexpr gba::text2::linear_tile_allocator alloc{.next_tile = 100, .end_tile = 200};
        gba::text2::bg4bpp_text_layer<32, 32> layer(31, cfg, alloc);

        // glyph_y = baseline_y - height - y_offset = 11 - 7 - 0 = 4
        // rows 0-3 -> abs_y=4..7 -> tile row 0
        // rows 4-6 -> abs_y=8..10 -> tile row 1
        layer.draw_char(cursor_font, static_cast<unsigned int>('A'),
                        0, 11, 1);
        layer.flush_cache();

        gba::test.eq(layer.current_tile_count(), 2u);

        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        // cell (tx=0, ty=0) -> map_idx = 0*32+0 = 0
        const auto tile_row0 = static_cast<unsigned int>(gba::mem_se[31u][0u].value().tile_index);
        // cell (tx=0, ty=1) -> map_idx = 1*32+0 = 32
        const auto tile_row1 = static_cast<unsigned int>(gba::mem_se[31u][32u].value().tile_index);

        // glyph row 3 -> abs_y=7, lx=0 (glyph col 0 -> abs_x=0), ly=7
        const auto n_row0 = (tiles[tile_row0][7u] >> 0u) & 0xFu;
        // glyph row 4 -> abs_y=8, lx=0 (glyph col 0 -> abs_x=0), ly=0
        const auto n_row1 = (tiles[tile_row1][0u] >> 0u) & 0xFu;
        gba::test.eq(n_row0, 2u);
        gba::test.eq(n_row1, 2u);
    }

    // Test: 2D span - glyph at pen_x=5, baseline_y=11 crosses both tile column and row boundaries
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile     = gba::text2::bitplane_profile::one_plane_full_color,
            .start_index = 1,
        };
        constexpr gba::text2::linear_tile_allocator alloc{.next_tile = 100, .end_tile = 200};
        gba::text2::bg4bpp_text_layer<32, 32> layer(31, cfg, alloc);

        // glyph_x=5, glyph_y=4 -> spans cells (tx=0,ty=0),(tx=1,ty=0),(tx=0,ty=1),(tx=1,ty=1)
        layer.draw_char(cursor_font, static_cast<unsigned int>('A'),
                        5, 11, 1);
        layer.flush_cache();

        gba::test.eq(layer.current_tile_count(), 4u);

        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        // map indices: (ty*32+tx) for each of the 4 cells in screenblock 31
        const auto t00 = static_cast<unsigned int>(gba::mem_se[31u][ 0u].value().tile_index); // tx=0,ty=0
        const auto t10 = static_cast<unsigned int>(gba::mem_se[31u][ 1u].value().tile_index); // tx=1,ty=0
        const auto t01 = static_cast<unsigned int>(gba::mem_se[31u][32u].value().tile_index); // tx=0,ty=1
        const auto t11 = static_cast<unsigned int>(gba::mem_se[31u][33u].value().tile_index); // tx=1,ty=1

        // tile(0,0): glyph row 3, col 2 -> abs(7,7) -> lx=7, ly=7 -> shift=28
        const auto n00 = (tiles[t00][7u] >> 28u) & 0xFu;
        // tile(1,0): glyph row 1, col 3 -> abs(8,5) -> lx=0, ly=5 -> shift=0
        const auto n10 = (tiles[t10][5u] >>  0u) & 0xFu;
        // tile(0,1): glyph row 4, col 0 -> abs(5,8) -> lx=5, ly=0 -> shift=20
        const auto n01 = (tiles[t01][0u] >> 20u) & 0xFu;
        // tile(1,1): glyph row 4, col 4 -> abs(9,8) -> lx=1, ly=0 -> shift=4
        const auto n11 = (tiles[t11][0u] >>  4u) & 0xFu;

        gba::test.eq(n00, 2u);
        gba::test.eq(n10, 2u);
        gba::test.eq(n01, 2u);
        gba::test.eq(n11, 2u);
    }

    // Test: draw_stream across tile column boundary (two glyphs in adjacent tile columns)
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile     = gba::text2::bitplane_profile::one_plane_full_color,
            .start_index = 1,
        };
        constexpr gba::text2::linear_tile_allocator alloc{.next_tile = 100, .end_tile = 200};
        gba::text2::bg4bpp_text_layer<32, 32> layer(31, cfg, alloc);

        // 'A' at pen_x=0 (glyph_x=0, pixels abs_x=0..4, all in tx=0)
        // 'B' at pen_x=8 via dwidth advance (glyph_x=8, pixels abs_x=8..12, all in tx=1)
        layer.draw_stream(cursor_font, "AB", 0, 0);

        gba::test.eq(layer.current_tile_count(), 2u);

        // 'B' row 0 (0xF0): glyph cols 0..3 -> glyph_x=8 -> abs_x=8,9,10,11 -> all tx=1 -> map_idx=1
        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        const auto tile_b = static_cast<unsigned int>(gba::mem_se[31u][1u].value().tile_index);
        // glyph row 0 of 'B' (0xF0) -> cols 0..3 -> abs_x=8..11 -> lx=0..3; abs_y=0 -> ly=0
        const auto nb0 = (tiles[tile_b][0u] >> 0u) & 0xFu;
        gba::test.eq(nb0, 2u);
    }

    // Test: draw_char with shadow font writes both foreground and shadow nibbles into VRAM
    {
        constexpr auto cfg = gba::text2::bitplane_config{
            .profile     = gba::text2::bitplane_profile::two_plane_three_color,
            .palbank_0   = 1,
            .palbank_1   = 2,
            .start_index = 1,
        };
        constexpr gba::text2::linear_tile_allocator alloc{.next_tile = 100, .end_tile = 200};
        gba::text2::bg4bpp_text_layer<32, 32> layer(31, cfg, alloc);

        layer.draw_char(cursor_font_shadowed, static_cast<unsigned int>('A'),
                        0, cursor_font_shadowed.ascent, 1);
        layer.flush_cache();

        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));

        const auto nibble_at = [](const gba::tile4bpp& tile, unsigned int x, unsigned int y) {
            return static_cast<unsigned int>((tile[y] >> (x * 4u)) & 0xFu);
        };

        const auto region_has_nibble = [&](unsigned int nibble) {
            for (unsigned int row = 0; row < 4u; ++row) {
                for (unsigned int col = 0; col < 4u; ++col) {
                    const auto se = gba::mem_se[31u][row * 32u + col].value();
                    if (se.tile_index < 100u || se.tile_index >= 200u) continue;
                    const auto& tile = tiles[se.tile_index];
                    for (unsigned int y = 0; y < 8u; ++y) {
                        for (unsigned int x = 0; x < 8u; ++x) {
                            if (nibble_at(tile, x, y) == nibble) return true;
                        }
                    }
                }
            }
            return false;
        };

        gba::test.is_true(region_has_nibble(2u)); // foreground nibble (start_index + fg_role * stride = 1+1*1)
        gba::test.is_true(region_has_nibble(3u)); // shadow nibble (start_index + shadow_role * stride = 1+2*1)
    }

    return gba::test.finish();
}
