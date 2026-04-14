/// @file demo_text_render.cpp
/// @brief Animated formatted BG text rendering demo using embedded BDF glyphs.

#include <gba/bios>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/text>
#include <gba/video>

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

    // Configure bitplane rendering with two planes using palette banks 1 and 2
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
    gba::text::bg4_text_layer layer{31, config, alloc};

    gba::text::draw_metrics drawMetrics{
        .letter_spacing_px = 1,
        .line_spacing_px = 2,
        .wrap_width_px = 220,
        .break_chars = gba::text::break_policy::whitespace,
    };
    gba::text::stream_metrics streamMetrics{.letter_spacing_px = 1};

    auto make_cursor = [&] {
        auto gen = fmt.generator("value"_arg = [&] { return frame; });
        auto s = gba::text::stream(gen, font, streamMetrics);
        return layer.make_cursor(font, s, 0, 0, drawMetrics);
    };

    auto cursor = make_cursor();

    while (true) {
        gba::VBlankIntrWait();
        ++frame;

        if (!cursor.next_visible()) {
            // Stream exhausted -- wait, then restart with updated HP.
            if ((frame % 120) == 0) {
                alloc = {.next_tile = 1, .end_tile = 512};
                layer = gba::text::bg4_text_layer{31, config, alloc};
                cursor = make_cursor();
            }
        }
    }
}
