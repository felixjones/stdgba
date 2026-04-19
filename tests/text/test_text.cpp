/// @file test_text.cpp
/// @brief Tests for streaming text helpers and bitplane palette helpers.

#include <gba/embed>
#include <gba/format>
#include <gba/testing>
#include <gba/text>
#include <gba/video>

#include <array>
#include <cstddef>

static constexpr auto make_test_bdf_for_text() {
    constexpr char source[] = R"(STARTFONT 2.1
FONT test
SIZE 12 100 100
FONTBOUNDINGBOX 9 18 0 -4
STARTPROPERTIES 3
FONT_ASCENT 14
FONT_DESCENT 4
DEFAULT_CHAR 65
ENDPROPERTIES
CHARS 3
STARTCHAR space
ENCODING 32
SWIDTH 540 0
DWIDTH 3 0
BBX 1 1 0 0
BITMAP
0000
ENDCHAR
STARTCHAR A
ENCODING 65
SWIDTH 540 0
DWIDTH 9 0
BBX 1 1 0 0
BITMAP
8000
ENDCHAR
STARTCHAR i
ENCODING 105
SWIDTH 540 0
DWIDTH 4 0
BBX 1 1 0 0
BITMAP
8000
ENDCHAR
ENDFONT
)";

    std::array<unsigned char, sizeof(source) - 1> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(source[i]);
    return data;
}

static constexpr auto text_test_font = gba::embed::bdf([] { return make_test_bdf_for_text(); });
static constexpr auto text_test_font_shadowed = gba::text::with_shadow<1, 1>(text_test_font);
static constexpr auto text_test_font_outlined = gba::text::with_outline<1>(text_test_font);

int main() {
    static constexpr auto font = text_test_font;

    {
        gba::text::stream_metrics metrics{};
        metrics.letter_spacing_px = 1;

        auto s = gba::text::stream("Ai A", font, metrics);
        gba::test.eq(s.until_break_px(), 14u); // A(9) + 1 + i(4)

        gba::test.eq(*s.next(), 'A');
        gba::test.eq(s.until_break_px(), 4u); // i only

        gba::test.eq(*s.next(), 'i');
        gba::test.eq(*s.next(), ' ');
        gba::test.eq(s.until_break_px(), 9u);
    }

    // stream: until_break_px honors break_policy::whitespace_and_hyphen.
    {
        gba::text::stream_metrics metrics{};
        metrics.break_chars = gba::text::break_policy::whitespace_and_hyphen;

        auto s = gba::text::stream("AA-AA", font, metrics);
        gba::test.eq(s.until_break_px(), 18u); // stops before '-'

        gba::test.eq(*s.next(), 'A');
        gba::test.eq(s.until_break_px(), 9u); // remaining "A" before '-'
    }

    // draw_cursor: wrap happens only at word starts (long tokens do not wrap per-character).
    {
        using font_type = decltype(font);
        struct mock_layer {
            struct draw_call {
                int x{};
                int baseline_y{};
                unsigned int encoding{};
            };

            std::array<draw_call, 16> calls{};
            std::size_t count = 0;

            unsigned short draw_char(const font_type& f, unsigned int enc, int pen_x, int baseline_y,
                                     unsigned char = 1) {
                if (count < calls.size()) calls[count] = {.x = pen_x, .baseline_y = baseline_y, .encoding = enc};
                ++count;
                return f.glyph_or_default(enc).dwidth;
            }
        };

        mock_layer layer{};

        gba::text::stream_metrics streamMetrics{};
        auto s = gba::text::stream("AAAAA", font, streamMetrics);

        gba::text::draw_metrics drawMetrics{};
        drawMetrics.wrap_width_px = 10; // narrower than the token

        gba::text::draw_cursor<mock_layer, decltype(font), decltype(s)> cursor{layer, font, s, 0, 0, drawMetrics};

        while (cursor.next()) {}

        gba::test.eq(layer.count, 5u);
        gba::test.eq(layer.calls[0].x, 0);
        gba::test.eq(layer.calls[1].x, 9);
        gba::test.eq(layer.calls[2].x, 18);
        gba::test.eq(layer.calls[3].x, 27);
        gba::test.eq(layer.calls[4].x, 36);
        // Baseline must remain constant (no wrap).
        gba::test.eq(layer.calls[0].baseline_y, layer.calls[4].baseline_y);
    }

    // draw_cursor: wraps at word boundaries when the next word would exceed wrap_width_px.
    {
        using font_type = decltype(font);
        struct mock_layer {
            struct draw_call {
                int x{};
                int baseline_y{};
            };

            std::array<draw_call, 16> calls{};
            std::size_t count = 0;

            unsigned short draw_char(const font_type& f, unsigned int, int pen_x, int baseline_y,
                                     unsigned char = 1) {
                if (count < calls.size()) calls[count] = {.x = pen_x, .baseline_y = baseline_y};
                ++count;
                return f.glyph_or_default('A').dwidth;
            }
        };

        mock_layer layer{};
        gba::text::stream_metrics streamMetrics{};
        auto s = gba::text::stream("AA AA", font, streamMetrics);

        gba::text::draw_metrics drawMetrics{};
        drawMetrics.wrap_width_px = 20;

        gba::text::draw_cursor<mock_layer, decltype(font), decltype(s)> cursor{layer, font, s, 0, 0, drawMetrics};

        while (cursor.next()) {}

        gba::test.eq(layer.count, 4u);
        gba::test.eq(layer.calls[0].x, 0);
        gba::test.eq(layer.calls[1].x, 9);
        gba::test.eq(layer.calls[2].x, 0); // wrapped
        gba::test.eq(layer.calls[3].x, 9);
        gba::test.eq(layer.calls[2].baseline_y, layer.calls[0].baseline_y + static_cast<int>(font.line_height()));
    }

    {
        using namespace gba::literals;
        constexpr auto fmt = "{a} {i}"_fmt;

        int supplierCalls = 0;
        auto gen = fmt.generator(
            "a"_arg =
                [&] {
                    ++supplierCalls;
                    return "A";
                },
            "i"_arg =
                [&] {
                    ++supplierCalls;
                    return "i";
                });

        gba::text::stream_metrics metrics{};
        metrics.letter_spacing_px = 1;

        auto s = gba::text::stream(gen, font, metrics);

        gba::test.eq(supplierCalls, 0);
        gba::test.eq(s.until_break_px(), 9u);
        gba::test.is_true(supplierCalls > 0);

        auto c0 = s.next();
        gba::test.is_true(c0.has_value());
        gba::test.eq(*c0, 'A');

        auto c1 = s.next();
        gba::test.is_true(c1.has_value());
        gba::test.eq(*c1, ' ');

        gba::test.eq(s.until_break_px(), 4u);
    }

    // stream: one_plane_full_color inline color commands do not affect measured width.
    {
        constexpr auto esc = gba::text::color_escape(4);
        gba::test.eq(static_cast<unsigned int>(static_cast<unsigned char>(esc[0])),
                     static_cast<unsigned int>(static_cast<unsigned char>(gba::text::color_escape_prefix)));
        gba::test.eq(esc[1], '4');

        gba::text::stream_metrics metrics{};
        metrics.letter_spacing_px = 1;

        auto s = gba::text::stream("A" "\x1B" "4" "i", font, metrics);
        gba::test.eq(s.until_break_px(), 14u); // A(9) + 1 + i(4)

        auto trailing = gba::text::stream("A" "\x1B" "4", font, metrics);
        gba::test.eq(trailing.until_break_px(), 9u);
    }

    {
        constexpr auto cfg = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_binary,
            .palbank_0 = 2,
            .palbank_1 = 3,
            .start_index = 1,
        };

        gba::test.eq(gba::text::required_entries(cfg.profile), 4u);
        gba::test.is_true(gba::text::valid_config(cfg));

        constexpr auto bad_cfg = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 5,
            .start_index = 13, // start_index(13) + entry_count(9) > 16
        };
        // This config is invalid: start_index(13) + entry_count(9) > 16
        gba::test.is_false(gba::text::valid_config(bad_cfg));

        // Missing required palbank_1 for a 2-plane profile.
        constexpr auto missing_bank = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_binary,
            .palbank_0 = 2,
            .start_index = 1,
        };
        gba::test.is_false(gba::text::valid_config(missing_bank));

        // Extra palbank_2 must be unset for a 2-plane profile.
        constexpr auto extra_bank = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_binary,
            .palbank_0 = 2,
            .palbank_1 = 3,
            .palbank_2 = 4,
            .start_index = 1,
        };
        gba::test.is_false(gba::text::valid_config(extra_bank));

        // 3-plane profile must have palbank_0..2 set.
        constexpr auto missing_plane2 = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::three_plane_binary,
            .palbank_0 = 2,
            .palbank_1 = 3,
            .start_index = 1,
        };
        gba::test.is_false(gba::text::valid_config(missing_plane2));

        constexpr auto full_color = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::one_plane_full_color,
            .palbank_0 = 4,
            .start_index = 0,
        };
        gba::test.eq(gba::text::required_entries(full_color.profile), 16u);
        gba::test.is_true(gba::text::valid_config(full_color));

        constexpr auto full_color_bad_start = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::one_plane_full_color,
            .palbank_0 = 4,
            .start_index = 1,
        };
        gba::test.is_false(gba::text::valid_config(full_color_bad_start));

        constexpr auto full_color_extra_bank = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::one_plane_full_color,
            .palbank_0 = 4,
            .palbank_1 = 5,
            .start_index = 0,
        };
        gba::test.is_false(gba::text::valid_config(full_color_extra_bank));
    }

    // Test bitplane encoding/decoding
    {
        constexpr auto cfg = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 1,
            .palbank_1 = 2,
            .start_index = 1,
        };

        // Test background nibble (all planes at background role)
        constexpr auto bg_nibble = cfg.background_nibble();
        gba::test.eq(bg_nibble, 1u); // start_index=1, roles=(0,0) -> 1 + 0*1 + 0*3 = 1

        // Test encoding: plane0=fg(1), plane1=bg(0)
        std::array<unsigned char, 3> roles = {1, 0, 0};
        constexpr auto fg_bg_nibble = 1 + 1 * 1 + 0 * 3; // = 2
        gba::test.eq(cfg.encode_roles(roles), static_cast<unsigned char>(fg_bg_nibble));

        // Test decoding: nibble 2 should decode to plane0=fg(1), plane1=bg(0)
        gba::test.eq(cfg.decode_role(2, 0), 1u); // plane 0 is foreground
        gba::test.eq(cfg.decode_role(2, 1), 0u); // plane 1 is background

        // Test encoding: plane0=bg(0), plane1=fg(1)
        roles = {0, 1, 0};
        constexpr auto bg_fg_nibble = 1 + 0 * 1 + 1 * 3; // = 4
        gba::test.eq(cfg.encode_roles(roles), static_cast<unsigned char>(bg_fg_nibble));

        // Test decoding: nibble 4 should decode to plane0=bg(0), plane1=fg(1)
        gba::test.eq(cfg.decode_role(4, 0), 0u); // plane 0 is background
        gba::test.eq(cfg.decode_role(4, 1), 1u); // plane 1 is foreground
    }

    // draw_cursor: inline color commands change subsequent one_plane_full_color glyph nibble and are not emitted.
    {
        using font_type = decltype(font);
        struct mock_layer {
            struct draw_call {
                unsigned int encoding{};
                unsigned char foreground_nibble{};
            };

            std::array<draw_call, 8> calls{};
            std::size_t count = 0;

            unsigned short draw_char(const font_type& f, unsigned int enc, int, int,
                                     unsigned char foreground_nibble = 1) {
                if (count < calls.size()) calls[count] = {.encoding = enc, .foreground_nibble = foreground_nibble};
                ++count;
                return f.glyph_or_default(enc).dwidth;
            }

            [[nodiscard]]
            constexpr bool uses_full_color() const noexcept {
                return true;
            }
        };

        mock_layer layer{};
        gba::text::stream_metrics streamMetrics{};
        auto s = gba::text::stream("A" "\x1B" "4" "A", font, streamMetrics);

        gba::text::draw_metrics drawMetrics{};
        gba::text::draw_cursor<mock_layer, decltype(font), decltype(s)> cursor{layer, font, s, 0, 0, drawMetrics};

        while (cursor.next()) {}

        gba::test.eq(layer.count, 2u);
        gba::test.eq(layer.calls[0].encoding, static_cast<unsigned int>('A'));
        gba::test.eq(layer.calls[0].foreground_nibble, 1u);
        gba::test.eq(layer.calls[1].encoding, static_cast<unsigned int>('A'));
        gba::test.eq(layer.calls[1].foreground_nibble, 4u);
        gba::test.eq(cursor.emitted(), 2u);
    }

    // bg4_text_layer: region origin (start_tile_x/y) confines screen-entry writes.
    {
        constexpr auto cfg = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_binary,
            .palbank_0 = 1,
            .palbank_1 = 2,
            .start_index = 1,
        };

        // Seed two screen entries with known values so we can prove which cells were touched.
        gba::mem_se[31][0] = {.tile_index = 123, .palette_index = 9};
        gba::mem_se[31][6 * 32 + 5] = {.tile_index = 124, .palette_index = 10};

        // 2x2 region starting at tile (5, 6).
        auto layer = gba::text::bg4_text_layer<2, 2>{
            31, cfg, gba::text::linear_tile_allocator{.next_tile = 1, .end_tile = 1024},
              5, 6,
        };

        // clear() should not touch outside-region screen entries.
        {
            const auto se0 = gba::mem_se[31][0].value();
            gba::test.eq(se0.tile_index, 123u);
            gba::test.eq(se0.palette_index, 9u);
        }
        // clear() should initialize the region origin tile entry.
        {
            const auto se = gba::mem_se[31][6 * 32 + 5].value();
            gba::test.eq(se.tile_index, 0u);
            gba::test.eq(se.palette_index, 1u);
        }

        // Drawing at local pixel (0,0) should allocate a tile and write to (5,6).
        layer.draw_char(font, 'A', 0, 1);
        {
            const auto se = gba::mem_se[31][6 * 32 + 5].value();
            gba::test.eq(se.tile_index, 1u);
            gba::test.eq(se.palette_index, 1u);
        }
    }

    // bg4_text_layer runtime path: one_plane_full_color inline ESC color changes and decorated-font shading.
    {
        using namespace gba::literals;

        constexpr auto cfg = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::one_plane_full_color,
            .palbank_0 = 1,
            .start_index = 0,
        };

        gba::text::set_theme(cfg, {
                                    .background = gba::color{},
                                    .foreground = "white"_clr,
                                    .shadow = "#102040"_clr,
                                });

        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 1024};
        auto layer = gba::text::bg4_text_layer<4, 2>{31, cfg, alloc};

        auto s = gba::text::stream("A" "\x1B" "4" "A", font);
        gba::text::draw_metrics metrics{};
        const auto emitted = layer.draw_stream(font, s, 0, 0, metrics);
        gba::test.eq(emitted, 2u);

        const auto se0 = gba::mem_se[31][1 * 32 + 0].value();
        const auto se1 = gba::mem_se[31][1 * 32 + 1].value();
        gba::test.eq(se0.palette_index, 1u);
        gba::test.eq(se1.palette_index, 1u);

        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        const auto& t0 = tiles[se0.tile_index];
        const auto& t1 = tiles[se1.tile_index];

        const auto nibble_at = [](const gba::tile4bpp& tile, unsigned int x, unsigned int y) {
            return static_cast<unsigned int>((tile[y] >> (x * 4)) & 0xFu);
        };

        const auto tile_has_nibble = [&](const gba::tile4bpp& tile, unsigned int nibble) {
            for (unsigned int y = 0; y < 8; ++y) {
                for (unsigned int x = 0; x < 8; ++x) {
                    if (nibble_at(tile, x, y) == nibble) return true;
                }
            }
            return false;
        };

         // First glyph uses default foreground nibble 1, second glyph uses nibble 4.
         gba::test.is_true(tile_has_nibble(t0, 1u));
         gba::test.is_true(tile_has_nibble(t1, 4u));

         // Pre-baked shadow variant keeps separate decoration/foreground masks so the
         // renderer still uses the shadow role palette for the decoration pass.
         auto layer_shadow = gba::text::bg4_text_layer<4, 4>{31, cfg, gba::text::linear_tile_allocator{.next_tile = 20, .end_tile = 1024}, 0, 0};
         layer_shadow.draw_char(text_test_font_shadowed, 'A', 4, 8, 4);

         const auto region_has_nibble = [&](unsigned short screenblock, unsigned short start_x, unsigned short start_y,
                                            unsigned int nibble) {
             for (unsigned int cell_y = 0; cell_y < 4; ++cell_y) {
                 for (unsigned int cell_x = 0; cell_x < 4; ++cell_x) {
                     const auto se = gba::mem_se[screenblock][(start_y + cell_y) * 32 + (start_x + cell_x)].value();
                     if (se.tile_index == 0u) continue;
                     const auto& tile = tiles[se.tile_index];
                     if (tile_has_nibble(tile, nibble)) return true;
                 }
             }
             return false;
         };

         gba::test.is_true(region_has_nibble(31u, 0u, 0u, 4u)); // foreground nibble
         gba::test.is_true(text_test_font_shadowed.glyph_or_default('A').decoration_width > 0);
         gba::test.is_true(text_test_font_shadowed.glyph_or_default('A').decoration_height > 0);

         const auto& outline_glyph = text_test_font_outlined.glyph_or_default('A');
         const auto& plain_glyph = font.glyph_or_default('A');
         gba::test.eq(outline_glyph.width, static_cast<unsigned short>(plain_glyph.width + 2));
         gba::test.eq(outline_glyph.height, static_cast<unsigned short>(plain_glyph.height + 2));
         gba::test.is_true(text_test_font_outlined.decoration_bitmap_data(outline_glyph)[0] != 0u);

         const auto& shadow_glyph = text_test_font_shadowed.glyph_or_default('A');
         const auto shadow_bitmap_bytes = static_cast<std::size_t>(shadow_glyph.bitmap_byte_width) * shadow_glyph.height;
         const auto* shadow_fg = text_test_font_shadowed.bitmap_data(shadow_glyph);
         const auto* shadow_deco = text_test_font_shadowed.decoration_bitmap_data(shadow_glyph);
         for (std::size_t i = 0; i < shadow_bitmap_bytes; ++i) {
             gba::test.eq(static_cast<unsigned int>(shadow_fg[i] & shadow_deco[i]), 0u);
         }

         const auto outline_bitmap_bytes = static_cast<std::size_t>(outline_glyph.bitmap_byte_width) * outline_glyph.height;
         const auto* outline_fg = text_test_font_outlined.bitmap_data(outline_glyph);
         const auto* outline_deco = text_test_font_outlined.decoration_bitmap_data(outline_glyph);
         for (std::size_t i = 0; i < outline_bitmap_bytes; ++i) {
             gba::test.eq(static_cast<unsigned int>(outline_fg[i] & outline_deco[i]), 0u);
         }
    }

    return gba::test.finish();
}
