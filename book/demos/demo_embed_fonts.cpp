/// @file demo_embed_fonts.cpp
/// @brief Embedded BDF font demo using multiple compile-time fonts.

#include <gba/bios>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/text>

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

    constexpr auto config = gba::text::bitplane_config{
        .profile = gba::text::bitplane_profile::two_plane_three_color,
        .palbank_0 = 1,
        .palbank_1 = 2,
        .start_index = 1,
    };

    gba::text::set_theme(config, {
                                     .background = "#1A2238"_clr,
                                     .foreground = "#F6F7FB"_clr,
                                     .shadow = "#0A1020"_clr,
                                 });
    gba::pal_bg_mem[0] = "#1A2238"_clr;

    gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
    gba::text::bg4_text_layer layer{31, config, alloc};

    gba::text::draw_metrics draw_title{
        .letter_spacing_px = 0,
        .line_spacing_px = 0,
        .wrap_width_px = 224,
        .break_chars = gba::text::break_policy::whitespace,
    };
    gba::text::draw_metrics draw_body{
        .letter_spacing_px = 1,
        .line_spacing_px = 1,
        .wrap_width_px = 224,
        .break_chars = gba::text::break_policy::whitespace,
    };

    gba::text::stream_metrics stream_title{.letter_spacing_px = 0};
    gba::text::stream_metrics stream_body{.letter_spacing_px = 1};

    auto line_1 = gba::text::stream("Embedded BDF fonts", font_haxor, stream_title);
    layer.draw_stream(font_haxor, line_1, 4, 8, draw_title);

    auto line_2 = gba::text::stream("HaxorMedium-12: ABC abc 0123", font_haxor, stream_body);
    layer.draw_stream(font_haxor, line_2, 4, 34, draw_body);

    auto line_3 = gba::text::stream("6x13B: GBA text layer sample", font_ui, stream_body);
    layer.draw_stream(font_ui, line_3, 4, 64, draw_body);

    auto line_4 = gba::text::stream("glyph_or_default + BitUnPack-ready rows", font_ui, stream_body);
    layer.draw_stream(font_ui, line_4, 4, 84, draw_body);

    while (true) {
        gba::VBlankIntrWait();
    }
}
