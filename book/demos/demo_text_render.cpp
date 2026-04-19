/// @file demo_text_render.cpp
/// @brief Animated formatted BG text rendering demo using embedded BDF glyphs.

#include <gba/bios>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/text>

#include <array>

int main() {
    using namespace gba::literals;

    static constexpr auto font = gba::text::with_shadow<1, 1>(gba::embed::bdf([] {
        return std::to_array<unsigned char>({
#embed "9x18.bdf"
        });
    }));
    static constexpr auto fmt = "The frame is: {value}"_fmt;

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
                                      .background = "#304060"_clr,
                                      .foreground = "white"_clr,
                                      .shadow = "#102040"_clr,
                                  });
    gba::pal_bg_mem[0] = "#304060"_clr;

    unsigned int frame = 0;

    gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
    using layer_type = gba::text::bg4bpp_text_layer<240, 160>;
    static layer_type::cell_state_map cell_state{};
    layer_type layer{31, config, alloc, cell_state};

    gba::text::stream_metrics metrics{
        .letter_spacing_px = 1,
        .line_spacing_px = 2,
        .tab_width_px = 32,
        .wrap_width_px = 220,
    };

    auto make_cursor = [&] {
        auto gen = fmt.generator("value"_arg = [&] { return frame; });
        auto s = gba::text::stream(gen, font, metrics);
        return layer.make_cursor(font, s, 0, 0, metrics);
    };

    auto cursor = make_cursor();

    while (true) {
        gba::VBlankIntrWait();
        ++frame;

        if (!cursor.next_visible() && frame % 120 == 0) {
            alloc = {.next_tile = 1, .end_tile = 512};
            layer = layer_type{31, config, alloc, cell_state};
            cursor = make_cursor();
        }
    }
}
