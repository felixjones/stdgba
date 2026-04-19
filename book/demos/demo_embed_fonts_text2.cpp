/// @file demo_embed_fonts_text2.cpp
/// @brief text2 engine version of embedded BDF font demo.
/// Demonstrates plain and shadow-decorated fonts in multi-plane profiles.

#include <gba/bios>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/text>
#include <gba/text2>

#include <array>

int main() {
    using namespace gba::literals;

    static constexpr auto base_font_ui = gba::embed::bdf([] {
        return std::to_array<unsigned char>({
#embed "6x13B.bdf"
        });
    });

    static constexpr auto base_font_haxor = gba::embed::bdf([] {
        return std::to_array<unsigned char>({
#embed "HaxorMedium-12.bdf"
        });
    });

    static constexpr auto font_ui = gba::text::with_shadow<1, 1>(base_font_ui);
    static constexpr auto font_haxor = gba::text::with_shadow<1, 1>(base_font_haxor);

    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {.video_mode = 0, .enable_bg0 = true};
    gba::reg_bgcnt[0] = {.screenblock = 31};

    constexpr auto config = gba::text2::bitplane_config{
        .profile = gba::text2::bitplane_profile::two_plane_three_color,
        .palbank_0 = 1,
        .palbank_1 = 2,
        .start_index = 1,
    };

    constexpr auto theme = gba::text2::bitplane_theme{
        .background = "#1A2238"_clr,
        .foreground = "#F6F7FB"_clr,
        .shadow = "#0A1020"_clr,
    };

    gba::text2::set_theme(config, theme);
    gba::pal_bg_mem[0] = theme.background;

    gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
    using layer_type = gba::text2::bg4bpp_text_layer<240, 160>;
    static layer_type::cell_state_map cell_state{};
    layer_type layer{31, config, alloc, cell_state};

    // Stream metrics for layout
    gba::text2::stream_metrics title_metrics{
        .letter_spacing_px = 0,
        .line_spacing_px = 0,
        .tab_width_px = 32,
        .wrap_width_px = 224,
    };
    gba::text2::stream_metrics body_metrics{
        .letter_spacing_px = 1,
        .line_spacing_px = 1,
        .tab_width_px = 32,
        .wrap_width_px = 224,
    };

    layer.draw_stream(font_haxor, "Embedded BDF fonts", 4, 8, title_metrics);

    layer.draw_stream(font_haxor, "HaxorMedium-12: ABC abc 0123", 4, 34, body_metrics);

    layer.draw_stream(font_ui, "6x13B: GBA text layer sample", 4, 64, body_metrics);

    layer.draw_stream(font_ui, "glyph_or_default + BitUnPack-ready rows", 4, 84, body_metrics);

    layer.flush_cache();

    while (true) {
        gba::VBlankIntrWait();
    }
}
