/// @file tests/text2/test_text2_render.cpp
/// @brief Rendering engine tests for text2.

#include <gba/embed>
#include <gba/testing>
#include <gba/text2>

using namespace gba::literals;

int main() {
    // Test: Layer instantiation
    {
        auto config = gba::text2::bitplane_config{
            .profile = gba::text2::bitplane_profile::two_plane_three_color,
            .palbank_0 = 1,
            .palbank_1 = 2,
            .start_index = 1,
        };
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
        gba::test.eq(layer.palette(), 1u);
    }

    // Test: Palette setting
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        layer.set_palette(5);
        gba::test.eq(layer.palette(), 5u);
    }

    // Test: Palette bounds (invalid palette should clamp)
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        layer.set_palette(5);
        layer.set_palette(20);
        gba::test.eq(layer.palette(), 5u);
    }

    // Test: Reset clears state
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        layer.set_palette(7);
        layer.reset();
        gba::test.eq(layer.palette(), 1u);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
    }

    // Test: Max tile count calculation (240x160 = 30×20 tiles)
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        gba::test.eq(layer.max_tile_count(), 600u);
    }

    // Test: Different layer dimensions
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 120, 80> layer(nullptr, config);
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
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
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
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        auto idx1 = layer.tile_index_from_row_col(2, 5);
        auto idx2 = layer.tile_index_from_row_col(2, 6);
        gba::test.eq(idx2 - idx1, 1u);
    }

    // Test: Current tile count tracking
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        gba::test.eq(layer.current_tile_count(), 0u);
        gba::test.eq(layer.max_tile_count(), 600u); // 30 × 20 tiles
    }

    // Test: Reset clears tile count
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        layer.reset();
        gba::test.eq(layer.current_tile_count(), 0u);
        gba::test.eq(layer.cursor_column(), 0u);
        gba::test.eq(layer.cursor_row(), 0u);
    }

    // Test: Color escape with palette clamp
    {
        auto config = gba::text2::bitplane_config{};
        gba::text2::bg4bpp_text_layer<gba::embed::bdf_font_result<256, 1024>, 240, 160> layer(nullptr, config);
        layer.set_palette(5);
        gba::test.eq(layer.palette(), 5u);
        layer.set_palette(20); // Out of range
        gba::test.eq(layer.palette(), 5u); // Should retain previous
    }

    return gba::test.finish();
}
